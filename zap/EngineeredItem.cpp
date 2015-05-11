//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "EngineeredItem.h"

#include "gameWeapons.h"
#include "GameObjectRender.h"
#include "WallItem.h"
#include "Teleporter.h"
#include "gameType.h"
#include "Intervals.h"
#include "Level.h"
#include "PolyWall.h"
#include "projectile.h"
#include "GeomUtils.h"
#include "clipper.hpp"

#include "ServerGame.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"        // for accessing client's spark manager
#  include "UIQuickMenu.h"
#endif

#include "Colors.h"
#include "stringUtils.h"
#include "MathUtils.h"           // For findLowestRootIninterval()
#include "GeomUtils.h"

namespace Zap
{

static bool forceFieldEdgesIntersectPoints(const Vector<Point> *points, const Vector<Point> forceField)
{
   return polygonIntersectsSegment(*points, forceField[0], forceField[1]) || polygonIntersectsSegment(*points, forceField[2], forceField[3]);
}


// Constructor
Engineerable::Engineerable()
{
   mEngineered = false;
}

// Destructor
Engineerable::~Engineerable()
{
   // Do nothing
}


void Engineerable::setEngineered(bool isEngineered)
{
   mEngineered = isEngineered;
}


bool Engineerable::isEngineered()
{
   // If the engineered item has a resource attached, then it was engineered by a player
   return mEngineered;
}


void Engineerable::setResource(MountableItem *resource)
{
   mResource = resource;
   mResource->removeFromDatabase(false);     // Don't want to delete this item -- we'll need it later in releaseResource()

   TNLAssert(dynamic_cast<ServerGame*>(mResource->getGame()), "Null ServerGame");
   static_cast<ServerGame*>(mResource->getGame())->onObjectRemoved(mResource);
}


void Engineerable::releaseResource(const Point &releasePos, Level *level)
{
   if(!mResource) 
      return;

   mResource->addToDatabase(level);
   mResource->setPosVelAng(releasePos, Point(), 0);               // Reset velocity of resource item to 0,0

   TNLAssert(dynamic_cast<ServerGame*>(mResource->getGame()), "NULL ServerGame");
   static_cast<ServerGame*>(mResource->getGame())->onObjectAdded(mResource);
}


////////////////////////////////////////
////////////////////////////////////////

// Returns true if deploy point is valid, false otherwise.  deployPosition and deployNormal are populated if successful.
bool EngineerModuleDeployer::findDeployPoint(const Ship *ship, U32 objectType, Point &deployPosition, Point &deployNormal)
{
   if(objectType == EngineeredTurret || objectType == EngineeredForceField)
   {
         // Ship must be within Ship::MaxEngineerDistance of a wall, pointing at where the object should be placed
         Point startPoint = ship->getActualPos();
         Point endPoint = startPoint + ship->getAimVector() * Ship::MaxEngineerDistance;

         F32 collisionTime;

         // Computes collisionTime and deployNormal -- deployNormal will have been normalized to length of 1
         BfObject *hitObject = ship->findObjectLOS((TestFunc)isWallType, ActualState, startPoint, endPoint,
                                                     collisionTime, deployNormal);

         if(!hitObject)    // No appropriate walls found, can't deploy, sorry!
            return false;


         if(deployNormal.dot(ship->getAimVector()) > 0)
            deployNormal = -deployNormal;      // This is to fix deploy at wrong side of barrier.

         // Set deploy point, and move one unit away from the wall (this is a tiny amount, keeps linework from overlapping with wall)
         deployPosition.set(startPoint + (endPoint - startPoint) * collisionTime + deployNormal);
   }
   else if(objectType == EngineeredTeleporterEntrance || objectType == EngineeredTeleporterExit)
      deployPosition.set(ship->getActualPos() + (ship->getAimVector() * (Ship::CollisionRadius + Teleporter::TELEPORTER_RADIUS)));

   return true;
}


// Check for sufficient energy and resources; return empty string if everything is ok
string EngineerModuleDeployer::checkResourcesAndEnergy(const Ship *ship)
{
   if(!ship->isCarryingItem(ResourceItemTypeNumber))
      return "!!! Need resource item to use Engineer module";

   if(ship->getEnergy() < ModuleInfo::getModuleInfo(ModuleEngineer)->getPrimaryPerUseCost())
      return "!!! Not enough energy to engineer an object";

   return "";
}


// Returns "" if location is OK, otherwise returns an error message
// Runs on client and server
bool EngineerModuleDeployer::canCreateObjectAtLocation(const Level *level, const Ship *ship, U32 objectType)
{
   string msg;

   // Everything needs energy and a resource, except the teleport exit
   if(objectType != EngineeredTeleporterExit)
      mErrorMessage = checkResourcesAndEnergy(ship);

   if(mErrorMessage != "")
      return false;

   if(!findDeployPoint(ship, objectType, mDeployPosition, mDeployNormal))     // Computes mDeployPosition and mDeployNormal
   {
      mErrorMessage = "!!! Could not find a suitable wall for mounting the item";
      return false;
   }

   Vector<Point> bounds;
   bool goodDeploymentPosition = false;

   // Seems inefficient to construct these just for the purpose of bounds checking...
   switch(objectType)
   {
      case EngineeredTurret:
         bounds = Turret::getTurretGeometry(mDeployPosition, mDeployNormal);   
         goodDeploymentPosition = EngineeredItem::checkDeploymentPosition(bounds, level);
         break;
      case EngineeredForceField:
         bounds = ForceFieldProjector::getForceFieldProjectorGeometry(mDeployPosition, mDeployNormal);
         goodDeploymentPosition = EngineeredItem::checkDeploymentPosition(bounds, level);
         break;
      case EngineeredTeleporterEntrance:
      case EngineeredTeleporterExit:
         goodDeploymentPosition = Teleporter::checkDeploymentPosition(mDeployPosition, level, ship);
         break;
      default:    // will never happen
         TNLAssert(false, "Bad objectType");
         return false;
   }

   if(!goodDeploymentPosition)
   {
      mErrorMessage = "!!! Cannot deploy item at this location";
      return false;
   }

   // If this is anything but a forcefield, then we're good to go!
   if(objectType != EngineeredForceField)
      return true;


   // Forcefields only from here on down; we've got miles to go before we sleep

   /// Part ONE
   // We need to ensure forcefield doesn't cross another; doing so can create an impossible situation
   // Forcefield starts at the end of the projector.  Need to know where that is.
   Point forceFieldStart = ForceFieldProjector::getForceFieldStartPoint(mDeployPosition, mDeployNormal, 0);

   // Now we can find the point where the forcefield would end if this were a valid position
   Point forceFieldEnd;
   DatabaseObject *terminatingWallObject = 
               ForceField::findForceFieldEnd(level, forceFieldStart, mDeployNormal, forceFieldEnd);

   bool collision = false;

   // Check for collisions with existing projectors
   Rect queryRect(forceFieldStart, forceFieldEnd);
   queryRect.expand(Point(5,5));    // Just a touch bigger than the bare minimum

   Vector<Point> candidateForceFieldGeom = ForceField::computeGeom(forceFieldStart, forceFieldEnd);

   fillVector.clear();
   level->findObjects(ForceFieldProjectorTypeNumber, fillVector, queryRect);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      ForceFieldProjector *ffp = static_cast<ForceFieldProjector *>(fillVector[i]);

      if(forceFieldEdgesIntersectPoints(ffp->getCollisionPoly(), candidateForceFieldGeom))
      {
         collision = true;
         break;
      }
   }
   
   if(!collision)
   {
      // Check for collision with forcefields that could be projected from those projectors.
      // Projectors up to two forcefield lengths away must be considered because the end of 
      // one could intersect the end of the other.
      fillVector.clear();
      queryRect.expand(Point(ForceField::MAX_FORCEFIELD_LENGTH, ForceField::MAX_FORCEFIELD_LENGTH));
      level->findObjects(ForceFieldProjectorTypeNumber, fillVector, queryRect);

      // Reusable containers for holding geom of any forcefields we might need to check for intersection with our candidate
      Point start, end;

      for(S32 i = 0; i < fillVector.size(); i++)
      {
         ForceFieldProjector *proj = static_cast<ForceFieldProjector *>(fillVector[i]);

         proj->getForceFieldStartAndEndPoints(start, end);

         if(forceFieldEdgesIntersectPoints(&candidateForceFieldGeom, ForceField::computeGeom(start, end)))
         {
            collision = true;
            break;
         }
      }
   }

   if(collision)
   {
      mErrorMessage = "!!! Cannot deploy forcefield where it could cross another.";
      return false;
   }


   /// Part TWO - preventative abuse measures

   // First thing first, is abusive engineer allowed?  If so, let's get out of here.
   if(ship->getGame()->getGameType()->isEngineerUnrestrictedEnabled())
      return true;

   // Continuing on...  let's check to make sure that forcefield doesn't come within a ship's
   // width of a wall; this should really squelch the forcefield abuse
   bool wallTooClose = false;
   fillVector.clear();

   // Build collision poly from forcefield and ship's width
   // Similar to expanding a barrier spine
   Vector<Point> collisionPoly;
   Point dir = forceFieldEnd - forceFieldStart;

   Point crossVec(dir.y, -dir.x);
   crossVec.normalize(2 * Ship::CollisionRadius + ForceField::ForceFieldHalfWidth);

   collisionPoly.push_back(forceFieldStart + crossVec);
   collisionPoly.push_back(forceFieldEnd + crossVec);
   collisionPoly.push_back(forceFieldEnd - crossVec);
   collisionPoly.push_back(forceFieldStart - crossVec);

   // Reset query rect
   queryRect = Rect(collisionPoly);

   // Search for wall segments within query
   level->findObjects(isWallType, fillVector, queryRect);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      // Exclude the end segment from our search
      if(terminatingWallObject && terminatingWallObject == fillVector[i])
         continue;

      if(polygonsIntersect(*fillVector[i]->getCollisionPoly(), collisionPoly))
      {
         wallTooClose = true;
         break;
      }
   }

   if(wallTooClose)
   {
      mErrorMessage = "!!! Cannot deploy forcefield where it will pass too close to a wall";
      return false;
   }


   /// Part THREE
   // Now we should check for any turrets that may be in the way using the same geometry as in
   // part two.  We can excluded engineered turrets because they can be destroyed
   bool turretInTheWay = false;
   fillVector.clear();
   level->findObjects(TurretTypeNumber, fillVector, queryRect);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Turret *turret = static_cast<Turret *>(fillVector[i]);

      // We don't care about engineered turrets because they can be destroyed
      if(turret->isEngineered())
         continue;

      if(polygonsIntersect(*turret->getCollisionPoly(), collisionPoly))
      {
         turretInTheWay = true;
         break;
      }
   }

   if(turretInTheWay)
   {
      mErrorMessage = "!!! Cannot deploy forcefield over a non-destructible turret";
      return false;
   }

   return true;     // We've run the gammut -- this location is OK
}


// Runs on server
// Only run after canCreateObjectAtLocation, which checks for errors and sets mDeployPosition
// Return true if everything went well, false otherwise.  Caller will manage energy credits and debits.
bool EngineerModuleDeployer::deployEngineeredItem(ClientInfo *clientInfo, U32 objectType)
{
   // Do some basic crash-proofing sanity checks first
   Ship *ship = clientInfo->getShip();
   if(!ship)
      return false;

   BfObject *deployedObject = NULL;

   // Create the new engineered item here
   // These will be deleted when destroyed using deleteObject(); or, if not destroyed by end
   // of game, Game::cleanUp() will get rid of them
   switch(objectType)
   {
      case EngineeredTurret:
         deployedObject = new Turret(ship->getTeam(), mDeployPosition, mDeployNormal);    // Deploy pos/norm calc'ed in canCreateObjectAtLocation()
         break;

      case EngineeredForceField:
         deployedObject = new ForceFieldProjector(ship->getTeam(), mDeployPosition, mDeployNormal);
         break;

      case EngineeredTeleporterEntrance:
         deployedObject = new Teleporter(mDeployPosition, mDeployPosition, ship);
         ship->setEngineeredTeleporter(static_cast<Teleporter*>(deployedObject));

         clientInfo->sDisableShipSystems(true);
         clientInfo->setEngineeringTeleporter(true);
         break;

      case EngineeredTeleporterExit:
         if(ship->getEngineeredTeleporter() && !ship->getEngineeredTeleporter()->hasAnyDests())
         {
            // Set the telport endpoint
            ship->getEngineeredTeleporter()->setEndpoint(mDeployPosition);

            // Clean-up
            clientInfo->sTeleporterCleanup();
         }
         else   // Something went wrong
            return false;

         return true;
         break;

      default:
         return false;
   }
   
   Engineerable *engineerable = dynamic_cast<Engineerable *>(deployedObject);

   if((!deployedObject || !engineerable) && !clientInfo->isRobot())  // Something went wrong
   {
      GameConnection *conn = clientInfo->getConnection();
      conn->s2cDisplayErrorMessage("Error deploying object.");
      delete deployedObject;
      return false;
   }

   // It worked!  Object depolyed!
   deployedObject->updateExtentInDatabase();

   deployedObject->setOwner(clientInfo);
   deployedObject->addToGame(ship->getGame(), ship->getGame()->getLevel());

   MountableItem *resource = ship->dismountFirst(ResourceItemTypeNumber);
   ship->resetFastRecharge();

   engineerable->setResource(resource);
   engineerable->onConstructed();
   engineerable->setEngineered(true);

   return true;
}


string EngineerModuleDeployer::getErrorMessage()
{
   return mErrorMessage;
}


////////////////////////////////////////
////////////////////////////////////////

const F32 EngineeredItem::EngineeredItemRadius = 7.f;
const F32 EngineeredItem::DamageReductionFactor = 0.25f;

// Constructor
EngineeredItem::EngineeredItem(S32 team, const Point &anchorPoint, const Point &anchorNormal) : 
      Parent(EngineeredItemRadius), 
      Engineerable(), 
      mAnchorNormal(anchorNormal)
{
   mHealth = 1.0f;
   setTeam(team);
   mOriginalTeam = team;
   mIsDestroyed = false;
   mHealRate = 0;
   mMountSeg = NULL;
   mSnapped = false;

   Parent::setPos(anchorPoint);  // Must be parent, or else... TNLAssert!!!
}


// Destructor
EngineeredItem::~EngineeredItem()
{
   // Do nothing
}


// XXXX <Team> <X> <Y> [HealRate]
bool EngineeredItem::processArguments(S32 argc, const char **argv, Level *level)
{
   if(argc < 3)
      return false;

   setTeam(atoi(argv[0]));
   mOriginalTeam = getTeam();

   if(mOriginalTeam == TEAM_NEUTRAL)      // Neutral object starts with no health and can be repaired and claimed by anyone
      mHealth = 0;
   
   Point pos;
   pos.read(argv + 1);
   pos *= level->getLegacyGridSize();

   if(argc >= 4)
      setHealRate(atoi(argv[3]));

   findMountPoint(level, pos);

   return true;
}


void EngineeredItem::computeObjectGeometry()
{
   mCollisionPolyPoints = getObjectGeometry(getPos(), mAnchorNormal);
}


F32 EngineeredItem::getSelectionOffsetMagnitude()
{
   TNLAssert(false, "Not implemented");
   return 0;
}


void EngineeredItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

   computeObjectGeometry();

   if(mHealth != 0)
      onEnabled();
}


string EngineeredItem::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + itos(getTeam()) + " " + geomToLevelCode() + " " + itos(mHealRate);
}


void EngineeredItem::onGeomChanged()
{
   mCollisionPolyPoints = getObjectGeometry(getPos(), mAnchorNormal);     // Recompute collision poly
   Parent::onGeomChanged();
}


#ifndef ZAP_DEDICATED
Point EngineeredItem::getEditorSelectionOffset(F32 currentScale)
{
   if(!mSnapped)
      return Parent::getEditorSelectionOffset(currentScale);

   F32 m = getSelectionOffsetMagnitude();

   Point cross(mAnchorNormal.y, -mAnchorNormal.x);
   F32 ang = cross.ATAN2();

   F32 x = -m * sin(ang);
   F32 y =  m * cos(ang);

   return Point(x,y);
}

#endif


// Render some attributes when item is selected but not being edited
void EngineeredItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("10% Heal");   values.push_back((mHealRate == 0 ? "Disabled" : itos(mHealRate) + " sec" + (mHealRate != 1 ? "s" : "")));
}


// Database could be either a database full of WallEdges or game objects
static DatabaseObject *findClosestWall(const GridDatabase *database, const Point &pos, F32 snapDist, 
                                       bool format,
                                       Point &anchor, Point &normal)
{
   DatabaseObject *closestWall = NULL;
   F32 minDist = F32_MAX;

   Point n, dir, mountPos;    // Reused in loop below
   F32 t;

   // Start with a sweep of the area.
   //
   // The smaller the increment, the closer to finding an accurate line perpendicular to the wall; however
   // we will trade accuracy for performance here and follow up with finding the exact normal and anchor
   // below this loop.
   //
   // Start at any angle other than 0.  Search at angle 0 seems to return the wrong wall sometimes.
   F32 increment = Float2Pi * 0.0625f;

   for(F32 theta = increment; theta < Float2Pi + increment; theta += increment)
   {
      dir.set(cos(theta) * snapDist, sin(theta) * snapDist);
      mountPos.set(pos - dir * 0.001f);   // Offsetting slightly prevents spazzy behavior in editor

      // Look for walls
      DatabaseObject *wall = database->findObjectLOS(isWallType, ActualState, format, mountPos, mountPos + dir, t, n);

      if(wall == NULL)     // No wall in this direction
         continue;

      if(t >= minDist)     // Wall in this direction, but not as close as other candidates
         continue;

      // Skip candidate if it's selected -- when would we want to snap to a selected object?
      //if(wall->getObjectTypeNumber() != WallEdgeTypeNumber)    // WallEdges are NOT BfObjects!
      //   if(static_cast<BfObject *>(wall)->isSelected())
      //      continue;

      // If we get to here, the wall we've found is our best candidate yet!
      anchor.set(mountPos + dir * t);
      normal.set(n);
      minDist = t;
      closestWall = wall;
   }

   return closestWall;
}


// Static function -- returns segment item is mounted on; returns NULL if item is not mounted; populates anchor and normal
BfObject *EngineeredItem::findAnchorPointAndNormal(const GridDatabase *gameObjectDatabase, 
                                                   const GridDatabase *wallEdgeDatabase,
                                                   const Point &pos, 
                                                   F32 snapDist, 
                                                   bool format, Point &anchor, Point &normal)
{
   // Here we're interested in finding the closest wall edge to our item -- since edges are anonymous (i.e.
   // we don't know which wall they belong to), we don't really care which edge it is, only where the item
   // will snap to.  We'll use this snap location to identify the actual wall segment later.
   DatabaseObject *edge = findClosestWall(wallEdgeDatabase, pos, snapDist, format, anchor, normal);

   if(!edge)
      return NULL;

   // Re-adjust our anchor to a segment built from the anchor and normal vector found above.
   // This is because the anchor may be slightly off due to the inaccurate sweep angles.
   //
   // The algorithm here is to concoct a small segment through the anchor detected in the sweep, and
   // make it perpendicular to the normal vector that was also detected in the sweep (so parallel to
   // the wall edge).  Then find the new normal point to this segment and make that the anchor.
   //
   // 10 point length parallel segment should be plenty.
   Point normalNormal(normal.y, -normal.x);
   Point p1 = Point(anchor.x + (5 * normalNormal.x), anchor.y + (5 * normalNormal.y));
   Point p2 = Point(anchor.x - (5 * normalNormal.x), anchor.y - (5 * normalNormal.y));

   // Now find our new anchor
   findNormalPoint(pos, p1, p2, anchor);

   // Finally, figure out which segment this item is mounted on by re-running our find algorithm, but using our segment
   // database rather than our wall-edge database.  We'll pass the anchor location we found above as the snap object's
   // position, and use a dummy point to avoid clobbering the anchor location we found.
   Point dummy;

   BfObject *closestWall = static_cast<BfObject *>(
         findClosestWall(gameObjectDatabase, anchor, snapDist, format, dummy, normal));

   TNLAssert(closestWall, "Should have found something here -- we already found an edge, there should be a wall there too!");

   // If closestWall is a polywall, and if it is wound CW, need to reverse the normal point
   if(closestWall->getObjectTypeNumber() == PolyWallTypeNumber)
   { 
      PolyWall *polywall = static_cast<PolyWall *>(closestWall);

      if(isWoundClockwise(polywall->getCollisionPoly()))
         normal *= -1;
   }

   return closestWall;
}


BfObject *EngineeredItem::getMountSegment() const
{
   return mMountSeg;
}


void EngineeredItem::setMountSegment(BfObject *mountSeg)
{
   mMountSeg = mountSeg;
}


// setSnapped() / isSnapped() only called from editor
void EngineeredItem::setSnapped(bool snapped)
{
   mSnapped = snapped;
}


bool EngineeredItem::isSnapped() const
{
   return mSnapped;
}


static const F32 DisabledLevel = 0.25;

bool EngineeredItem::isEnabled() const
{
   return mHealth >= DisabledLevel;
}


void EngineeredItem::damageObject(DamageInfo *di)
{
   // Don't do self damage.  This is more complicated than it should probably be.
   BfObject *damagingObject = di->damagingObject;

   U8 damagingObjectType = UnknownTypeNumber;
   if(damagingObject != NULL)
      damagingObjectType = damagingObject->getObjectTypeNumber();

   if(isProjectileType(damagingObjectType))
   {
      BfObject *shooter = WeaponInfo::getWeaponShooterFromObject(damagingObject);

      // We have a shooter that is another engineered object (turret)
      if(shooter != NULL && isEngineeredType(shooter->getObjectTypeNumber()))
      {
         EngineeredItem* engShooter = static_cast<EngineeredItem*>(shooter);

         // Don't do self damage or damage to a team-turret
         if(engShooter == this || engShooter->getTeam() == this->getTeam())
            return;
      }
   }

   F32 prevHealth = mHealth;

   if(di->damageAmount > 0)
      setHealth(mHealth - di->damageAmount * DamageReductionFactor);
   else
      setHealth(mHealth - di->damageAmount);

   mHealTimer.reset();     // Restart healing timer...

   setMaskBits(HealthMask);

   // Check if turret just died
   if(prevHealth >= DisabledLevel && mHealth < DisabledLevel)        // Turret just died
   {
      // Revert team to neutral if this was a repaired turret
      if(getTeam() != mOriginalTeam)
      {
         setTeam(mOriginalTeam);
         setMaskBits(TeamMask);
      }
      onDisabled();

      // Handle scoring
      if(damagingObject && damagingObject->getOwner())
      {
         ClientInfo *player = damagingObject->getOwner();

         if(mObjectTypeNumber == TurretTypeNumber)
         {
            GameType *gt = getGame()->getGameType();

            if(gt->isTeamGame() && player->getTeamIndex() == getTeam())
               gt->updateScore(player, KillOwnTurret);
            else
               gt->updateScore(player, KillEnemyTurret);

            player->getStatistics()->mTurretsKilled++;
         }
         else if(mObjectTypeNumber == ForceFieldProjectorTypeNumber)
            player->getStatistics()->mFFsKilled++;
      }
   }
   else if(prevHealth < DisabledLevel && mHealth >= DisabledLevel)   // Turret was just repaired or healed
   {
      if(getTeam() == TEAM_NEUTRAL)                   // Neutral objects...
      {
         if(damagingObject)
         {
            setTeam(damagingObject->getTeam());   // ...join the team of their repairer
            setMaskBits(TeamMask);                    // Broadcast new team status
         }
      }
      onEnabled();
   }

   if(mHealth == 0 && mEngineered)
   {
      mIsDestroyed = true;
      onDestroyed();

      if(mResource.isValid())
         releaseResource(getPos() + mAnchorNormal * mResource->getRadius(), getGame()->getLevel());

      deleteObject(HALF_SECOND);
   }
}


bool EngineeredItem::collide(BfObject *hitObject)
{
   return true;
}


void EngineeredItem::setHealth(F32 health)
{
   mHealth = CLAMP(health, 0, 1);
}


F32 EngineeredItem::getHealth() const
{
   return mHealth;
}


Rect EngineeredItem::calcExtents() const
{
   const Vector<Point> *p = getCollisionPoly();
   return Rect(*p);
}


void EngineeredItem::onConstructed()
{
   onEnabled();      // Does something useful with ForceFieldProjectors!
}


void EngineeredItem::onDestroyed()
{
   // Do nothing
}


void EngineeredItem::onDisabled()
{
   // Do nothing
}


void EngineeredItem::onEnabled()
{
   // Do nothing
}


Vector<Point> EngineeredItem::getObjectGeometry(const Point &anchor, const Point &normal) const
{
   TNLAssert(false, "function not implemented!");

   Vector<Point> dummy;
   return dummy;
}


void EngineeredItem::setPos(lua_State *L, S32 stackIndex)
{
   Parent::setPos(L, stackIndex);

   // Find a database that contains objects we could snap to; if we don't have one, it's no snapping today

   if(!getGame())
      return;

   Level *level = getGame()->getLevel();

   if(!level)
      return;

   // Snap!
   findMountPoint(level, getPos());
}


void EngineeredItem::setPos(const Point &p)
{
   Parent::setPos(p);

   computeObjectGeometry();
   updateExtentInDatabase();
}


void EngineeredItem::explode()
{
#ifndef ZAP_DEDICATED
   const S32 EXPLOSION_COLOR_COUNT = 12;

   static Color ExplosionColors[EXPLOSION_COLOR_COUNT] = {
      Colors::red,
      Color(0.9, 0.5, 0),
      Colors::white,
      Colors::yellow,
      Colors::red,
      Color(0.8, 1.0, 0),
      Colors::orange50,
      Colors::white,
      Colors::red,
      Color(0.9, 0.5, 0),
      Colors::white,
      Colors::yellow,
   };

   getGame()->playSoundEffect(SFXShipExplode, getPos());

   F32 a = TNL::Random::readF() * 0.4f + 0.5f;
   F32 b = TNL::Random::readF() * 0.2f + 0.9f;
   F32 c = TNL::Random::readF() * 0.15f + 0.125f;
   F32 d = TNL::Random::readF() * 0.2f + 0.9f;

   ClientGame *game = static_cast<ClientGame *>(getGame());

   Point pos = getPos();

   game->emitExplosion(pos, 0.65f, ExplosionColors, EXPLOSION_COLOR_COUNT);
   game->emitBurst(pos, Point(a,c) * 0.6f, Color(1, 1, 0.25), Colors::red);
   game->emitBurst(pos, Point(b,d) * 0.6f, Colors::yellow, Colors::yellow);

   disableCollision();
#endif
}


bool EngineeredItem::isDestroyed()
{
   return mIsDestroyed;
}


// Make sure position looks good when player deploys item with Engineer module -- make sure we're not deploying on top of
// a wall or another engineered item
// static method
bool EngineeredItem::checkDeploymentPosition(const Vector<Point> &thisBounds, const GridDatabase *gb)
{
   Vector<DatabaseObject *> foundObjects;
   Rect queryRect(thisBounds);
   gb->findObjects((TestFunc) isForceFieldCollideableType, foundObjects, queryRect);

   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      if( polygonsIntersect(thisBounds, *static_cast<BfObject *>(foundObjects[i])->getCollisionPoly()) )     // Do they intersect?
         return false;     // Bad location
   }
   return true;            // Good location
}


U32 EngineeredItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitialMask))
   {
      Point pos = getPos();

      stream->write(pos.x);
      stream->write(pos.y);
      stream->write(mAnchorNormal.x);
      stream->write(mAnchorNormal.y);
      stream->writeFlag(mEngineered);
   }

   if(stream->writeFlag(updateMask & TeamMask))
      writeThisTeam(stream);

   if(stream->writeFlag(updateMask & HealthMask))
   {
      if(stream->writeFlag(isEnabled()))
         stream->writeFloat((mHealth - DisabledLevel) / (1 - DisabledLevel), 5);
      else
         stream->writeFloat(mHealth / DisabledLevel, 5);

      stream->writeFlag(mIsDestroyed);
   }

   if(stream->writeFlag(updateMask & HealRateMask))
   {
      stream->writeInt(mHealRate, 16);
   }
   return 0;
}


void EngineeredItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;

   if(stream->readFlag())
   {
      Point pos;
      initial = true;
      stream->read(&pos.x);
      stream->read(&pos.y);
      stream->read(&mAnchorNormal.x);
      stream->read(&mAnchorNormal.y);
      mEngineered = stream->readFlag();
      setPos(pos);
   }


   if(stream->readFlag())
      readThisTeam(stream);

   if(stream->readFlag())
   {
      if(stream->readFlag())
         mHealth = stream->readFloat(5) * (1 - DisabledLevel) + DisabledLevel; // enabled
      else
         mHealth = stream->readFloat(5) * (DisabledLevel * 0.99f); // disabled, make sure (mHealth < DisabledLevel)


      bool wasDestroyed = mIsDestroyed;
      mIsDestroyed = stream->readFlag();

      if(mIsDestroyed && !wasDestroyed && !initial)
         explode();
   }

   if(stream->readFlag())
   {
      mHealRate = stream->readInt(16);
   }

   if(initial)
   {
      computeObjectGeometry();
      updateExtentInDatabase();
   }
}

void EngineeredItem::setHealRate(S32 rate)
{
   setMaskBits(HealRateMask);
   mHealRate = rate;
   mHealTimer.setPeriod(mHealRate * 1000);
}


S32 EngineeredItem::getHealRate() const
{
   return mHealRate;
}


void EngineeredItem::healObject(S32 time)
{
   if(mHealRate == 0 || getTeam() == TEAM_NEUTRAL)      // Neutral items don't heal!
      return;

   F32 prevHealth = mHealth;

   if(mHealTimer.update(time))
   {
      mHealth += .1f;
      setMaskBits(HealthMask);

      if(mHealth >= 1)
         mHealth = 1;
      else
         mHealTimer.reset();

      if(prevHealth < DisabledLevel && mHealth >= DisabledLevel)
         onEnabled();
   }
}


// Server only
void EngineeredItem::getBufferForBotZone(F32 bufferRadius, Vector<Point> &points) const
{
   offsetPolygon(getCollisionPoly(), points, bufferRadius);    // Fill zonePoints
}


static const F32 MAX_SNAP_DISTANCE = 100.0f;    // Max distance to look for a mount point

// Figure out where to mount this item during construction; mountToWall() is similar, but used in editor.  
// findDeployPoint() is version used during deployment of engineerered item.
void EngineeredItem::findMountPoint(const Level *level, const Point &pos)
{
   Point normal, anchor;

   // Anchor objects to the correct point
   if(findAnchorPointAndNormal(level, level->getWallEdgeDatabase(), pos, 
                               MAX_SNAP_DISTANCE, true, anchor, normal))
   {
      setPos(anchor);
      mAnchorNormal.set(normal);
   }
   else   // Found no mount point
   {
      setPos(pos);   
      mAnchorNormal.set(1,0);
   }

   computeObjectGeometry();      // Fills mCollisionPolyPoints 
   updateExtentInDatabase();
}


// Find mount point or turret or forcefield closest to pos; used in editor.  See findMountPoint() for in-game version.
void EngineeredItem::mountToWall(const Point &pos, 
                                 const GridDatabase *gameObjectDatabase, 
                                 const GridDatabase *wallEdgeDatabase)
{  
   Point normal, anchor;
   BfObject *mountSeg;

   mountSeg = findAnchorPointAndNormal(gameObjectDatabase,
                                       wallEdgeDatabase, 
                                       pos,    
                                       MAX_SNAP_DISTANCE, 
                                       true, 
                                       anchor, 
                                       normal);

   // It is possible to find an edge but not a segment while a wall is being dragged -- the edge remains in it's original location 
   // while the segment is being dragged around, some distance away
   if(mountSeg)   // Found a segment we can mount to
   {
      setPos(anchor);
      mAnchorNormal.set(normal);

      setMountSegment(mountSeg);

      mSnapped = true;
   }
   else           // No suitable segments found
   {
      mSnapped = false;
      setPos(pos);
   }  

   onGeomChanged();
}


#ifndef ZAP_DEDICATED

bool EngineeredItem::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   CounterMenuItem *menuItem = new CounterMenuItem("10% Heal:", getHealRate(), 1, 0, 100, "secs", "Disabled",
      "Time for this item to heal itself 10%");
   attributeMenu->addMenuItem(menuItem);

   return true;
}


void EngineeredItem::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   setHealRate(attributeMenu->getMenuItem(0)->getIntValue());
}

#endif


/////
// Lua interface
/**
 * @luaclass EngineeredItem
 * 
 * @brief Parent class representing mountable items such as Turret and
 * ForceFieldProjector.
 * 
 * @descr EngineeredItem is a container class for wall-mountable items.
 * Currently, all EngineeredItems can be constructed with the Engineering
 * module, can be destroyed by enemy fire, and can be healed (and sometimes
 * captured) with the Repair module.  All EngineeredItems have a health value
 * that ranges from 0 to 1, where 0 is completely dead and 1 is fully healthy.
 * When health falls below a certain threshold (see getDisabledThrehold()), the
 * item becomes inactive and must be repaired or regenerate itself to be
 * functional again.
 * 
 * If an EngineeredItem has a heal rate greater than zero, it will slowly repair
 * damage to iteself. For more info see setHealRate()
 */
//               Fn name              Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, isActive,             ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, getMountAngle,        ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, getHealth,            ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, setHealth,            ARRAYDEF({{ NUM,  END }}), 1 ) \
   METHOD(CLASS, getDisabledThreshold, ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, getHealRate,          ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, setHealRate,          ARRAYDEF({{ INT,  END }}), 1 ) \
   METHOD(CLASS, getEngineered,        ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, setEngineered,        ARRAYDEF({{ BOOL, END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(EngineeredItem, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(EngineeredItem, LUA_METHODS);

#undef LUA_METHODS

const char *EngineeredItem::luaClassName = "EngineeredItem";
REGISTER_LUA_SUBCLASS(EngineeredItem, Item);


/**
 * @luafunc bool EngineeredItem::isActive()
 * 
 * @brief Determine if the item is active (i.e. its health is above the
 * disbaledThreshold).
 * 
 * @descr A player can activate an inactive item by repairing it. To set whether
 * an EngineeredItem as active or disabled, use setHealth()
 * 
 * @return Returns `true` if the item is active, or `false` if it is disabled
 */
S32 EngineeredItem::lua_isActive(lua_State *L)
{ 
   return returnBool(L, isEnabled()); 
}


/**
 * @luafunc num EngineeredItem::getMountAngle()
 * 
 * @brief Gets the angle (in radians) at which the item is mounted.
 * 
 * @return Returns the mount angle, in radians.
 */
S32 EngineeredItem::lua_getMountAngle(lua_State *L)
{ 
   return returnFloat(L, mAnchorNormal.ATAN2()); 
}


/**
 * @luafunc num EngineeredItem::getHealth()
 * 
 * @brief Returns health of the item. 
 * 
 * @descr Health is specified as a number between 0 and 1 where 0 is completely
 * dead and 1 is totally healthy.
 * 
 * @return Returns a value between 0 and 1 indicating the health of the item.
 */
S32 EngineeredItem::lua_getHealth(lua_State *L)
{ 
   return returnFloat(L, mHealth);     
}


/**
 * @luafunc EngineeredItem::setHealth(num health)
 * 
 * @brief Set the current health of the item. 
 * 
 * @descr Health is specified as a number between 0 and 1 where 0 is completely
 * dead and 1 is totally healthy.  Values outside this range will be clamped to
 * the valid range.
 * 
 * @param health The item's new health, between 0 and 1.
 */
S32 EngineeredItem::lua_setHealth(lua_State *L)
{ 
   checkArgList(L, functionArgs, "EngineeredItem", "setHealth");
   F32 flt = getFloat(L, 1);
   F32 newHealth = CLAMP(flt, 0, 1);


   // Just 'damage' the engineered item to take care of all of the disabling/mask/etc.
   DamageInfo di;
   di.damagingObject = NULL;

   F32 healthDifference = mHealth - newHealth;
   if(healthDifference > 0)
      di.damageAmount = 4.0f * healthDifference;
   else
      di.damageAmount = healthDifference;

   damageObject(&di);

   return 0;
}


/**
 * @luafunc num EngineeredItem::getDisabledThreshold()
 * 
 * @brief Gets the health threshold below which the item becomes disabled. 
 * 
 * @descr The value will always be between 0 and 1. This value is constant and
 * will be the same for all \link EngineeredItem EngineeredItems \endlink.
 * 
 * @return Health threshold below which the item will be disabled.
 */
S32 EngineeredItem::lua_getDisabledThreshold(lua_State *L)
{
   return returnFloat(L, DisabledLevel);
}


/**
 * @luafunc int EngineeredItem::getHealRate()
 * 
 * @brief Gets the item's healRate. 
 * 
 * @descr The specified heal rate will be the time, in seconds, it takes for the
 * item to repair itself by 10.  If an EngineeredItem is assigned to the neutral
 * team, it will not heal itself.
 * 
 * @return The item's heal rate
 */
S32 EngineeredItem::lua_getHealRate(lua_State *L)
{
   return returnInt(L, mHealRate);
}


/**
 * @luafunc EngineeredItem::setHealRate(int healRate)
 * 
 * @brief Sets the item's heal rate. 
 * 
 * @descr The specified `healRate` will be the time, in seconds, it takes for
 * the item to repair itself by 10. In practice, a heal rate of 1 makes an item
 * effectively unkillable. If the item is assigned to the neutral team, it will
 * not heal itself.  Passing a negative value will generate an error.
 * 
 * @param healRate The new heal rate. Specify 0 to disable healing.
 */
S32 EngineeredItem::lua_setHealRate(lua_State *L)
{
   checkArgList(L, functionArgs, "EngineeredItem", "setHealRate");

   S32 healRate = getInt(L, 1);

   if(healRate < 0)
      THROW_LUA_EXCEPTION(L, "Specified healRate is negative, and that just makes me crazy!");

   setHealRate(healRate);

   return returnInt(L, mHealRate);
}


/**
 * @luafunc bool EngineeredItem::getEngineered()
 * 
 * @breif Get whether the item can be totally destroyed
 * 
 * @return `true` if the item can be destroyed.
 */
S32 EngineeredItem::lua_getEngineered(lua_State *L)
{
   return returnBool(L, mEngineered);
}


/**
 * @luafunc EngineeredItem::setEngineered(engineered)
 * 
 * @brief Sets whether the item can be destroyed when its health reaches zero.
 * 
 * @param engineered `true` to make the item destructible, `false` to make it
 * permanent
 */
S32 EngineeredItem::lua_setEngineered(lua_State *L)
{
   checkArgList(L, functionArgs, "EngineeredItem", "setEngineered");

   mEngineered = getBool(L, 1);
   setMaskBits(InitialMask);

   return returnBool(L, mEngineered);
}


// Override some methods
S32 EngineeredItem::lua_setGeom(lua_State *L)
{
   S32 retVal = Parent::lua_setGeom(L);

   findMountPoint(getGame()->getLevel(), getPos());

   return retVal;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(ForceFieldProjector);

/**
 * @luafunc ForceFieldProjector::ForceFieldProjector()
 * @luafunc ForceFieldProjector::ForceFieldProjector(point)
 * @luafunc ForceFieldProjector::ForceFieldProjector(point, teamIndex)
 */
// Combined Lua / C++ default constructor
ForceFieldProjector::ForceFieldProjector(lua_State *L) : Parent(TEAM_NEUTRAL, Point(0,0), Point(1,0))
{
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { PT, TEAM_INDX, END }}, 3 };

      S32 profile = checkArgList(L, constructorArgList, "ForceFieldProjector", "constructor");

      if(profile == 1 )
      {
         setPos(L, 1);
         setTeam(TEAM_NEUTRAL);
      }
      if(profile == 2)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }

      findMountPoint(getGame()->getLevel(), getPos());
   }

   initialize();
}


// Constructor for when projector is built with engineer
ForceFieldProjector::ForceFieldProjector(S32 team, const Point &anchorPoint, const Point &anchorNormal) : 
   Parent(team, anchorPoint, anchorNormal)
{
   initialize();
}


// Destructor
ForceFieldProjector::~ForceFieldProjector()
{
   LUAW_DESTRUCTOR_CLEANUP;

   if(mNeedToCleanUpField)
      delete mField.getPointer();
}


void ForceFieldProjector::initialize()
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = ForceFieldProjectorTypeNumber;
   onGeomChanged();     // Can't be placed on parent, as parent constructor must initalized first

   mField = NULL;

   mNeedToCleanUpField = false;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


ForceFieldProjector *ForceFieldProjector::clone() const
{
   ForceFieldProjector *ffp = new ForceFieldProjector(*this);
   if(mField)
      ffp->mField = mField->clone();

   return ffp;
}


void ForceFieldProjector::onDisabled()
{
   if(mField.isValid())
      mField->deleteObject(0);
}


void ForceFieldProjector::idle(BfObject::IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   healObject(mCurrentMove.time);
}


static const S32 PROJECTOR_OFFSET = 15;      // Distance from wall to projector tip; thickness, if you will

F32 ForceFieldProjector::getSelectionOffsetMagnitude()
{
   return PROJECTOR_OFFSET / 3;     // Centroid of a triangle is at 1/3 its height
}


Vector<Point> ForceFieldProjector::getObjectGeometry(const Point &anchor, const Point &normal) const
{
   return getForceFieldProjectorGeometry(anchor, normal);
}


// static method
Vector<Point> ForceFieldProjector::getForceFieldProjectorGeometry(const Point &anchor, const Point &normal)
{
   static const S32 PROJECTOR_HALF_WIDTH = 12;  // Half the width of base of the projector, along the wall

   Vector<Point> geom;
   geom.reserve(3);

   Point cross(normal.y, -normal.x);
   cross.normalize((F32)PROJECTOR_HALF_WIDTH);

   geom.push_back(getForceFieldStartPoint(anchor, normal));
   geom.push_back(anchor - cross);
   geom.push_back(anchor + cross);

   TNLAssert(!isWoundClockwise(geom), "Go the other way!");

   return geom;
}


// Get the point where the forcefield actually starts, as it leaves the projector; i.e. the tip of the projector.  Static method.
Point ForceFieldProjector::getForceFieldStartPoint(const Point &anchor, const Point &normal, F32 scaleFact)
{
   return Point(anchor.x + normal.x * PROJECTOR_OFFSET * scaleFact, 
                anchor.y + normal.y * PROJECTOR_OFFSET * scaleFact);
}


void ForceFieldProjector::getForceFieldStartAndEndPoints(Point &start, Point &end) const
{
   Point pos = getPos();

   start = getForceFieldStartPoint(pos, mAnchorNormal);

   ForceField::findForceFieldEnd(getDatabase(), getForceFieldStartPoint(pos, mAnchorNormal), mAnchorNormal, end);
}


// Forcefield projector has been turned on some how; either at the beginning of a level, or via repairing, or deploying. 
// Called on both client and server, does nothing on client.
void ForceFieldProjector::onEnabled()
{
   // Server only -- nothing to do on client!
   if(isGhost())
      return;

   // Database can be NULL here if adding a forcefield from the editor:  The editor will
   // add a new game object *without* adding it to a grid database in order to optimize
   // adding large groups of objects with copy/paste/undo/redo
   if(!getDatabase())
      return;

   if(mField.isNull())     // Add mField only when we don't have any
   {
      Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
      Point end;
      ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end);

      mField = new ForceField(getTeam(), start, end);
      mField->addToGame(getGame(), getGame()->getLevel());
   }
}


const Vector<Point> *ForceFieldProjector::getCollisionPoly() const
{
   TNLAssert(mCollisionPolyPoints.size() != 0, "mCollisionPolyPoints.size() shouldn't be zero");
   return &mCollisionPolyPoints;
}


// Create a dummy ForceField object to help illustrate placement of ForceFieldProjectors in the editor
void ForceFieldProjector::createCaptiveForceField()
{
   Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
   Point end;
   ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end);

   TNLAssert(!mField, "Better clean up mField!");
   mField = new ForceField(getTeam(), start, end);    // Not added to a database, so needs to be cleaned up by us
   mNeedToCleanUpField = true;
}


void ForceFieldProjector::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
}


void ForceFieldProjector::onAddedToEditor()
{
   Parent::onAddedToEditor();

   TNLAssert(!mField, "Shouldn't have a captive forcefield yet!");
   createCaptiveForceField();
}


void ForceFieldProjector::render() const
{
#ifndef ZAP_DEDICATED
   // We're not in editor (connected to game)
   if (getGame() && static_cast<ClientGame*>(getGame())->isConnectedToServer())
      GameObjectRender::renderForceFieldProjector(&mCollisionPolyPoints, getPos(), getColor(), isEnabled(), mHealRate);
   else
      renderEditor(0, false);
#endif
}


void ForceFieldProjector::renderDock(const Color &color) const
{
   GameObjectRender::renderSquareItem(getPos(), color, 1, Colors::white, '>');
}


void ForceFieldProjector::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices) const
{
#ifndef ZAP_DEDICATED
   F32 scaleFact = 1;
   const Color &color = getColor();

   if(mSnapped)
   {
      Point forceFieldStart = getForceFieldStartPoint(getPos(), mAnchorNormal, scaleFact);

      GameObjectRender::renderForceFieldProjector(&mCollisionPolyPoints, getPos(), color, true, mHealRate);

      if(mField)
         mField->render(color);
   }
   else
      renderDock(color);
#endif
}


const char *ForceFieldProjector::getOnScreenName()     const {  return "ForceFld";  }
const char *ForceFieldProjector::getOnDockName()       const {  return "ForceFld";  }
const char *ForceFieldProjector::getPrettyNamePlural() const {  return "Force Field Projectors";  }
const char *ForceFieldProjector::getEditorHelpString() const {  return "Creates a force field that lets only team members pass. [F]";  }


bool ForceFieldProjector::hasTeam() { return true; }
bool ForceFieldProjector::canBeHostile() { return true; }
bool ForceFieldProjector::canBeNeutral() { return true; }


// Determine on which segment forcefield lands -- only used in the editor, wraps ForceField::findForceFieldEnd()
void ForceFieldProjector::findForceFieldEnd()
{
   if(!mField)
      return;

   // Load the corner points of a maximum-length forcefield into geom
   DatabaseObject *collObj;

   F32 scale = 1;
   
   Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
   Point end;

   // Pass in database containing WallSegments, returns object in collObj
   collObj = ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end);
   mField->setStartAndEndPoints(start, end);
   
   setExtent(Rect(ForceField::computeGeom(start, end, scale)));
}


void ForceFieldProjector::onGeomChanged()
{
   if(mField && mSnapped)
      findForceFieldEnd();

   Parent::onGeomChanged();
}


/////
// Lua interface

// No custom ForceFieldProjector methods
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(ForceFieldProjector, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(ForceFieldProjector, LUA_METHODS);

#undef LUA_METHODS

const char *ForceFieldProjector::luaClassName = "ForceFieldProjector";
REGISTER_LUA_SUBCLASS(ForceFieldProjector, EngineeredItem);

// LuaItem methods -- override method in parent class
S32 ForceFieldProjector::lua_getPos(lua_State *L)
{
   return returnPoint(L, getPos() + mAnchorNormal * getRadius() );
}


S32 ForceFieldProjector::lua_setPos(lua_State *L)
{
   // TODO
   return Parent::lua_setPos(L);
}


S32 ForceFieldProjector::lua_removeFromGame(lua_State *L)
{
   // Remove field
   onDisabled();

   return Parent::lua_removeFromGame(L);
}


S32 ForceFieldProjector::lua_setTeam(lua_State *L)
{
   // Save old team
   S32 prevTeam = getTeam();

   // Change to new team
   Parent::lua_setTeam(L);

   // We need to set the mOriginalTeam team as the just-set team because of conflicts with
   // projector-disabled logic due to the fact that they can start as neutral
   mOriginalTeam = getTeam();

   // Only re-add a forcefield if the team has changed and if it isn't disabled
   //
   // We're duplicating a lot of logic in the onEnabled() method because calling onEnabled()
   // doesn't seem to work right after calling onDisabled().  Probably because of slow deletion?
   if(mOriginalTeam != prevTeam && isEnabled() && getGame())
   {
      onDisabled();

      Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
      Point end;

      DatabaseObject *collObj = ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end);

      delete mField.getPointer();
      mField = new ForceField(getTeam(), start, end);
      mField->addToGame(getGame(), getGame()->getLevel());
   }

   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(ForceField);

ForceField::ForceField(S32 team, Point start, Point end) 
{
   setTeam(team);
   mStart = start;
   mEnd = end;

   mOutline = computeGeom(mStart, mEnd);

   Rect extent(mStart, mEnd);
   extent.expand(Point(5,5));
   setExtent(extent);

   mFieldUp = true;
   mObjectTypeNumber = ForceFieldTypeNumber;
   mNetFlags.set(Ghostable);

   setNewGeometry(geomSimpleLine);     // Not used, keeps clone from blowing up

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
ForceField::~ForceField()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


ForceField *ForceField::clone() const
{
   return new ForceField(*this);
}


bool ForceField::collide(BfObject *hitObject)
{
   if(!mFieldUp)
      return false;

   // If it's a ship that collides with this forcefield, check team to allow it through
   if(isShipType(hitObject->getObjectTypeNumber()))
   {
      if(hitObject->getTeam() == getTeam())     // Ship and force field are same team
      {
         if(!isGhost())
         {
            mFieldUp = false;
            mDownTimer.reset(FieldDownTime);
            setMaskBits(StatusMask);
         }
         return false;
      }
   }
   // If it's a flag that collides with this forcefield and we're hostile, let it through
   else if(hitObject->getObjectTypeNumber() == FlagTypeNumber)
   {
      if(getTeam() == TEAM_HOSTILE)
         return false;
      else
         return true;
   }

   return true;
}


// Returns true if two forcefields intersect
bool ForceField::intersects(ForceField *forceField)
{
   return polygonsIntersect(mOutline, *forceField->getOutline());
}


const Vector<Point> *ForceField::getOutline() const
{
   return &mOutline;
}


void ForceField::setStartAndEndPoints(const Point &start, const Point &end)
{
   mStart = start;
   mEnd = end;
}


void ForceField::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
}


void ForceField::idle(BfObject::IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   if(mDownTimer.update(mCurrentMove.time))
   {
      // do an LOS test to see if anything is in the field:
      F32 t;
      Point n;
      if(!findObjectLOS((TestFunc)isForceFieldDeactivatingType, ActualState, mStart, mEnd, t, n))
      {
         mFieldUp = true;
         setMaskBits(StatusMask);
      }
      else
         mDownTimer.reset(10);
   }
}


// TODO: I don't think this is right -- we are sending important state information about the FF using
// unverified packets that, if lost, will not be retransmitted.  I think it better to send this info either
// as part of the ghosting process or as an s2c.  Thoughts?
U32 ForceField::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitialMask))
   {
      stream->write(mStart.x);
      stream->write(mStart.y);
      stream->write(mEnd.x);
      stream->write(mEnd.y);
      writeThisTeam(stream);
   }
   stream->writeFlag(mFieldUp);
   return 0;
}


void ForceField::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   if(stream->readFlag())
   {
      initial = true;
      stream->read(&mStart.x);
      stream->read(&mStart.y);
      stream->read(&mEnd.x);
      stream->read(&mEnd.y);
      readThisTeam(stream);
      mOutline = computeGeom(mStart, mEnd);

      Rect extent(mStart, mEnd);
      extent.expand(Point(5,5));
      setExtent(extent);
   }
   bool wasUp = mFieldUp;
   mFieldUp = stream->readFlag();

   if(initial || (wasUp != mFieldUp))
      getGame()->playSoundEffect(mFieldUp ? SFXForceFieldUp : SFXForceFieldDown, mStart);
}


const F32 ForceField::ForceFieldHalfWidth = 2.5;

// static
Vector<Point> ForceField::computeGeom(const Point &start, const Point &end, F32 scaleFact)
{
   Vector<Point> geom;
   geom.reserve(4);

   Point normal(end.y - start.y, start.x - end.x);
   normal.normalize(ForceFieldHalfWidth * scaleFact);

   geom.push_back(start + normal);
   geom.push_back(end + normal);
   geom.push_back(end - normal);
   geom.push_back(start - normal);

   return geom;
}


// Pass in a database containing walls or wallsegments
// Static method
DatabaseObject *ForceField::findForceFieldEnd(const GridDatabase *database, const Point &start, const Point &normal, Point &end)
{
   F32 time;
   Point n;

   end.set(start.x + normal.x * MAX_FORCEFIELD_LENGTH, start.y + normal.y * MAX_FORCEFIELD_LENGTH);

   DatabaseObject *collObj = database->findObjectLOS((TestFunc)isWallType, ActualState, start, end, time, n);

   if(collObj)
      end.set(start + (end - start) * time); 

   return collObj;
}


const Vector<Point> *ForceField::getCollisionPoly() const
{
   return &mOutline;
}


void ForceField::render() const
{
   render(getColor());
}


void ForceField::render(const Color &color) const
{
   GameObjectRender::renderForceField(mStart, mEnd, color, mFieldUp);
}


S32 ForceField::getRenderSortValue()
{
   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Turret);


const F32 Turret::TURRET_OFFSET = 15; 

// Combined Lua / C++ default constructor
/**
 * @luafunc Turret::Turret()
 * @luafunc Turret::Turret(point, team)
 */
Turret::Turret(lua_State *L) : Parent(TEAM_NEUTRAL, Point(0,0), Point(1,0))
{
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { PT, TEAM_INDX, END }}, 2 };
      S32 profile = checkArgList(L, constructorArgList, "Turret", "constructor");
      
      if(profile == 1 )
      {
         setPos(L, 1);
         setTeam(TEAM_NEUTRAL);
      }
      if(profile == 2)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }
   }

   initialize();
}


// Constructor for when turret is built with engineer
Turret::Turret(S32 team, const Point &anchorPoint, const Point &anchorNormal) : Parent(team, anchorPoint, anchorNormal)
{
   initialize();
}


// Destructor
Turret::~Turret()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void Turret::initialize()
{
   mObjectTypeNumber = TurretTypeNumber;

   mWeaponFireType = WeaponTurret;
   mNetFlags.set(Ghostable);

   onGeomChanged();

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


Turret *Turret::clone() const
{
   return new Turret(*this);
}


// Turret <Team> <X> <Y> [HealRate]
bool Turret::processArguments(S32 argc2, const char **argv2, Level *level)
{
   S32 argc1 = 0;
   const char *argv1[32];

   for(S32 i = 0; i < argc2; i++)
   {
      char firstChar = argv2[i][0];

      if((firstChar >= 'a' && firstChar <= 'z') || (firstChar >= 'A' && firstChar <= 'Z'))  // starts with a letter
      {
         if(!strncmp(argv2[i], "W=", 2))  // W= is in 015a
         {
            S32 w = 0;
            while(w < WeaponCount && stricmp(WeaponInfo::getWeaponInfo(WeaponType(w)).name.getString(), &argv2[i][2]))
               w++;
            if(w < WeaponCount)
               mWeaponFireType = WeaponType(w);
            break;
         }
      }
      else
      {
         if(argc1 < 32)
         {
            argv1[argc1] = argv2[i];
            argc1++;
         }
      }
   }

   if (!EngineeredItem::processArguments(argc1, argv1, level))
      return false;

   mCurrentAngle = mAnchorNormal.ATAN2();
   return true;
}


string Turret::toLevelCode() const
{
   string out = Parent::toLevelCode();

   if(mWeaponFireType != WeaponTurret)
      out = out + " " + writeLevelString((string("W=") + WeaponInfo::getWeaponInfo(mWeaponFireType).name.getString()).c_str());

   return out;
}


Vector<Point> Turret::getObjectGeometry(const Point &anchor, const Point &normal) const
{
   return getTurretGeometry(anchor, normal);
}


// static method
Vector<Point> Turret::getTurretGeometry(const Point &anchor, const Point &normal)
{
   Point cross(normal.y, -normal.x);

   Vector<Point> polyPoints;
   polyPoints.reserve(4);

   polyPoints.push_back(anchor + cross * 25);
   polyPoints.push_back(anchor + cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 25);

   TNLAssert(!isWoundClockwise(polyPoints), "Go the other way!");

   return polyPoints;
}


const Vector<Point> *Turret::getCollisionPoly() const
{
   return &mCollisionPolyPoints;
}


const Vector<Point> *Turret::getOutline() const
{
   return getCollisionPoly();
}


F32 Turret::getEditorRadius(F32 currentScale) const
{
   if(mSnapped)
      return 25 * currentScale;
   else 
      return Parent::getEditorRadius(currentScale);
}


F32 Turret::getSelectionOffsetMagnitude()
{
   return 20;
}


void Turret::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
   mCurrentAngle = mAnchorNormal.ATAN2();
}


void Turret::render() const
{
   GameObjectRender::renderTurret(getColor(), getPos(), mAnchorNormal, isEnabled(), mHealth, mCurrentAngle, mHealRate);
}


void Turret::renderDock(const Color &color) const
{
   GameObjectRender::renderSquareItem(getPos(), color, 1, Colors::white, 'T');
}


void Turret::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices) const
{
   if(mSnapped)
   {
      // We render the turret with/without health if it is neutral or not (as it starts in the game)
      bool enabled = getTeam() != TEAM_NEUTRAL;

      GameObjectRender::renderTurret(getColor(), getPos(), mAnchorNormal, enabled, mHealth, mCurrentAngle, mHealRate);
   }
   else
      renderDock(getColor());
}


U32 Turret::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);
   if(stream->writeFlag(updateMask & AimMask))
      stream->write(mCurrentAngle);

   return ret;
}


void Turret::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
      stream->read(&mCurrentAngle);
}


// Choose target, aim, and, if possible, fire
void Turret::idle(IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   // Server only!

   healObject(mCurrentMove.time);

   if(!isEnabled())
      return;

   mFireTimer.update(mCurrentMove.time);

   // Choose best target:
   Point aimPos = getPos() + mAnchorNormal * TURRET_OFFSET;
   Point cross(mAnchorNormal.y, -mAnchorNormal.x);

   Rect queryRect(aimPos, aimPos);
   queryRect.unionPoint(aimPos + cross * TurretPerceptionDistance);
   queryRect.unionPoint(aimPos - cross * TurretPerceptionDistance);
   queryRect.unionPoint(aimPos + mAnchorNormal * TurretPerceptionDistance);
   fillVector.clear();
   findObjects((TestFunc)isTurretTargetType, fillVector, queryRect);    // Get all potential targets

   BfObject *bestTarget = NULL;
   F32 bestRange = F32_MAX;
   Point bestDelta;

   Point delta;
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      if(isShipType(fillVector[i]->getObjectTypeNumber()))
      {
         Ship *potential = static_cast<Ship *>(fillVector[i]);

         // Is it dead or cloaked?  Carrying objects makes ship visible, except in nexus game
         if(!potential->isVisible(false) || potential->mHasExploded)
            continue;
      }

      // Don't target mounted items (like resourceItems and flagItems)
      if(isMountableItemType(fillVector[i]->getObjectTypeNumber()))
         if(static_cast<MountableItem *>(fillVector[i])->isMounted())
            continue;
      
      BfObject *potential = static_cast<BfObject *>(fillVector[i]);
      if(potential->getTeam() == getTeam())     // Is target on our team?
         continue;                              // ...if so, skip it!

      // Calculate where we have to shoot to hit this...
      Point Vs = potential->getVel();
      F32 S = (F32)WeaponInfo::getWeaponInfo(mWeaponFireType).projVelocity;
      Point d = potential->getPos() - aimPos;

// This could possibly be combined with Robot's getFiringSolution, as it's essentially the same thing
      F32 t;      // t is set in next statement
      if(!findLowestRootInInterval(Vs.dot(Vs) - S * S, 2 * Vs.dot(d), d.dot(d), WeaponInfo::getWeaponInfo(mWeaponFireType).projLiveTime * 0.001f, t))
         continue;

      Point leadPos = potential->getPos() + Vs * t;

      // Calculate distance
      delta = (leadPos - aimPos);

      Point angleCheck = delta;
      angleCheck.normalize();

      // Check that we're facing it...
      if(angleCheck.dot(mAnchorNormal) <= -0.1f)
         continue;

      // See if we can see it...
      Point n;
      if(findObjectLOS((TestFunc)isWallType, ActualState, aimPos, potential->getPos(), t, n))
         continue;

      // See if we're gonna clobber our own stuff...
      disableCollision();
      Point delta2 = delta;
      delta2.normalize(WeaponInfo::getWeaponInfo(mWeaponFireType).projLiveTime * (F32)WeaponInfo::getWeaponInfo(mWeaponFireType).projVelocity / 1000.f);
      BfObject *hitObject = findObjectLOS((TestFunc) isWithHealthType, 0, aimPos, aimPos + delta2, t, n);
      enableCollision();

      // Skip this target if there's a friendly object in the way
      if(hitObject && hitObject->getTeam() == getTeam() &&
        (hitObject->getPos() - aimPos).lenSquared() < delta.lenSquared())         
         continue;

      F32 dist = delta.len();

      if(dist < bestRange)
      {
         bestDelta  = delta;
         bestRange  = dist;
         bestTarget = potential;
      }
   }

   if(!bestTarget)      // No target, nothing to do
      return;
 
   // Aim towards the best target.  Note that if the turret is at one extreme of its range, and the target is at the other,
   // then the turret will rotate the wrong-way around to aim at the target.  If we were to detect that condition here, and
   // constrain our turret to turning the correct direction, that would be great!!
   F32 destAngle = bestDelta.ATAN2();

   F32 angleDelta = destAngle - mCurrentAngle;

   if(angleDelta > FloatPi)
      angleDelta -= Float2Pi;
   else if(angleDelta < -FloatPi)
      angleDelta += Float2Pi;

   F32 maxTurn = TurretTurnRate * mCurrentMove.time * 0.001f;

   if(angleDelta != 0)
      setMaskBits(AimMask);

   if(angleDelta > maxTurn)
      mCurrentAngle += maxTurn;
   else if(angleDelta < -maxTurn)
      mCurrentAngle -= maxTurn;
   else
   {
      mCurrentAngle = destAngle;

      if(mFireTimer.getCurrent() == 0)
      {
         bestDelta.normalize();
         Point velocity;
         
         // String handling in C++ is such a mess!!!
         string killer = string("got blasted by ") + getGame()->getTeamName(getTeam()).getString() + " turret";
         mKillString = killer.c_str();

         GameWeapon::createWeaponProjectiles(WeaponType(mWeaponFireType), bestDelta, aimPos, velocity, 
                                             0, mWeaponFireType == WeaponBurst ? 45.f : 35.f, this);
         mFireTimer.reset(WeaponInfo::getWeaponInfo(mWeaponFireType).fireDelay);
      }
   }
}


const char *Turret::getOnScreenName()     const  { return "Turret";  }
const char *Turret::getOnDockName()       const  { return "Turret";  }
const char *Turret::getPrettyNamePlural() const  { return "Turrets"; }
const char *Turret::getEditorHelpString() const  { return "Creates shooting turret.  Can be on a team, neutral, or \"hostile to all\". [Y]"; }


bool Turret::hasTeam()      { return true; }
bool Turret::canBeHostile() { return true; }
bool Turret::canBeNeutral() { return true; }


void Turret::onGeomChanged() 
{ 
   mCurrentAngle = mAnchorNormal.ATAN2();       // Keep turret pointed away from the wall... looks better like that!
   Parent::onGeomChanged();
}


/////
// Lua interface
/**
 * @luaclass Turret
 * 
 * @brief Mounted gun that shoots at enemy ships and other objects.
 */
//               Fn name     Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getAimAngle,  ARRAYDEF({{      END }}), 1 ) \
   METHOD(CLASS, setAimAngle,  ARRAYDEF({{ NUM, END }}), 1 ) \
   METHOD(CLASS, setWeapon,    ARRAYDEF({{ WEAP_ENUM, END }}), 1 ) \


GENERATE_LUA_METHODS_TABLE(Turret, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(Turret, LUA_METHODS);

#undef LUA_METHODS


const char *Turret::luaClassName = "Turret";
REGISTER_LUA_SUBCLASS(Turret, EngineeredItem);


/**
 * @luafunc num Turret::getAimAngle()
 * 
 * @brief Returns the angle (in radians) at which the Turret is aiming.
 * 
 * @return The angle (in radians) at which the Turret is aiming.
 */
S32 Turret::lua_getAimAngle(lua_State *L)
{
   return returnFloat(L, mCurrentAngle);
}


/**
 * @luafunc Turret::setAimAngle(num angle)
 * 
 * @brief Sets the angle (in radians) where the Turret should aim.
 * 
 * @param angle Angle (in radians) where the turret should aim.
 */
S32 Turret::lua_setAimAngle(lua_State *L)
{
   checkArgList(L, functionArgs, "Turret", "setAimAngle");
   mCurrentAngle = getFloat(L, 1);

   return 0;
}


/**
 * @luafunc Turret::setWeapon(Weapon weapon)
 *
 * @brief Sets the weapon for this turret to use.
 *
 * @param weapon Weapon to set on the turret
 *
 * @note This is experimental and may be removed or changed from the game at any time
 */
S32 Turret::lua_setWeapon(lua_State *L)
{
   checkArgList(L, functionArgs, "Turret", "setWeapon");

   mWeaponFireType = getWeaponType(L, 1);

   return 0;
}


// Override some methods
S32 Turret::lua_getRad(lua_State *L)
{
   return returnFloat(L, TURRET_OFFSET);
}


S32 Turret::lua_getPos(lua_State *L)
{
   return returnPoint(L, getPos() + mAnchorNormal * TURRET_OFFSET);
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Mortar);


const F32 Mortar::MORTAR_OFFSET = 25;

// Combined Lua / C++ default constructor
/**
 * @luafunc Mortar::Mortar()
 * @luafunc Mortar::Mortar(point, team)
 */
Mortar::Mortar(lua_State *L) : Parent(TEAM_NEUTRAL, Point(0, 0), Point(1, 0))
{
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { PT, TEAM_INDX, END }}, 2 };
      S32 profile = checkArgList(L, constructorArgList, "Mortar", "constructor");
      
      if(profile == 1 )
      {
         setPos(L, 1);
         setTeam(TEAM_NEUTRAL);
      }
      if(profile == 2)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }
   }

   initialize();
}


// Constructor for when Mortar is built with engineer
Mortar::Mortar(S32 team, const Point &anchorPoint, const Point &anchorNormal) : Parent(team, anchorPoint, anchorNormal)
{
   initialize();
}


// Destructor
Mortar::~Mortar()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void Mortar::initialize()
{
   mObjectTypeNumber = MortarTypeNumber;

   mWeaponFireType = WeaponSeeker;
   mNetFlags.set(Ghostable);

   onGeomChanged();

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


Mortar *Mortar::clone() const
{
   return new Mortar(*this);
}


// Mortar <Team> <X> <Y> [HealRate]
bool Mortar::processArguments(S32 argc2, const char **argv2, Level *level)
{
   S32 argc1 = 0;
   const char *argv1[32];

   for(S32 i = 0; i < argc2; i++)
   {
      char firstChar = argv2[i][0];

      if((firstChar >= 'a' && firstChar <= 'z') || (firstChar >= 'A' && firstChar <= 'Z'))  // starts with a letter
      {
         if(!strncmp(argv2[i], "W=", 2))  // W= is in 015a
         {
            S32 w = 0;
            while(w < WeaponCount && stricmp(WeaponInfo::getWeaponInfo(WeaponType(w)).name.getString(), &argv2[i][2]))
               w++;
            if(w < WeaponCount)
               mWeaponFireType = WeaponType(w);
            break;
         }
      }
      else
      {
         if(argc1 < 32)
         {
            argv1[argc1] = argv2[i];
            argc1++;
         }
      }
   }

   if (!EngineeredItem::processArguments(argc1, argv1, level))
      return false;

   return true;
}


string Mortar::toLevelCode() const
{
   string out = Parent::toLevelCode();

   if(mWeaponFireType != WeaponSeeker)
      out = out + " " + writeLevelString((string("W=") + WeaponInfo::getWeaponInfo(mWeaponFireType).name.getString()).c_str());

   return out;
}


Vector<Point> Mortar::getObjectGeometry(const Point &anchor, const Point &normal) const
{
   return getMortarGeometry(anchor, normal);
}


// static method
Vector<Point> Mortar::getMortarGeometry(const Point &anchor, const Point &normal)
{
   Point cross(normal.y, -normal.x);

   Vector<Point> polyPoints;
   polyPoints.reserve(4);

   polyPoints.push_back(anchor + cross * 25);
   polyPoints.push_back(anchor + cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 25);

   TNLAssert(!isWoundClockwise(polyPoints), "Go the other way!");

   return polyPoints;
}


const Vector<Point> *Mortar::getCollisionPoly() const
{
   return getOutline();
}


const Vector<Point> *Mortar::getOutline() const
{
   return &mCollisionPolyPoints;
}


F32 Mortar::getEditorRadius(F32 currentScale) const
{
   if(mSnapped)
      return 25 * currentScale;
   else 
      return Parent::getEditorRadius(currentScale);
}


F32 Mortar::getSelectionOffsetMagnitude()
{
   return 20;
}


void Mortar::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
}


void Mortar::render() const
{
   GameObjectRender::renderMortar(getColor(), getPos(), mAnchorNormal, isEnabled(), mHealth, mHealRate);

   // Render target zone?
//   mGL->glPushMatrix();
//   Point aimCenter = getPos() + mAnchorNormal * Turret::TURRET_OFFSET;
//   glTranslate(aimCenter);
//
//   glRotate(mAnchorNormal.ATAN2() * RADIANS_TO_DEGREES);
//
//   renderPointVector(&mZone, GLOPT::LineLoop);
//   mGL->glPopMatrix();
}


void Mortar::renderDock(const Color &color) const
{
   GameObjectRender::renderSquareItem(getPos(), color, 1, Colors::white, 'M');
}


void Mortar::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices) const
{
   if(mSnapped)
   {
      // We render the Mortar with/without health if it is neutral or not (as it starts in the game)
      bool enabled = getTeam() != TEAM_NEUTRAL;

      GameObjectRender::renderMortar(getColor(), getPos(), mAnchorNormal, enabled, mHealth, mHealRate);
   }
   else
      renderDock(getColor());
}


U32 Mortar::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);

   return ret;
}


void Mortar::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);
}


// Choose target, and, if possible, fire
void Mortar::idle(IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   // Server only!

   healObject(mCurrentMove.time);

   if(!isEnabled())
      return;

   mFireTimer.update(mCurrentMove.time);

   // Choose best target:
   Point aimPos = getPos() + mAnchorNormal * MORTAR_OFFSET;
   Point cross(mAnchorNormal.y, -mAnchorNormal.x);

   Rect queryRect(mZone);
   fillVector.clear();
   findObjects((TestFunc)isTurretTargetType, fillVector, queryRect);    // Get all potential targets

   BfObject *bestTarget = NULL;
   F32 bestRange = F32_MAX;
   Point bestDelta;

   Point delta;
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      if(isShipType(fillVector[i]->getObjectTypeNumber()))
      {
         Ship *potential = static_cast<Ship *>(fillVector[i]);

         // Is it dead or cloaked?  Carrying objects makes ship visible, except in nexus game
         if(!potential->isVisible(false) || potential->mHasExploded)
            continue;

         //bool polygonContainsPoint(const Point *vertices, S32 vertexCount, const Point &poin
         if(!polygonContainsPoint(mZone.address(), mZone.size(), potential->getPos()))
            continue;
      }

      // Don't target mounted items (like resourceItems and flagItems)
      if(isMountableItemType(fillVector[i]->getObjectTypeNumber()))
         if(static_cast<MountableItem *>(fillVector[i])->isMounted())
            continue;
      
      BfObject *potential = static_cast<BfObject *>(fillVector[i]);
      if(potential->getTeam() == getTeam())     // Is target on our team?
         continue;                              // ...if so, skip it!

      // See if we can see it...
      Point n;
      F32 t;
      if(findObjectLOS((TestFunc)isWallType, ActualState, aimPos, potential->getPos(), t, n))
         continue;

      // See if we're gonna clobber our own stuff...
      disableCollision();
      Point delta2 = delta;
      delta2.normalize(WeaponInfo::getWeaponInfo(mWeaponFireType).projLiveTime * (F32)WeaponInfo::getWeaponInfo(mWeaponFireType).projVelocity / 1000.f);
      BfObject *hitObject = findObjectLOS((TestFunc) isWithHealthType, 0, aimPos, aimPos + delta2, t, n);
      enableCollision();

      // Skip this target if there's a friendly object in the way
      if(hitObject && hitObject->getTeam() == getTeam() &&
        (hitObject->getPos() - aimPos).lenSquared() < delta.lenSquared())         
         continue;

      F32 dist = delta.len();

      if(dist < bestRange)
      {
         bestDelta  = delta;
         bestRange  = dist;
         bestTarget = potential;
      }
   }

   if(!bestTarget)      // No target, nothing to do
      return;
 
   // Aim towards the best target.  Note that if the Mortar is at one extreme of its range, and the target is at the other,
   // then the Mortar will rotate the wrong-way around to aim at the target.  If we were to detect that condition here, and
   // constrain our Mortar to turning the correct direction, that would be great!!
   if(mFireTimer.getCurrent() == 0)
   {
      bestDelta.normalize();
      Point velocity;
         
      // String handling in C++ is such a mess!!!
      string killer = string("got blasted by ") + getGame()->getTeamName(getTeam()).getString() + " Mortar";
      mKillString = killer.c_str();

      GameWeapon::createWeaponProjectiles(WeaponType(mWeaponFireType), mAnchorNormal, aimPos, velocity,
                                          0, mWeaponFireType == WeaponBurst ? 45.f : 35.f, this);
      mFireTimer.reset(WeaponInfo::getWeaponInfo(mWeaponFireType).fireDelay);
   }
}


const char *Mortar::getOnScreenName()     const  { return "Mortar"; }
const char *Mortar::getOnDockName()       const  { return "Mortar"; }
const char *Mortar::getPrettyNamePlural() const  { return "Mortars"; }
const char *Mortar::getEditorHelpString() const  { return "Creates shooting Mortar.  Can be on a team, neutral, or \"hostile to all\". [Y]"; }


bool Mortar::hasTeam()      { return true; }
bool Mortar::canBeHostile() { return true; }
bool Mortar::canBeNeutral() { return true; }


void Mortar::onGeomChanged()
{ 
   Parent::onGeomChanged();



   Point normal = mAnchorNormal;
   normal.normalize();
   Point perpendicular = Point(normal.y, -normal.x);
   Point offset = normal;
   offset.normalize(MORTAR_OFFSET + 35);     // 35 determined by trial and error, only coincidentally equal to MORTAR_OFFSET

   Vector<Point> points;
   F32 size = 400;
   points.push_back(Point(getPos() + perpendicular * size));
   points.push_back(Point(getPos() + perpendicular * size + normal * 2 * size));
   points.push_back(Point(getPos() + perpendicular * -size + normal * 2 * size));
   points.push_back(Point(getPos() + perpendicular * -size));
   //points.push_back(Point(getPos() + offset));


   F32 radius = WeaponInfo::getWeaponInfo(WeaponSeeker).projVelocity / FloatPi;

   perpendicular.normalize(radius);
   Point center1 = getPos() + perpendicular + offset;
   Point center2 = getPos() - perpendicular + offset;

   Vector<Point> circle;

   generatePointsInACurve(0, FloatTau, 10 + 1, radius, circle);   // +1 so we can "close the loop"


   Vector<Vector<Point> > p, c, clipped;
   p.push_back(points);

   for(S32 i = 0; i < circle.size(); i++)
      circle[i] += center1;

   c.push_back(circle);

   for(S32 i = 0; i < circle.size(); i++)
      circle[i] += center2 - center1;

   c.push_back(circle);

   clipPolygons(ClipperLib::ctDifference, p, c, clipped, true);

   mZone.clear();

   for(S32 i = 0; i < clipped[0].size(); i++)
      mZone.push_back(clipped[0][i]);
}


/////
// Lua interface
/**
 * @luaclass Mortar
 * 
 * @brief Mounted gun that shoots at enemy ships and other objects.
 */
//               Fn name     Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, setWeapon,    ARRAYDEF({{ WEAP_ENUM, END }}), 1 ) \


GENERATE_LUA_METHODS_TABLE(Mortar, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(Mortar, LUA_METHODS);

#undef LUA_METHODS


const char *Mortar::luaClassName = "Mortar";
REGISTER_LUA_SUBCLASS(Mortar, EngineeredItem);


/**
 * @luafunc Mortar::setWeapon(Weapon weapon)
 *
 * @brief Sets the weapon for this Mortar to use.
 *
 * @param weapon Weapon to set on the Mortar
 *
 * @note This is experimental and may be removed or changed from the game at any time
 */
S32 Mortar::lua_setWeapon(lua_State *L)
{
   checkArgList(L, functionArgs, "Mortar", "setWeapon");

   mWeaponFireType = getWeaponType(L, 1);

   return 0;
}


// Override some methods
S32 Mortar::lua_getRad(lua_State *L)
{
   return returnFloat(L, MORTAR_OFFSET);
}


S32 Mortar::lua_getPos(lua_State *L)
{
   return returnPoint(L, getPos() + mAnchorNormal * MORTAR_OFFSET);
}

};

