//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "BfObject.h"

#include "gameConnection.h"
#include "game.h"
#include "ClientInfo.h"
#include "Level.h"
#include "moveObject.h"
#include "TeamConstants.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#  include "RenderUtils.h"    // For drawHollowSquare
#endif

#include "ServerGame.h"

#include "Colors.h"

#include "GeomUtils.h"
#include "MathUtils.h"           // For sq()
#include "stringUtils.h"         // For itos()

using namespace TNL;

namespace Zap
{
using namespace LuaArgs;

// Derived Object Type conditional methods
bool isEngineeredType(U8 x)
{
   return
         x == TurretTypeNumber || x == ForceFieldProjectorTypeNumber || x == MortarTypeNumber;
}


bool isShipType(U8 x)
{
   return
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber;
}


bool isProjectileType(U8 x)
{
   return
         x == MineTypeNumber  || x == SpyBugTypeNumber      || x == BulletTypeNumber ||
         x == BurstTypeNumber || x == SeekerTypeNumber;
}


bool isGrenadeType(U8 x)
{
   return
         x == MineTypeNumber || x == SpyBugTypeNumber || x == BurstTypeNumber;
}


// Ship::findRepairTargets uses this and expects everything to be a sub-class of Item (except for teleporter)
// This is used to determine if bursts should explode on impact or not.
bool isWithHealthType(U8 x)      
{
   return
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber           ||
         x == TurretTypeNumber     || x == ForceFieldProjectorTypeNumber ||
         x == CoreTypeNumber       || x == TeleporterTypeNumber          ||
         x == MortarTypeNumber;
}


bool isForceFieldDeactivatingType(U8 x)
{
   return
         x == MineTypeNumber         || x == SpyBugTypeNumber         ||
         x == FlagTypeNumber         || x == SoccerBallItemTypeNumber ||
         x == ResourceItemTypeNumber || x == TestItemTypeNumber       ||
         x == EnergyItemTypeNumber   || x == RepairItemTypeNumber     ||
         x == PlayerShipTypeNumber   || x == RobotShipTypeNumber      || 
         x == AsteroidTypeNumber;
}


bool isRadiusDamageAffectableType(U8 x)
{
   return
         x == PlayerShipTypeNumber   || x == RobotShipTypeNumber           || x == BurstTypeNumber      ||
         x == BulletTypeNumber       || x == MineTypeNumber                || x == SpyBugTypeNumber     ||
         x == ResourceItemTypeNumber || x == TestItemTypeNumber            || x == AsteroidTypeNumber   ||
         x == TurretTypeNumber       || x == ForceFieldProjectorTypeNumber || x == CoreTypeNumber       ||
         x == FlagTypeNumber         || x == SoccerBallItemTypeNumber      || x == TeleporterTypeNumber ||
         x == SeekerTypeNumber       || x == MortarTypeNumber;
}


bool isMotionTriggerType(U8 x)
{
   return
         x == PlayerShipTypeNumber   || x == RobotShipTypeNumber || x == SoccerBallItemTypeNumber ||
         x == ResourceItemTypeNumber || x == TestItemTypeNumber  || 
         x == AsteroidTypeNumber     || x == MineTypeNumber;
}


bool isTurretTargetType(U8 x)
{
   return
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber       || x == ResourceItemTypeNumber ||
         x == TestItemTypeNumber   || x == SoccerBallItemTypeNumber;
}


bool isCollideableType(U8 x)
{
   return
         x == BarrierTypeNumber || x == PolyWallTypeNumber   ||
         x == TurretTypeNumber  || x == ForceFieldTypeNumber ||
         x == CoreTypeNumber    || x == ForceFieldProjectorTypeNumber ||
         x == MortarTypeNumber;
}


bool isForceFieldCollideableType(U8 x)
{
   return
         x == BarrierTypeNumber || x == PolyWallTypeNumber ||
         x == TurretTypeNumber  || x == ForceFieldProjectorTypeNumber;
}


bool isWallType(U8 x)
{
   return
         x == BarrierTypeNumber  || x == PolyWallTypeNumber ||
         x == WallItemTypeNumber || x == WallEdgeTypeNumber || x == WallSegmentTypeNumber;
}


bool isWallOrForcefieldType(U8 x)
{
   return
      isWallType(x) || x == ForceFieldTypeNumber;
}


bool isWallItemType(U8 x)
{
   return x == WallItemTypeNumber;
}


bool isLineItemType(U8 x)
{
   return
         x == BarrierTypeNumber || x == WallItemTypeNumber || x == LineTypeNumber;
}


bool isWeaponCollideableType(U8 x)
{
   return
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber      || x == BurstTypeNumber               ||
         x == SpyBugTypeNumber     || x == MineTypeNumber           || x == BulletTypeNumber              ||
         x == FlagTypeNumber       || x == SoccerBallItemTypeNumber || x == ForceFieldProjectorTypeNumber ||
         x == AsteroidTypeNumber   || x == TestItemTypeNumber       || x == ResourceItemTypeNumber        ||
         x == TurretTypeNumber     || x == CoreTypeNumber           || x == BarrierTypeNumber             ||
         x == PolyWallTypeNumber   || x == ForceFieldTypeNumber     || x == TeleporterTypeNumber          ||
         x == SeekerTypeNumber     || x == MortarTypeNumber;
}

bool isAsteroidCollideableType(U8 x)
{
   return
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber           ||
         x == TestItemTypeNumber   || x == ResourceItemTypeNumber        ||
         x == TurretTypeNumber     || x == ForceFieldProjectorTypeNumber ||
         x == BarrierTypeNumber    || x == PolyWallTypeNumber            ||
         x == ForceFieldTypeNumber || x == CoreTypeNumber                ||
         x == MortarTypeNumber;
}

bool isFlagCollideableType(U8 x)
{
   return
         x == BarrierTypeNumber   || x == ForceFieldProjectorTypeNumber || x == ForceFieldTypeNumber || x == PolyWallTypeNumber;
}

bool isFlagOrShipCollideableType(U8 x)
{
   return
         x == BarrierTypeNumber    || x == PolyWallTypeNumber    || x == ForceFieldTypeNumber ||
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber;
}

bool isVisibleOnCmdrsMapType(U8 x)
{
   return
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber      || x == CoreTypeNumber                ||
         x == BarrierTypeNumber    || x == PolyWallTypeNumber       || x == TextItemTypeNumber            ||
         x == TurretTypeNumber     || x == ForceFieldTypeNumber     || x == ForceFieldProjectorTypeNumber ||
         x == FlagTypeNumber       || x == SoccerBallItemTypeNumber || x == LineTypeNumber                ||
         x == GoalZoneTypeNumber   || x == NexusTypeNumber          || x == LoadoutZoneTypeNumber         || 
         x == SpeedZoneTypeNumber  || x == TeleporterTypeNumber     || x == SlipZoneTypeNumber            ||
         x == AsteroidTypeNumber   || x == TestItemTypeNumber       || x == ResourceItemTypeNumber        ||
         x == EnergyItemTypeNumber || x == RepairItemTypeNumber     || x == MortarTypeNumber; 
}

bool isVisibleOnCmdrsMapWithSensorType(U8 x)     // Weapons visible on commander's map for sensor
{
   return
         x == PlayerShipTypeNumber || x == RobotShipTypeNumber      || x == ResourceItemTypeNumber        ||
         x == BarrierTypeNumber    || x == PolyWallTypeNumber       || x == LoadoutZoneTypeNumber         || 
         x == TurretTypeNumber     || x == ForceFieldTypeNumber     || x == ForceFieldProjectorTypeNumber ||
         x == FlagTypeNumber       || x == SoccerBallItemTypeNumber || x == SlipZoneTypeNumber            ||
         x == GoalZoneTypeNumber   || x == NexusTypeNumber          || x == CoreTypeNumber                ||
         x == SpeedZoneTypeNumber  || x == TeleporterTypeNumber     || x == BurstTypeNumber               ||
         x == LineTypeNumber       || x == TextItemTypeNumber       || x == RepairItemTypeNumber          ||
         x == AsteroidTypeNumber   || x == TestItemTypeNumber       || x == EnergyItemTypeNumber          ||
         x == BulletTypeNumber     || x == MineTypeNumber           || x == SeekerTypeNumber              ||
         x == MortarTypeNumber;
}


bool isZoneType(U8 x)      // Zones a ship could be in
{
   return 
         x == LoadoutZoneTypeNumber || x == GoalZoneTypeNumber   || x == NexusTypeNumber  ||
         x == ZoneTypeNumber        || x == SlipZoneTypeNumber;
}


bool isSeekerTarget(U8 x)
{
   return isShipType(x);
}


bool isMountableItemType(U8 x)
{
   return
         x == ResourceItemTypeNumber || x == FlagTypeNumber;
}


bool isAnyObjectType(U8 x)
{
   return true;
}

////////////////////////////////////////
////////////////////////////////////////


// Constructor
DamageInfo::DamageInfo()
{
   damageSelfMultiplier = 1;
   damageAmount = 0;
   damagingObject = NULL;
   damageType = DamageTypePoint;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
EditorObject::EditorObject()
{
   mInEditor = false;
   mLitUp = false; 
   mSelected = false; 
   mVertexLitUp = 0;
}


// Destructor
EditorObject::~EditorObject()
{
   // Do nothing
}


void EditorObject::onItemDragging()  { /* Do nothing */ }
void EditorObject::onAttrsChanging() { /* Do nothing */ }
void EditorObject::onAttrsChanged()  { /* Do nothing */ }


const char *EditorObject::getEditorHelpString() const
{
   TNLAssert(false, "getEditorHelpString method not implemented!");
   return "getEditorHelpString method not implemented!";  // better then a NULL crash in non-debug mode or continuing past the Assert
}


const char *EditorObject::getPrettyNamePlural() const
{
   TNLAssert(false, "getPrettyNamePlural method not implemented!");
   return "getPrettyNamePlural method not implemented!";
}


const char *EditorObject::getOnDockName() const
{
   TNLAssert(false, "getOnDockName method not implemented!");
   return "getOnDockName method not implemented!";
}


const char *EditorObject::getOnScreenName() const
{
   TNLAssert(false, "getOnScreenName method not implemented!");
   return "getOnScreenName method not implemented!";
}


// Not all editor objects will implement this
const char *EditorObject::getInstructionMsg(S32 attributeCount) const
{
   if(attributeCount > 0)
      return "[Enter] to edit attributes";

   return "";
}


void EditorObject::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   return;
}


S32 EditorObject::getDockRadius() const
{
   return 10;
}



bool EditorObject::isSelected() const
{
   return mSelected;
}


U32 EditorObject::getSelectedTime()
{
   return mSelectedTime;
}


void EditorObject::setSelected(bool selected)
{
   mSelected = selected;
   mSelectedTime = Platform::getRealMilliseconds();
}


bool EditorObject::isLitUp() const 
{ 
   return mLitUp; 
}


void EditorObject::setLitUp(bool litUp) 
{ 
   mLitUp = litUp; 

   if(!litUp) 
      setVertexLitUp(NONE); 
}


bool EditorObject::isVertexLitUp(S32 vertexIndex) const
{
   return mVertexLitUp == vertexIndex;
}


void EditorObject::setVertexLitUp(S32 vertexIndex)
{
   mVertexLitUp = vertexIndex;
}


void EditorObject::onAddedToEditor()
{
   mInEditor = true;
}


// Size of object in editor 
F32 EditorObject::getEditorRadius(F32 currentScale) const
{
   return 10 * currentScale;   // 10 pixels is base size
}


bool EditorObject::isInEditor() const
{
   return mInEditor;
}


////////////////////////////////////////
////////////////////////////////////////

// BfObject - the declarations are in GameObject.h


static S32 getNextDefaultId() 
{
   static S32 nextId = 0;
   nextId--;
   return nextId;
}


// Constructor
BfObject::BfObject()
{
   mGame = NULL;
   mObjectTypeNumber = UnknownTypeNumber;

   assignNewSerialNumber();
   assignNewUserAssignedId();

   mTeam = -1;
   mDisableCollisionCount = 0;
   mCreationTime = 0;

   mOwner = NULL;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
BfObject::~BfObject()
{
   // Restore type number so database can fully remove item.  In some cases, we change an item's type number to DeletedTypeNumber to 
   // prevent it from showing up in a database search.  This has the unfortunate side effect of also preventing it from being properly
   // removed from the database.  So, before we run removeFromDatabase, we'll check to see if the type number has been altered, and, if
   // so, we'll restore the original.  This is not a great solution to the problem, but works for now.
   if(mObjectTypeNumber == DeletedTypeNumber)
      mObjectTypeNumber = mOriginalTypeNumber;
   
   removeFromDatabase(false);
   mGame = NULL;
   LUAW_DESTRUCTOR_CLEANUP;
}


// BfObjects inherit from NetObjects, which delete themselves if their reference count hits 0.  Using this
// mechanism is safer than an outright delete, which will fail if there are more than one pointers to
// the "this" object.
void BfObject::deleteThyself()
{
   decRef();
}


void BfObject::assignNewUserAssignedId()
{
   setUserAssignedId(getNextDefaultId(), false);
}


// Serial numbers are used in a couple of ways: in the editor, they are used to identify same objects in different databases,
// for example to identify objects across undo/redo states.  They are also used by walls to help identify which segments belong
// to which wall, even as walls are being moved around, and wall edits are undone/redone.
void BfObject::assignNewSerialNumber()
{
   static S32 mNextSerialNumber = 0;

   mSerialNumber = mNextSerialNumber++;
}


S32 BfObject::getSerialNumber() const
{
   return mSerialNumber;
}


S32 BfObject::getTeam() const
{
   return mTeam;     // Team index, actually!
}


void BfObject::setTeam(S32 team)
{
   // Don't update clients if team has not changed
   if(team == mTeam)
      return;

   mTeam = team;
   setMaskBits(TeamMask);
}


// Lua helper methods -- these assume that the params have already been checked and are valid
void BfObject::setTeam(lua_State *L, S32 stackPos)
{
   setTeam(getTeamIndex(L, stackPos));
}


// Assumes that the params have already been checked and are valid
void BfObject::setPos(lua_State *L, S32 stackPos)
{
   setPos(getPointOrXY(L, stackPos));
}


// Overridden in children
bool BfObject::overlapsPoint(const Point &point) const
{
   return false;
}


// Function needed to provide this signature at this level
void BfObject::setPos(const Point &point)
{
   GeomObject::setPos(point);
}


void BfObject::setGeom(lua_State *L, S32 stackIndex)
{
   Vector<Point> points = getPointsOrXYs(L, stackIndex);

   // TODO: Q. Shouldn't we verify that the number of points here is appropriate for this object?   
   //       A. Yes!!

   S32 pointSize = points.size();
   GeomType geomType = getGeomType();

   // No points?  Do nothing!
   if(pointSize == 0)
      return;

   // Don't update geom if the new geom is the same
   bool hasChanged = false;

   if(geomType == geomPoint)
      hasChanged = points[0] != GeomObject::getPos();

   else  // geomSimpleLine, geomPolyLine, geomPolygon
   {
      // Quickie size check
      if(GeomObject::getOutline()->size() != pointSize)
         hasChanged = true;

      // Go through each point
      else
      {
         for(S32 i = 0; i < points.size(); i++)
         {
            if(points[i] != GeomObject::getOutline()->get(i))
            {
               hasChanged = true;
               break;
            }
         }
      }
   }

   // Silently return if geom hasn't changed
   if(!hasChanged)
      return;


   // Adjust geometry
   GeomObject::setGeom(points);

   // Tell this GeomObject/BfObject its geometry has changed
   onGeomChanged();
}


const Color &BfObject::getColor() const
{ 
   TNLAssert(getDatabase(), "Why do we need the color of an object not in a database?");
   TNLAssert(dynamic_cast<Level *>(getDatabase()), "Looks like this database is not a Level!");

   return static_cast<Level *>(getDatabase())->getTeamColor(getTeam());
}


Game *BfObject::getGame() const
{
   return mGame;
}


// These will all be overridden by various child classes
bool BfObject::hasTeam()            { return true; }
bool BfObject::canBeNeutral()       { return true; }
bool BfObject::canBeHostile()       { return true; }
bool BfObject::shouldRender() const { return true; }     


bool BfObject::canAddToEditor() { return true; }


void BfObject::addToGame(Game *game, GridDatabase *database)
{   
   TNLAssert(mGame == NULL, "Error: Object already in a game in BfObject::addToGame.");
   TNLAssert(game != NULL,  "Error: Adding to a NULL game in BfObject::addToGame.");

   mGame = game;
   if(database)
      addToDatabase(database);

   setCreationTime(game->getCurrentTime());
   onAddedToGame(game);

   if(game->isServer())
   {
      ServerGame *serverGame = static_cast<ServerGame *>(game);
      serverGame->onObjectAdded(this);
   }
}


// Removes object from game, but DOES NOT DELETE IT
void BfObject::removeFromGame(bool deleteObject)
{
   removeFromDatabase(deleteObject);
   if(!deleteObject)  // if "this" gets deleted inside removeFromDatabase(deleteObject == true), don't corrupt memory
      mGame = NULL;
}


bool BfObject::processArguments(S32 argc, const char **argv, Level *level)
{
   logprintf(LogConsumer::LogError, "Missing processArguments for %s", getClassName());
   return false;
}


// Make sure the database extents are in sync with where the object actually is
void BfObject::updateExtentInDatabase()
{
   setExtent(calcExtents());
}


void BfObject::unselect()
{
   setSelected(false);
   setLitUp(false);

   unselectVerts();
}


// Can be overriden by child objects, which should always call Parent::onGeomChanged()
void BfObject::onGeomChanged()
{
   // Notify our GeomObject parent of the geometry change
   GeomObject::onGeomChanged();

   updateExtentInDatabase();
   setMaskBits(GeomMask);
}


void BfObject::moveTo(const Point &pos, S32 snapVertex)
{  
   GeomObject::moveTo(pos, snapVertex);
   onGeomChanged();
}


// Item is being dragged around in the editor...
// Update their geometry so they will be visible in game-preview mode (tab key in the editor) while being dragged
void BfObject::onItemDragging()  { onGeomChanged(); }


#ifndef ZAP_DEDICATED
void BfObject::prepareForDock(const Point &point, S32 teamIndex)
{
   unselectVerts();
   setTeam(teamIndex);
   setExtent(calcExtents());    // Make sure the object's extents are properly set
}

#endif


#ifndef ZAP_DEDICATED
// Render selected and highlighted vertices, called from renderEditor
void BfObject::renderAndLabelHighlightedVertices(F32 currentScale)
{
   F32 radius = getEditorRadius(currentScale);

   // Label and highlight any selected or lit up vertices.  This will also highlight point items.
   for(S32 i = 0; i < getVertCount(); i++)
      if(vertSelected(i) || isVertexLitUp(i) || ((isSelected() || isLitUp())  && getVertCount() == 1))
      {
         const Color &color = (vertSelected(i) || (isSelected() && getGeomType() == geomPoint)) ? 
                                Colors::EDITOR_SELECT_COLOR : 
                                Colors::EDITOR_HIGHLIGHT_COLOR;

         Point center = getVert(i) + getEditorSelectionOffset(currentScale);

         RenderUtils::drawHollowSquare(center, radius / currentScale, color);
      }
}
#endif


Point BfObject::getDockLabelPos() const
{
   static const Point labelOffset(0, 11);

   return getPos() + labelOffset;
}


void BfObject::highlightDockItem() const
{
#ifndef ZAP_DEDICATED
   RenderUtils::drawHollowSquare(getPos(), (F32)getDockRadius(), Colors::EDITOR_HIGHLIGHT_COLOR);
#endif
}


void BfObject::initializeEditor()
{
   unselectVerts();
}


string BfObject::toLevelCode() const
{
   TNLAssert(false, "This object cannot be serialized");
   return "";
}


string BfObject::appendId(const string &objName) const
{
   if(mUserAssignedId <= 0)      // Ignore machine-assigned default ids
      return objName;

   return objName + "!" + itos(mUserAssignedId);
}


// Return a pointer to a new copy of the object.  This is more like a duplicate or twin of the object -- it has the same
// serial number, and is already assigned to a game.
// You will have to delete this copy when you are done with it!
BfObject *BfObject::copy()
{
   BfObject *newObject = clone();     
   newObject->initializeEditor();         // Marks all vertices as unselected

   return newObject;
}


// Return a pointer to a new copy of the object.  This copy will be completely new -- new serial number, mGame set to NULL, everything.
// You will have to delete this copy when you are done with it!
BfObject *BfObject::newCopy()
{
   BfObject *newObject = copy();
   newObject->mGame = NULL;

   newObject->assignNewSerialNumber();                      // Give this object an identity of its own
   newObject->assignNewUserAssignedId(); // Make sure we don't end up with duplicate IDs!

   return newObject;
}


BfObject *BfObject::clone() const
{
   TNLAssert(false, "Clone method not implemented!");
   return NULL;
}


void BfObject::setSnapped(bool snapped)
{
   // Do nothing
}


// Called when item dragged from dock to editor -- overridden by several objects
void BfObject::newObjectFromDock(F32 gridSize) 
{  
   assignNewSerialNumber();

   updateExtentInDatabase();
   mGame = NULL;
}   


Point BfObject::getEditorSelectionOffset(F32 scale)
{
   return Point(0,0);     // No offset for most items
}


Point BfObject::getInitialPlacementOffset(U32 gridSize) const
{
   return Point(0,0);
}


void BfObject::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices) const
{
   TNLAssert(false, "renderEditor not implemented!");
}


void BfObject::renderDock(const Color &color) const
{
   TNLAssert(false, "renderDock not implemented!");
}


// For editing attributes -- all implementation will need to be provided by the children
bool                   BfObject::startEditingAttrs(EditorAttributeMenuUI *attributeMenu) { return false; }
void                   BfObject::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)  { /* Do nothing */ }


bool BfObject::controllingClientIsValid()
{
   return mControllingClient.isValid();
}


SafePtr<GameConnection> BfObject::getControllingClient()
{
   return mControllingClient;
}


void BfObject::setControllingClient(GameConnection *c)         // This only gets run on the server
{
   mControllingClient = c;
}


void BfObject::setOwner(ClientInfo *clientInfo)
{
   mOwner = clientInfo;
}


ClientInfo *BfObject::getOwner()
{
   return mOwner;
}


void BfObject::deleteObject(U32 deleteTimeInterval)  // interval defaults to 0
{
   if(mObjectTypeNumber == DeletedTypeNumber)
      return;

   mOriginalTypeNumber = mObjectTypeNumber;
   mObjectTypeNumber = DeletedTypeNumber;

   if(!mGame)                    // Not in a game
      delete this;
   else
      mGame->addToDeleteList(this, deleteTimeInterval);
}


// Passing 0 will have no effect on existing id
void BfObject::setUserAssignedId(S32 id, bool permitZero)
{
   if(permitZero || id != 0)
      mUserAssignedId = id;
}


S32 BfObject::getUserAssignedId()
{
   return mUserAssignedId;
}


void BfObject::setScopeAlways()
{
   getGame()->setScopeAlwaysObject(this);
}


bool BfObject::isVisibleToTeam(S32 teamIndex) const
{
   return true;      // By default, all teams can see all objects
}


F32 BfObject::getUpdatePriority(GhostConnection *connection, U32 updateMask, S32 updateSkips)
{
   GameConnection *gc = dynamic_cast<GameConnection *>(connection);
   BfObject *so = gc ? gc->getControlObject() : NULL;
   F32 add = 0;
   if(so)
   {
      Point center = so->getExtent().getCenter();

      Point nearest;
      const Rect &extent = getExtent();

      if(center.x < extent.min.x)
         nearest.x = extent.min.x;
      else if(center.x > extent.max.x)
         nearest.x = extent.max.x;
      else
         nearest.x = center.x;

      if(center.y < extent.min.y)
         nearest.y = extent.min.y;
      else if(center.y > extent.max.y)
         nearest.y = extent.max.y;
      else
      nearest.y = center.y;

      Point deltap = nearest - center;

      F32 distance = (nearest - center).len();

      Point deltav = getVel() - so->getVel();


      // initial scoping factor is distance based.
      add += (500 - distance) / 500;

      // give some extra love to things that are moving towards the scope object
      if(deltav.dot(deltap) < 0)
         add += 0.7f;
   }

   // and a little more love if this object has not yet been scoped.
   if(updateMask == 0xFFFFFFFF)
      add += 2.5;
   return add + updateSkips * 0.2f;
}


void BfObject::damageObject(DamageInfo *theInfo)
{
   // Do nothing
}


bool BfObject::collide(BfObject *hitObject)                  { return false; }
bool BfObject::collided(BfObject *hitObject, U32 stateIndex) { return false; }


Vector<Point> BfObject::getRepairLocations(const Point &repairOrigin)
{
   Vector<Point> repairLocations;
   repairLocations.push_back(getPos());

   return repairLocations;
}


// This method returns true if the specified object collides with the given ray designated by
// rayStart and rayEnd
bool BfObject::objectIntersectsSegment(BfObject *object, const Point &rayStart, const Point &rayEnd,
                                       F32 &fillCollisionTime)
{
   F32 collisionTime = 1.f;

   Point targetLocation;
   F32 targetRadius;

   // If our target has a collision circle
   if(object->getCollisionCircle(ActualState, targetLocation, targetRadius))
   {
      if(circleIntersectsSegment(targetLocation, targetRadius, rayStart, rayEnd, fillCollisionTime))
         return(fillCollisionTime < collisionTime);   // If we're super close, we've hit!

      return false;
   }

   const Vector<Point> *fillPolygon = object->getCollisionPoly();

   if(fillPolygon && fillPolygon->size() > 0)      // Our target has a collision polygon
   {
      Point normal;
      if(polygonIntersectsSegmentDetailed(&fillPolygon->first(), fillPolygon->size(), true, rayStart, rayEnd, fillCollisionTime, normal))
         return(fillCollisionTime < collisionTime);
   }

   return false;
}


// Returns number of ships hit
S32 BfObject::radiusDamage(Point pos, S32 innerRad, S32 outerRad, TestFunc objectTypeTest, DamageInfo &info, F32 force)
{
   // Check for players within range.  If so, blast them to little tiny bits!
   // Those within innerRad get full force of the damage.  Those within outerRad get damage proportional to distance.
   Rect queryRect(pos, pos);
   queryRect.expand(Point(outerRad, outerRad));

   fillVector.clear();
   findObjects(objectTypeTest, fillVector, queryRect);

   // No damage calculated on the client
   if(isClient())
      info.damageAmount = 0;

   S32 shipsHit = 0;

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *foundObject = static_cast<BfObject *>(fillVector[i]);

      // No object damages itself
      if(foundObject == info.damagingObject)
         continue;

      // Check the actual distance against our outer radius.  Recall that we got a list of potential
      // collision objects based on a square area, but actual collisions will be based on true distance.
      Point objPos = foundObject->getPos();
      Point delta = objPos - pos;

      if(delta.lenSquared() > sq(outerRad))
         continue;

      // Check if this pair of objects can damage one another
      if(!getGame()->objectCanDamageObject(info.damagingObject, foundObject))
         continue;

      // Do an LOS check...
      F32 t;
      Point n;

      // No damage through walls or forcefields
      if(findObjectLOS((TestFunc)isWallType, ActualState, pos, objPos, t, n))
         continue;

      // Figure the impulse and damage
      DamageInfo localInfo = info;

      // Figure collision forces...
      localInfo.impulseVector  = delta;
      localInfo.impulseVector.normalize();

      //localInfo.collisionPoint  = objPos;
      localInfo.collisionPoint -= info.impulseVector;

      // Reuse t from above to represent interpolation based on distance
      F32 dist = delta.len();
      if(dist < innerRad)           // Inner radius gets full force of blast
         t = 1.f;
      else                          // But if we're further away, force is attenuated
         t = 1.f - (dist - innerRad) / (outerRad - innerRad);

      // Attenuate impulseVector and damageAmount
      localInfo.impulseVector  *= force * t;
      localInfo.damageAmount   *= t;

      // Adjust for self-damage
      ClientInfo *damagerOwner = info.damagingObject->getOwner();
      ClientInfo *victimOwner = foundObject->getOwner();

      if(victimOwner && damagerOwner == victimOwner)
         localInfo.damageAmount *= localInfo.damageSelfMultiplier;

      if(isShipType(foundObject->getObjectTypeNumber()))
         shipsHit++;

      foundObject->damageObject(&localInfo); 
   }

   return shipsHit;
}


void BfObject::findObjects(TestFunc objectTypeTest, Vector<DatabaseObject *> &fillVector, const Rect &ext) const
{
   GridDatabase *gridDB = getDatabase();

   if(gridDB)
      gridDB->findObjects(objectTypeTest, fillVector, ext);
}


void BfObject::findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector, const Rect &ext) const
{
   GridDatabase *gridDB = getDatabase();

   if(gridDB)
      gridDB->findObjects(typeNumber, fillVector, ext);
}


BfObject *BfObject::findObjectLOS(U8 typeNumber, U32 stateIndex, const Point &rayStart, const Point &rayEnd,
                                      float &collisionTime, Point &collisionNormal) const
{
   GridDatabase *gridDB = getDatabase();

   if(gridDB)
     return static_cast<BfObject *>(
         gridDB->findObjectLOS(typeNumber, stateIndex, rayStart, rayEnd, collisionTime, collisionNormal)
         );

   // TODO: Probably need to check level::wallEdgeDatabase as well

   return NULL;
}


BfObject *BfObject::findObjectLOS(TestFunc objectTypeTest, U32 stateIndex, const Point &rayStart, const Point &rayEnd,
                                      float &collisionTime, Point &collisionNormal) const
{
   GridDatabase *gridDB = getDatabase();

   if(gridDB)
     return static_cast<BfObject *>(
         gridDB->findObjectLOS(objectTypeTest, stateIndex, rayStart, rayEnd, collisionTime, collisionNormal)
         );

   // TODO: Probably need to check level::wallEdgeDatabase as well

   return NULL;
}


void BfObject::onAddedToGame(Game *game)
{
   // Do nothing
}


void BfObject::markAsGhost()
{
   mNetFlags = NetObject::IsGhost;
}


bool BfObject::isMoveObject()
{
   return false;
}


Point BfObject::getVel() const
{
   return Point(0,0);
}


U32 BfObject::getCreationTime() const
{
   return mCreationTime;
}


void BfObject::setCreationTime(U32 creationTime)
{
   mCreationTime = creationTime;
}


StringTableEntry BfObject::getKillString()
{
   return mKillString;
}


S32 BfObject::getRenderSortValue()
{
   return 2;
}


const Move &BfObject::getCurrentMove()
{
   return mCurrentMove;
}


const Move &BfObject::getLastMove()
{
   return mPrevMove;
}


void BfObject::setCurrentMove(const Move &move)
{
   mCurrentMove = move;
}


void BfObject::setPrevMove(const Move &move)
{
   mPrevMove = move;
}


void BfObject::render() const
{
   // Do nothing
}


void BfObject::renderLayer(S32 layerIndex)
{
   if(layerIndex == 1)
      render();
}


void BfObject::disableCollision()
{
   TNLAssert(mDisableCollisionCount < 10, "Too many disabled collisions");
   mDisableCollisionCount++;
}


void BfObject::enableCollision()
{
   TNLAssert(mDisableCollisionCount != 0, "Trying to enable collision, already enabled");
   mDisableCollisionCount--;
}


bool BfObject::isCollisionEnabled() const
{
   return mDisableCollisionCount == 0;
}


// Find if the specified polygon intersects theObject's collisionPoly or collisonCircle
bool BfObject::collisionPolyPointIntersect(Point center, F32 radius)
{
   Point c;
   const Vector<Point> *polyPoints = getCollisionPoly();

   if(polyPoints && polyPoints->size())
      return polygonCircleIntersect(&polyPoints->first(), polyPoints->size(), center, radius * radius, c);

   F32 r;

   if(getCollisionCircle(ActualState, c, r))
      return ( center.distSquared(c) < (radius + r) * (radius + r) );

   return false;
}


F32 BfObject::getHealth() const
{
   return 1;
}


bool BfObject::isDestroyed()
{
   return false;
}


void BfObject::idle(IdleCallPath path)
{
   // Do nothing
}


void BfObject::writeControlState(BitStream *)
{
   // Do nothing
}


void BfObject::readControlState(BitStream *)
{
   // Do nothing
}


void BfObject::controlMoveReplayComplete()
{
   // Do nothing
}


void BfObject::writeCompressedVelocity(const Point &vel, U32 max, BitStream *stream)
{
   U32 len = U32(vel.len());

   // Write a flag designating 0; 0 is 0, rounding errors highly undesireable
   if(stream->writeFlag(len == 0))
      return;

   // Write actual x and y components as floats
   if(stream->writeFlag(len > max))
   {
      stream->write(vel.x);
      stream->write(vel.y);
   }
   else
   {
      // Write a length and angle
      F32 theta = atan2(vel.y, vel.x);

      stream->writeSignedFloat(theta * FloatInverse2Pi, 10);
      stream->writeRangedU32(len, 0, max);
   }
}


void BfObject::readCompressedVelocity(Point &vel, U32 max, BitStream *stream)
{
   if(stream->readFlag())
   {
      vel.x = vel.y = 0;
      return;
   }
   else if(stream->readFlag())
   {
      stream->read(&vel.x);
      stream->read(&vel.y);
   }
   else
   {
      F32 theta = stream->readSignedFloat(10) * Float2Pi;
      F32 magnitude = (F32)stream->readRangedU32(0, max);
      vel.set(cos(theta) * magnitude, sin(theta) * magnitude);
   }
}


void BfObject::onGhostAddBeforeUpdate(GhostConnection *theConnection)
{
#ifndef ZAP_DEDICATED
   // Some unpackUpdate need getGame() available.
   GameConnection *gc = (GameConnection *)(theConnection);  // GhostConnection is always GameConnection
   TNLAssert(theConnection && gc->getClientGame(), "Should only be client here!");
   mGame = gc->getClientGame();
#endif
}


// Overrides method in tnlNetObject.
// onGhostAdd is called on the client side of a connection after
// the constructor and after the first call to unpackUpdate (the
// initial call).  Returning true signifies no error - returning
// false causes the connection to abort.
bool BfObject::onGhostAdd(GhostConnection *theConnection)
{
#ifndef ZAP_DEDICATED
   GameConnection *gc = (GameConnection *)(theConnection);  // GhostConnection is always GameConnection
   TNLAssert(theConnection && gc->getClientGame(), "Should only be client here!");

#ifdef TNL_ENABLE_ASSERTS
   mGame = NULL;  // prevent false asserts
#endif

   // for performance, add to GridDatabase after update, to avoid slowdown from adding to database with zero points or (0,0) then moving
   addToGame(gc->getClientGame(), gc->getClientGame()->getLevel());
#endif
   return true;
}


const Vector<Point> *BfObject::getEditorHitPoly() const
{
   return getOutline();
}


static const U8 TeamBits = 4;       // 4 bits = 16, we have 9 + 2 teams... so it fits!
static const U8 TeamOffset = 2;     // To account for Neutral and Hostile teams

void BfObject::readThisTeam(BitStream *stream)
{
   mTeam = stream->readInt(TeamBits) - TeamOffset;
}


void BfObject::writeThisTeam(BitStream *stream)
{
   stream->writeInt(mTeam + TeamOffset, TeamBits);
}


/////
// Lua interface
//               Fn name         Param profiles     Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getClassId,     ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, getObjType,     ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, getId,          ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, setId,          ARRAYDEF({{ INT,       END }               }), 1 ) \
   METHOD(CLASS, getLoc,         ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, setLoc,         ARRAYDEF({{ PT,        END }               }), 1 ) \
   METHOD(CLASS, getPos,         ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, setPos,         ARRAYDEF({{ PT,        END }               }), 1 ) \
   METHOD(CLASS, getTeamIndx,    ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, getTeamIndex,   ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, setTeam,        ARRAYDEF({{ TEAM_INDX, END }               }), 1 ) \
   METHOD(CLASS, removeFromGame, ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, setGeom,        ARRAYDEF({{ PT,        END }, { GEOM, END }}), 2 ) \
   METHOD(CLASS, getGeom,        ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, clone,          ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, isSelected,     ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, setSelected,    ARRAYDEF({{ BOOL,      END }               }), 1 ) \
   METHOD(CLASS, getOwner,       ARRAYDEF({{            END }               }), 1 ) \
   METHOD(CLASS, setOwner,       ARRAYDEF({{ STR,       END }               }), 1 ) \

GENERATE_LUA_METHODS_TABLE(BfObject, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(BfObject, LUA_METHODS);

#undef LUA_METHODS


const char *BfObject::luaClassName = "BfObject";
REGISTER_LUA_CLASS(BfObject);


/**
 * @luafunc ObjType BfObject::getObjType()
 * 
 * @brief Gets an object's \ref ObjTypeEnum.
 * 
 * @code
 *   obj = TestItem.new()
 *   -- prints 'true'
 *   print(obj:getObjType() == ObjType.TestItem)
 * @endcode
 *
 * See \ref ObjTypeEnum for a list of possible return values.
 * 
 * @return The object's ObjType.
 */
S32 BfObject::lua_getObjType(lua_State *L)  
{ 
   return returnInt(L, mObjectTypeNumber); 
}

/**
 * @luafunc ObjType BfObject::getClassId()
 * 
 * @deprecated Use getObjType()
 */
S32 BfObject::lua_getClassId(lua_State *L)  
{ 
   logprintf(LogConsumer::LuaBotMessage, "'getClassId()' is deprecated and will be removed in the future.  Use 'getObjType()', instead");
   return lua_getObjType(L); 
}


/**
 * @luafunc int BfObject::getId()
 * 
 * @brief Gets an object's user assigned id.
 * 
 * @descr Users can assign an id to elements in the editor with the ! or # keys.
 * Use this function to obtain this id. If the user has not assigned an object
 * an id, getId() will return a negative id that will remain consistent throught
 * the game.
 * 
 * @return The object's id.
 */
S32 BfObject::lua_getId(lua_State *L)  
{ 
   return returnInt(L, mUserAssignedId); 
}


/**
 * @luafunc BfObject::setId()
 * 
 * @brief Sets an object's user assigned id.
 * 
 * @descr Users can assign an id to elements in the editor with the ! or # keys.
 * Use this function to set this id from Lua. When called from an editor plugin,
 * the value passed will be displayed in the editor when the player presses ! or
 * #.
 * */
S32 BfObject::lua_setId(lua_State *L)  
{ 
   checkArgList(L, functionArgs, "BfObject", "setId");
   mUserAssignedId = getInt(L, 1);
   return 0;            
}


/**
 * @luafunc Point BfObject::getPos()
 * 
 * @brief Gets an object's position.
 * 
 * @descr For objects that are not points (such as a LoadoutZone), will return
 * the object's centroid.
 * 
 * @return A Point representing the object's position.
 */
S32 BfObject::lua_getPos(lua_State *L)
{ 
   return returnPoint(L, getPos()); 
}


/**
 * @luafunc Point BfObject::getLoc()
 *
 * @deprecated Use getPos() instead.
 */
S32 BfObject::lua_getLoc(lua_State *L)
{ 
   logprintf(LogConsumer::LuaBotMessage, "'getLoc()' is deprecated and will be removed in the future.  Use 'getPos()', instead");
   return lua_getPos(L); 
}

// TODO Remove after 019
/**
 * @luafunc int BfObject::getTeamIndx()
 * 
 * @brief See BfObject::getTeamIndex()
 * 
 * @deprecated This method is deprecated and will be removed
 * 
 * @descr Use BfObject::getTeamIndex() instead. This method will be removed in
 * the future
 * 
 * @return Team index of the object.
 */
S32 BfObject::lua_getTeamIndx(lua_State *L)
{
   logprintf(LogConsumer::LuaBotMessage, "'getTeamIndx()' is deprecated and will be removed in the future.  Use 'getTeamIndex()', with an 'e', instead");

   return lua_getTeamIndex(L);
}


/**
 * @luafunc int BfObject::getTeamIndex()
 * 
 * @brief Gets the index of the object's team.
 * 
 * @descr Many BfObjects (such as \link TestItem TestItems\endlink) are never
 * part of any particular team. For these objects, this method will return the
 * Neutral Team index
 * 
 * @note Remember that in Lua, indices start with 1!
 * 
 * @return Team index of the object.
 */
S32 BfObject::lua_getTeamIndex(lua_State *L)
{
   return returnTeamIndex(L, mTeam);
}


/**
 * @luafunc BfObject::setTeam(int teamIndex)
 * 
 * @brief Assigns the object to a team.
 * 
 * @param teamIndex Index of the team the object should be assigned to.
 * 
 * @descr Use the special team constants to make an item neutral or hostile.
 * Will have no effect on items that are inherently teamless (such as a
 * NexusZone).
 * 
 * @note Remember that in Lua, indices start with 1!
 */
S32 BfObject::lua_setTeam(lua_State *L) 
{ 
   checkArgList(L, functionArgs, "BfObject", "setTeam");
   setTeam(L, 1);
   return 0;            
}  


/**
 * @luafunc BfObject::setPos(Point pos)
 * 
 * @brief Set the object's position.
 * 
 * @descr To set the full geometry of a more complex object, see the setGeom()
 * method.
 * 
 * @param pos The new position of the object. 
 */
S32 BfObject::lua_setPos(lua_State *L)
{
   checkArgList(L, functionArgs, "BfObject", "setPos");
   setPos(L, 1);
   return 0;
}


/**
 * @luafunc BfObject::setLoc(Point pos)
 *
 * @deprecated Use setPos() instead.
 */
S32 BfObject::lua_setLoc(lua_State *L)
{
   logprintf(LogConsumer::LuaBotMessage, "'setLoc()' is deprecated and will be removed in the future.  Use 'setPos()', instead");
   return lua_setPos(L);
}


/**
 * @luafunc BfObject::removeFromGame()
 * 
 * @brief Removes the object from the current game or editor session.
 * 
 * @descr May not be implemented for all objects.
 */
S32 BfObject::lua_removeFromGame(lua_State *L)
{
   removeFromGame(true);
   return 0;
}


/**
 * @luafunc BfObject::setGeom(Geom geometry)
 * 
 * @brief Sets an object's geometry. 
 * 
 * @param geometry The object's new geometry.
 * 
 * @descr Note that not all objects support changing geometry if the object has
 * already been added to a game.
 */
S32 BfObject::lua_setGeom(lua_State *L)
{
   checkArgList(L, functionArgs, "BfObject", "setGeom");

   setGeom(L, 1);

   return 0;
}

/**
 * @luafunc Geom BfObject::getGeom()
 * 
 * @brief Returns an object's geometry. 
 * 
 * @return A geometry as described on the Geom page
 */
S32 BfObject::lua_getGeom(lua_State *L)
{
   // Simple geometry
   if(getGeomType() == geomPoint)
      return returnPoint(L, GeomObject::getPos());

   // Complex geometry
   return returnPoints(L, GeomObject::getOutline());
}


/**
 * @luafunc BfObject BfObject::clone()
 * 
 * @brief Make an exact duplicate of an object.
 * 
 * @descr Returned object will not be added to the current game, and will have a
 * different id than the source object.
 * 
 * @return Returns the new clone of the object.
 *
 * @note This function is not yet implemented.
 */
S32 BfObject::lua_clone(lua_State *L)
{
   TNLAssert(false, "Not yet implemented!!!");
   return 0;      // TODO: return clone -- how do we prevent leaks here?
}


/**
 * @luafunc bool BfObject::isSelected()
 * 
 * @brief Determine if an object is selected in the editor.
 * 
 * @descr This is useful for editor plugins only.
 * 
 * @return Returns `true` if the object is selected, `false` if not.
 */
S32 BfObject::lua_isSelected(lua_State *L)
{
   return returnBool(L, isSelected());
}


/**
 * @luafunc BfObject::setSelected(bool selected)
 * 
 * @brief Set whether an object is selected in the editor.
 * 
 * @descr This is useful for editor plugins only.
 * 
 * @param selected `true` to select the object, `false` to deselect it.
 */
S32 BfObject::lua_setSelected(lua_State *L)
{
   checkArgList(L, functionArgs, "BfObject", "setSelected");

   setSelected(getBool(L, 1));

   return 0;
}


/**
 * @luafunc LuaPlayerInfo BfObject::getOwner()
 * 
 * @brief Gets an object's owner as a LuaPlayerInfo.
 * 
 * @descr Some objects (like projectiles) have an owning player associated. This
 * method returns a LuaPlayerInfo object if there is an owner. Otherwise, returns
 * nil.
 * 
 * @return A LuaPlayerInfo representing the object's owner, or nil.
 */
S32 BfObject::lua_getOwner(lua_State *L)
{
   if(mOwner.isNull())
      return returnNil(L);

   return returnPlayerInfo(L, mOwner->getPlayerInfo());
}


/**
 * @luafunc BfObject::setOwner(string playerName)
 * 
 * @brief Sets the owner of the object.
 * 
 * @param playerName Name of player as a string.
 * 
 * @note This method only works if the item in question has already been added
 * to the game via addItem(object).  The owner cannot be set beforehand. Also,
 * `playerName` must exactly match a the name of a player already in the game
 * (case-sensitive).
 */
S32 BfObject::lua_setOwner(lua_State *L)
{
   checkArgList(L, functionArgs, luaClassName, "setOwner");

   const char *playerName = getString(L, 1);

   // This is NULL if the owner was set *before* adding this object to the game
   if(mGame == NULL)
   {
      logprintf(LogConsumer::LuaBotMessage, "You cannot call setOwner() on an object before it is added to the game.");
      return 0;
   }

   ClientInfo *clientInfo = mGame->findClientInfo(StringTableEntry(playerName));

   if(clientInfo == NULL)  // Player not found
      return 0;

   mOwner = clientInfo;

   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

// Destructor
CentroidObject::~CentroidObject()
{
   // Do nothing
}


// 2D objects need special handling when getting/setting location
S32 CentroidObject::lua_getLoc(lua_State *L)
{
   logprintf(LogConsumer::LuaBotMessage, "'getLoc()' is deprecated and will be removed in the future.  Use 'getPos()', instead");
   return lua_getPos(L);
}


S32 CentroidObject::lua_setLoc(lua_State *L)
{
   logprintf(LogConsumer::LuaBotMessage, "'setLoc()' is deprecated and will be removed in the future.  Use 'setPos()', instead");
   return lua_setPos(L);
}


S32 CentroidObject::lua_getPos(lua_State *L)
{
   return returnPoint(L, getCentroid());      // Do we want this to return a series of points?
}


S32 CentroidObject::lua_setPos(lua_State *L)
{
   checkArgList(L, functionArgs, "BfObject", "setLoc");

   offset(getPointOrXY(L, 1) - getCentroid());

   return 0;
}
};

