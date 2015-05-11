//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "CoreGame.h"

#include "Level.h"
#include "SoundSystem.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#  include "RenderUtils.h"
#  include "GameObjectRender.h"
#  include "UIQuickMenu.h"
#endif

#include "Colors.h"

#include "stringUtils.h"

#include <cmath>

namespace Zap {

using namespace LuaArgs;


CoreGameType::CoreGameType() : GameType(0)  // Winning score hard-coded to 0
{
   // Do nothing
}


CoreGameType::~CoreGameType()
{
   // Do nothing
}


bool CoreGameType::processArguments(S32 argc, const char **argv, Level *level)
{
   if(argc > 0)
      setGameTime(F32(atof(argv[0]) * 60.0));      // Game time, stored in minutes in level file

   return true;
}


string CoreGameType::toLevelCode() const
{
   return string(getClassName()) + " " + getRemainingGameTimeInMinutesString();
}


// Runs on client
void CoreGameType::renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const
{
#ifndef ZAP_DEDICATED
   Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);

   Ship *ship = getGame()->getLocalPlayerShip();

   if(!ship)
      return;

   for(S32 i = mCores.size() - 1; i >= 0; i--)
   {
      CoreItem *coreItem = mCores[i];
      if(coreItem)  // Core may have been destroyed
         if(coreItem->getTeam() != ship->getTeam())
            renderObjectiveArrow(coreItem, canvasWidth, canvasHeight);
   }
#endif
}


void CoreGameType::idle(BfObject::IdleCallPath path, U32 deltaT)
{
   Parent::idle(path, deltaT);

   if(isServer()) 
      if(isOvertime())
      {
         static const F32 OvertimeDegradeRate = 0.01f;      // 1 % per second
         for(S32 i = 0; i < mCores.size(); i++)
            mCores[i]->degradeAllPanels(OvertimeDegradeRate * deltaT / 1000);
      }
}


bool CoreGameType::isTeamCoreBeingAttacked(S32 teamIndex) const
{
   for(S32 i = mCores.size() - 1; i >= 0; i--)
   {
      CoreItem *coreItem = mCores[i];

      if(coreItem && coreItem->getTeam() == teamIndex)
         if(coreItem->isBeingAttacked())
            return true;
   }

   return false;
}


#ifndef ZAP_DEDICATED

Vector<string> CoreGameType::makeParameterMenuKeys() const
{
   // Start with the keys from our parent (GameType)
   Vector<string> items = *Parent::getGameParameterMenuKeys();

   // Remove "Win Score" as that's not needed here -- win score is determined by the number of cores
   S32 index = items.getIndex("Win Score");
   TNLAssert(index >= 0, "Invalid index!");     // Protect against text of Win Score being changed in parent

   items.erase(index);

   return items;
}


const Vector<string> *CoreGameType::getGameParameterMenuKeys() const
{
   static const Vector<string> keys = makeParameterMenuKeys();
   return &keys;
}
#endif


void CoreGameType::addCore(CoreItem *core, S32 teamIndex)
{
   mCores.push_back(core);

   if(!core->isGhost() && U32(teamIndex) < U32(getGame()->getTeamCount()) && getGame()->isServer()) // No EditorTeam
   {
      Team * team = dynamic_cast<Team *>(getGame()->getTeam(teamIndex));
      TNLAssert(team, "Bad team pointer or bad type");
      team->addScore(1);
      s2cSetTeamScore(teamIndex, team->getScore());
   }
}


// Dont't need to handle scores here; will be handled elsewhere
void CoreGameType::removeCore(CoreItem *core)
{
   S32 index = mCores.getIndex(core);
   if(index > -1)
      mCores.erase_fast(index);
}


// Overrides GameType function
void CoreGameType::updateScore(ClientInfo *player, S32 team, ScoringEvent event, S32 data)
{
   if(isGameOver()) // Game play ended, no changing score
      return;

   if(player != NULL)  // Individual scores is only for game reports statistics not seen during game play
   {
      S32 points = getEventScore(IndividualScore, event, data);
      TNLAssert(points != naScore, "Bad score value");
      player->addScore(points);
   }

   if((event == OwnCoreDestroyed || event == EnemyCoreDestroyed) && U32(team) < U32(getGame()->getTeamCount()))
   {
      getGame()->getTeam(team)->addScore(-1);      // Count down when a core is destoryed
      S32 numberOfTeamsHaveSomeCores = 0;

      s2cSetTeamScore(team, getGame()->getTeam(team)->getScore());     // Broadcast result

      for(S32 i = 0; i < getGame()->getTeamCount(); i++)
         if(getGame()->getTeam(i)->getScore() != 0)
            numberOfTeamsHaveSomeCores++;

      if(numberOfTeamsHaveSomeCores <= 1)
         gameOverManGameOver();
   }
}


S32 CoreGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data)
{
   if(scoreGroup == TeamScore)
   {
      return naScore;  // We never use TeamScore in CoreGameType
   }
   else  // scoreGroup == IndividualScore
   {
      switch(scoreEvent)
      {
         case KillEnemy:
            return 1;
         case KilledByAsteroid:  // Fall through OK
         case KilledByTurret:    // Fall through OK
         case KillSelf:
            return -1;
         case KillTeammate:
            return 0;
         case KillEnemyTurret:
            return 1;
         case KillOwnTurret:
            return -1;
         case OwnCoreDestroyed:
            return -5 * data;
         case EnemyCoreDestroyed:
            return 5 * data;
         default:
            return naScore;
      }
   }
}


void CoreGameType::score(ClientInfo *destroyer, S32 coreOwningTeam, S32 score)
{
   Vector<StringTableEntry> e;

   if(destroyer)
   {
      e.push_back(destroyer->getName());
      e.push_back(getGame()->getTeamName(coreOwningTeam));

      // If someone destroyed enemy core
      if(destroyer->getTeamIndex() != coreOwningTeam)
      {
         static StringTableEntry capString("%e0 destroyed a %e1 Core!");
         broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, capString, e);

         updateScore(NULL, coreOwningTeam, EnemyCoreDestroyed, score);
      }
      else
      {
         static StringTableEntry capString("%e0 destroyed own %e1 Core!");
         broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, capString, e);

         updateScore(NULL, coreOwningTeam, OwnCoreDestroyed, score);
      }
   }
   else     // No or unknown destroyer
   {
      static StringTableEntry killString("Something destroyed a %e0 Core!");

      e.push_back(getGame()->getTeamName(coreOwningTeam));
      
      broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, killString, e);

      updateScore(NULL, coreOwningTeam, EnemyCoreDestroyed, score);
   }
}


// In Core games, we'll enter sudden death... next score wins
void CoreGameType::onOvertimeStarted()
{
   startSuddenDeath();

   if(isClient())
   {
      // Augment messages shown by startSuddenDeath()
      getGame()->emitDelayedTextEffect(1500, "CORES WEAKEN!", Colors::red, Point(0,0), false);
   }
}


GameTypeId CoreGameType::getGameTypeId() const { return CoreGame; }

const char *CoreGameType::getShortName() const { return "Core"; }

static const char *instructions[] = { "Destroy enemy Cores",  0 };
const char **CoreGameType::getInstructionString() const { return instructions; }
HelpItem CoreGameType::getGameStartInlineHelpItem() const { return CoreGameStartItem; }


bool CoreGameType::canBeTeamGame()       const { return true;  }
bool CoreGameType::canBeIndividualGame() const { return false; }


TNL_IMPLEMENT_NETOBJECT(CoreGameType);


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(CoreItem);
//class LuaCore;


// Ratio at which damage is reduced so that Core Health can fit between 0 and 1.0
// for easier bit transmission
const F32 CoreItem::DamageReductionRatio = 1000.0f;

const F32 CoreItem::PANEL_ANGLE = FloatTau / (F32) CORE_PANELS;

/**
 * @luafunc CoreItem::CoreItem()
 * @luafunc CoreItem::CoreItem(geom, team)
 * @luafunc CoreItem::CoreItem(geom, team, health)
 */
// Combined Lua / C++ default constructor
CoreItem::CoreItem(lua_State *L) : Parent(F32(CoreRadius * 2))    
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = CoreTypeNumber;
   setStartingHealth(F32(CoreDefaultStartingHealth));      // Hits to kill

   mHasExploded = false;
   mHeartbeatTimer.reset(CoreHeartbeatStartInterval);
   mCurrentExplosionNumber = 0;
   //mPanelGeom.isValid = false;
   mRotateSpeed = 1;


   // Read some params from our L, if we have it
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, TEAM_INDX, END }, { PT, TEAM_INDX, INT, END }}, 3 };
      S32 profile = checkArgList(L, constructorArgList, "CoreItem", "constructor");
      if(profile == 1)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }
      else if(profile == 2)
      {
         setPos(L, 1);
         setTeam(L, 2);
         setStartingHealth(getFloat(L, 3));
      }
   }

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
CoreItem::~CoreItem()
{
   LUAW_DESTRUCTOR_CLEANUP;

   // Alert the gameType, if it still exists (it might not when the game is over)
   if(getDatabase())
   {
      GameType *gameType = static_cast<Level *>(getDatabase())->getGameType();

      if(gameType && gameType->getGameTypeId() == CoreGame)
         static_cast<CoreGameType *>(gameType)->removeCore(this);
   }
}


CoreItem *CoreItem::clone() const
{
   return new CoreItem(*this);
}


F32 CoreItem::getCoreAngle(U32 time)
{
   return F32(time & 16383) / 16384.f * FloatTau;
}


void CoreItem::renderItem(const Point &pos) const
{
#ifndef ZAP_DEDICATED
   if(shouldRender())
   {
      GameType *gameType = static_cast<Level *>(getDatabase())->getGameType();
      S32 time = gameType->getTotalGamePlayedInMs();
      PanelGeom panelGeom = getPanelGeom();
      GameObjectRender::renderCore(pos, getColor(), time, &panelGeom, mPanelHealth, mStartingPanelHealth);
   }
#endif
}


bool CoreItem::shouldRender() const
{
   return !mHasExploded;
}


void CoreItem::renderDock(const Color &color) const
{
#ifndef ZAP_DEDICATED
   Point pos = getPos();
   GameObjectRender::renderCoreSimple(pos, Colors::white, 10);
#endif
}


void CoreItem::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices) const
{
#ifndef ZAP_DEDICATED
   Point pos = getPos();
   GameObjectRender::renderCoreSimple(pos, getColor(), CoreRadius * 2);
#endif
}


#ifndef ZAP_DEDICATED
// xpos and ypos are coords of upper left corner of the adjacent score.  We'll need to adjust those coords
// to position our ornament correctly.
void CoreGameType::renderScoreboardOrnament(S32 teamIndex, S32 xpos, S32 ypos) const
{
   Point center(xpos, ypos + 16);
   GameObjectRender::renderCoreSimple(center, getGame()->getTeam(teamIndex)->getColor(), 20);

   // Flash the ornament if the Core is being attacked
   if(isTeamCoreBeingAttacked(teamIndex)) 
   {
      const S32 flashCycleTime = 300;  
      const Color *color = &Colors::red80;
      F32 alpha = 1;

      if(getGame()->getCurrentTime() % flashCycleTime <= flashCycleTime / 2)
      {
         color = &Colors::yellow;
         alpha = 0.6f;
      }
         
      RenderUtils::drawCircle(center, 15, color, alpha);
   }
}

#endif


// Render some attributes when item is selected but not being edited
void CoreItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("Health");   
   values.push_back(itos(S32(mStartingHealth * DamageReductionRatio + 0.5)));
}


const char *CoreItem::getOnScreenName()     const  {  return "Core";   }
const char *CoreItem::getOnDockName()       const  {  return "Core";   }
const char *CoreItem::getPrettyNamePlural() const  {  return "Cores";  }
const char *CoreItem::getEditorHelpString() const  {  return "Core.  Destroy to score.";  }


F32 CoreItem::getEditorRadius(F32 currentScale) const
{
   return CoreRadius * currentScale + 5;
}


bool CoreItem::getCollisionCircle(U32 state, Point &center, F32 &radius) const
{
   center = getPos();
   radius = CoreRadius;
   return true;
}


const Vector<Point> *CoreItem::getCollisionPoly() const
{
   return NULL;
}


bool CoreItem::isPanelDamaged(S32 panelIndex)
{
   return mPanelHealth[panelIndex] < mStartingPanelHealth && mPanelHealth[panelIndex] > 0;
}


bool CoreItem::isPanelInRepairRange(const Point &origin, S32 panelIndex)
{
   PanelGeom panelGeom = getPanelGeom();

   F32 distanceSq1 = (panelGeom.getStart(panelIndex)).distSquared(origin);
   F32 distanceSq2 = (panelGeom.getEnd(panelIndex)).distSquared(origin);
   S32 radiusSq = Ship::RepairRadius * Ship::RepairRadius;

   // Ignoring case where center is in range while endpoints are not...
   return (distanceSq1 < radiusSq) || (distanceSq2 < radiusSq);
}


void CoreItem::damageObject(DamageInfo *theInfo)
{
   if(mHasExploded)
      return;

   if(theInfo->damageAmount == 0)
      return;

   // Special logic for handling the repairing of Core panels
   if(theInfo->damageAmount < 0)
   {
      // Heal each damaged core if it is in range
      for(S32 i = 0; i < CORE_PANELS; i++)
         if(isPanelDamaged(i))
            if(isPanelInRepairRange(theInfo->damagingObject->getPos(), i))
            {
               mPanelHealth[i] -= theInfo->damageAmount / DamageReductionRatio;

               // Don't overflow
               if(mPanelHealth[i] > mStartingPanelHealth)
                  mPanelHealth[i] = mStartingPanelHealth;

               setMaskBits(PanelDamagedMask << i);
            }

      // We're done if we're repairing
      return;
   }

   // Check for friendly fire
   if(theInfo->damagingObject->getTeam() == getTeam())
      return;

   // Which panel was hit?  Look at shot position, compare it to core position
   F32 shotAngle;
   Point p = getPos();

   // Determine angle for Point projectiles like Phaser
   if(theInfo->damageType == DamageTypePoint)
      shotAngle = p.angleTo(theInfo->collisionPoint);

   // Area projectiles
   else
      shotAngle = p.angleTo(theInfo->damagingObject->getPos());


   const PanelGeom panelGeom = getPanelGeom();
   F32 coreAngle = panelGeom.angle;

   F32 combinedAngle = (shotAngle - coreAngle);

   // Make sure combinedAngle is between 0 and Tau -- sometimes angleTo returns odd values
   while(combinedAngle < 0)
      combinedAngle += FloatTau;
   while(combinedAngle >= FloatTau)
      combinedAngle -= FloatTau;

   S32 hit = (S32) (combinedAngle / PANEL_ANGLE);

   if(mPanelHealth[hit] > 0)
      if(damagePanel(hit, theInfo->damageAmount / DamageReductionRatio))
         if(checkIfCoreIsDestroyed())
         {
            coreDestroyed(theInfo);
            return;
         }

   // Reset the attacked warning timer if we're not healing
   if(theInfo->damageAmount > 0)
      mAttackedWarningTimer.reset(CoreAttackedWarningDuration);
}


// Damage specified panel.  Health will not fall below minHealth, which defaults to 0.
// Returns true if panel was destroyed
bool CoreItem::damagePanel(S32 panelIndex, F32 damage, F32 minHealth)
{
   // This check needed to avoid healing panel if minHealth > 0
   if(mPanelHealth[panelIndex] == 0)
      return false;

   mPanelHealth[panelIndex] -= damage;

   if(mPanelHealth[panelIndex] < minHealth)
      mPanelHealth[panelIndex] = minHealth;

   setMaskBits(PanelDamagedMask << panelIndex);

   return mPanelHealth[panelIndex] == 0;
}


// Returns true if all panels are at 0 health
bool CoreItem::checkIfCoreIsDestroyed() const
{
   for(S32 i = 0; i < CORE_PANELS; i++)
      if(mPanelHealth[i] > 0)
         return false;  

   return true;      // Core is dead!
}


void CoreItem::coreDestroyed(const DamageInfo *damageInfo)
{
   // We've scored!  But this only matters in a Core game...
   GameType *gameType = getGame()->getGameType();
   if(gameType && gameType->getGameTypeId() == CoreGame)
   {
      ClientInfo *destroyer = damageInfo->damagingObject->getOwner();

      static_cast<CoreGameType*>(gameType)->score(destroyer, getTeam(), CoreGameType::DestroyedCoreScore);
   }

   mHasExploded = true;
   deleteObject(ExplosionCount * ExplosionInterval);  // Must wait for triggered explosions
   setMaskBits(ExplodedMask);                         
   disableCollision();
}


#ifndef ZAP_DEDICATED
void CoreItem::doExplosion(const Point &pos)
{
   ClientGame *game = static_cast<ClientGame *>(getGame());

   const Color &teamColor = getColor();

   Color CoreExplosionColors[12] = {
      Colors::red,
      teamColor,
      Colors::white,
      teamColor,
      Colors::blue,
      teamColor,
      Colors::white,
      teamColor,
      Colors::yellow,
      teamColor,
      Colors::white,
      teamColor,
   };

   bool isStart = mCurrentExplosionNumber == 0;

   S32 xNeg = TNL::Random::readB() ? 1 : -1;
   S32 yNeg = TNL::Random::readB() ? 1 : -1;

   F32 x = TNL::Random::readF() * xNeg * FloatSqrtHalf * CoreRadius;  // exactly sin(45)
   F32 y = TNL::Random::readF() * yNeg * FloatSqrtHalf * CoreRadius;

   // First explosion is at the center
   Point blastPoint = isStart ? pos : pos + Point(x, y);

   // Also add in secondary sound at start
//   if(isStart)
//      SoundSystem::playSoundEffect(SFXCoreExplodeSecondary, blastPoint);

   SoundSystem::playSoundEffect(SFXCoreExplode, blastPoint, Point(), 1 - 0.25f * F32(mCurrentExplosionNumber));

   game->emitBlast(blastPoint, 600 - 100 * mCurrentExplosionNumber);
   game->emitExplosion(blastPoint, 4.f - F32(mCurrentExplosionNumber), CoreExplosionColors, ARRAYSIZE(CoreExplosionColors));

   mCurrentExplosionNumber++;
}
#endif


PanelGeom CoreItem::getPanelGeom() const
{
   PanelGeom panelGeom;

   U32 timePlayed = getGame() ? getGame()->getGameType()->getTotalGamePlayedInMs() : Platform::getRealMilliseconds();
   fillPanelGeom(getPos(), timePlayed * mRotateSpeed, panelGeom);

   return panelGeom;
}


// static method
void CoreItem::fillPanelGeom(const Point &pos, S32 time, PanelGeom &panelGeom)
{
   F32 size = CoreRadius;

   F32 angle = getCoreAngle(time);
   panelGeom.angle = angle;

   F32 angles[CORE_PANELS];

   for(S32 i = 0; i < CORE_PANELS; i++)
      angles[i] = i * PANEL_ANGLE + angle;

   for(S32 i = 0; i < CORE_PANELS; i++)
      panelGeom.vert[i].set(pos.x + cos(angles[i]) * size, pos.y + sin(angles[i]) * size);

   Point start, end, mid;
   for(S32 i = 0; i < CORE_PANELS; i++)
   {
      start = panelGeom.vert[i];
      end   = panelGeom.vert[(i + 1) % CORE_PANELS];      // Next point, with wrap-around
      mid   = (start + end) * .5;

      panelGeom.mid[i].set(mid);
      panelGeom.repair[i].interp(.6f, mid, pos);
   }

   panelGeom.isValid = true;
}

#ifndef ZAP_DEDICATED

void CoreItem::doPanelDebris(S32 panelIndex)
{
   ClientGame *game = static_cast<ClientGame *>(getGame());

   Point pos = getPos();               // Center of core

   const PanelGeom panelGeom = getPanelGeom();
   
   Point dir = panelGeom.mid[panelIndex] - pos;   // Line extending from the center of the core towards the center of the panel
   dir.normalize(100);
   Point cross(dir.y, -dir.x);         // Line parallel to the panel, perpendicular to dir

   // Debris line is relative to (0,0)
   Vector<Point> points;
   points.push_back(Point(0, 0));
   points.push_back(Point(0, 0));      // Dummy point will be replaced below

   // Draw debris for the panel
   S32 num = Random::readI(5, 15);
   const Color &teamColor = getColor();

   Point chunkPos, chunkVel;           // Reusable containers

   for(S32 i = 0; i < num; i++)
   {
      static const S32 MAX_CHUNK_LENGTH = 10;
      points[1].set(0, Random::readF() * MAX_CHUNK_LENGTH);

      chunkPos = panelGeom.getStart(panelIndex) + (panelGeom.getEnd(panelIndex) - panelGeom.getStart(panelIndex)) * Random::readF();
      chunkVel = dir * (Random::readF() * 10  - 3) * .2f + cross * (Random::readF() * 30  - 15) * .05f;

      S32 ttl = Random::readI(2500, 3000);
      F32 startAngle = Random::readF() * FloatTau;
      F32 rotationRate = Random::readF() * 4 - 2;

      // Every-other chunk is team color instead of panel color
      const Color &chunkColor = i % 2 == 0 ? Colors::gray80 : teamColor;

      game->emitDebrisChunk(points, chunkColor, chunkPos, chunkVel, ttl, startAngle, rotationRate);
   }


   // Draw debris for the panel health 'stake'
   num = Random::readI(5, 15);
   for(S32 i = 0; i < num; i++)
   {
      points.erase(1);
      points.push_back(Point(0, Random::readF() * 10));

      Point sparkVel = cross * (Random::readF() * 20  - 10) * .05f + dir * (Random::readF() * 2  - .5f) * .2f;
      S32 ttl = Random::readI(2500, 3000);
      F32 angle = Random::readF() * FloatTau;
      F32 rotation = Random::readF() * 4 - 2;

      game->emitDebrisChunk(points, Colors::gray20, (panelGeom.mid[i] + pos) / 2, sparkVel, ttl, angle, rotation);
   }

   // And do the sound effect
   SoundSystem::playSoundEffect(SFXCorePanelExplode, panelGeom.mid[panelIndex]);
}

#endif


void CoreItem::idle(BfObject::IdleCallPath path)
{
   //mPanelGeom.isValid = false;      // Force recalculation of panel geometry next time it's needed

   // Update attack timer on the server
   if(path == ServerIdleMainLoop)
   {
      // If timer runs out, then set this Core as having a changed state so the client
      // knows it isn't being attacked anymore
      if(mAttackedWarningTimer.update(mCurrentMove.time))
         setMaskBits(ItemChangedMask);
   }

#ifndef ZAP_DEDICATED
   // Only run the following on the client
   if(path != ClientIdlingNotLocalShip)
      return;

   // Update Explosion Timer
   if(mHasExploded)
   {
      if(mExplosionTimer.getCurrent() != 0)
         mExplosionTimer.update(mCurrentMove.time);
      else
         if(mCurrentExplosionNumber < ExplosionCount)
         {
            doExplosion(getPos());
            mExplosionTimer.reset(ExplosionInterval);
         }
   }

   if(mHeartbeatTimer.getCurrent() != 0)
      mHeartbeatTimer.update(mCurrentMove.time);
   else
   {
      // Thump thump
      SoundSystem::playSoundEffect(SFXCoreHeartbeat, getPos());

      // Now reset the timer as a function of health
      // Exponential
      F32 health = getHealth();
      U32 soundInterval = CoreHeartbeatMinInterval + U32(F32(CoreHeartbeatStartInterval - CoreHeartbeatMinInterval) * health * health);

      mHeartbeatTimer.reset(soundInterval);
   }

   // Emit some sparks from dead panels
   if(Platform::getRealMilliseconds() % 100 < 20)  // 20% of the time...
   {
      Point cross, dir;

      for(S32 i = 0; i < CORE_PANELS; i++)
      {
         if(mPanelHealth[i] == 0)                  // Panel is dead.  Guaranteed to be 0 in CoreItem::damageObject
         {
            Point sparkEmissionPos = getPos();
            sparkEmissionPos += dir * 3;

            Point dir = getPanelGeom().mid[i] - getPos();  // Line extending from the center of the core towards the center of the panel
            dir.normalize(100);
            Point cross(dir.y, -dir.x);                     // Line parallel to the panel, perpendicular to dir

            Point vel = dir * (Random::readF() * 3 + 2) + cross * (Random::readF() - .2f);
            U32 ttl = Random::readI(0, 1000) + 500;

            static_cast<ClientGame *>(getGame())->emitSpark(sparkEmissionPos, vel, Colors::gray20, ttl, UI::SparkTypePoint);
         }
      }
   }
#endif
}


void CoreItem::setStartingHealth(F32 health)
{
   mStartingHealth = health / DamageReductionRatio;

   // Now that starting health has been set, divide it amongst the panels
   mStartingPanelHealth = mStartingHealth / CORE_PANELS;
   
   // Core's total health is divided evenly amongst its panels
   for(S32 i = 0; i < 10; i++)
      mPanelHealth[i] = mStartingPanelHealth;
}


F32 CoreItem::getStartingHealth() const
{
   return mStartingHealth * DamageReductionRatio;
}


F32 CoreItem::getTotalCurrentHealth() const
{
   F32 total = 0;

   for(S32 i = 0; i < CORE_PANELS; i++)
      total += mPanelHealth[i];

   return total;
}


F32 CoreItem::getHealth() const
{
   // health is from 0 to 1.0
   return getTotalCurrentHealth() / mStartingHealth;
}


Vector<Point> CoreItem::getRepairLocations(const Point &repairOrigin)
{
   Vector<Point> repairLocations;

   const PanelGeom panelGeom = getPanelGeom();

   for(S32 i = 0; i < CORE_PANELS; i++)
      if(isPanelDamaged(i))
         if(isPanelInRepairRange(repairOrigin, i))
            repairLocations.push_back(panelGeom.repair[i]);

   return repairLocations;
}


void CoreItem::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   // Make cores always visible
   if(!isGhost())
      setScopeAlways();

   GameType *gameType = theGame->getGameType();

   if(!gameType)                 // Sam has observed this under extreme network packet loss
      return;

   // Alert the gameType
   if(gameType->getGameTypeId() == CoreGame)
      static_cast<CoreGameType *>(gameType)->addCore(this, getTeam());
}


// Compatible with readFloat at the same number of bits
static void writeFloatZeroOrNonZero(BitStream &s, F32 &val, U8 bitCount)
{
   TNLAssert(val >= 0 && val <= 1, "writeFloat Must be between 0.0 and 1.0");
   if(val == 0)
      s.writeInt(0, bitCount);  // always writes zero
   else
   {
      U32 transmissionValue = U32(val * ((1 << bitCount) - 1));  // rounds down

      // If we're not truly at zero, don't send '0', send '1'
      if(transmissionValue == 0)
         s.writeInt(1, bitCount);
      else
         s.writeInt(transmissionValue, bitCount);
   }
}


U32 CoreItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);

   if(stream->writeFlag(updateMask & (InitialMask | TeamMask)))
   {
      writeThisTeam(stream);
      stream->writeSignedInt(mRotateSpeed, 4);
   }

   stream->writeFlag(mHasExploded);

   if(!mHasExploded)
   {
      // Don't bother with health report if we've exploded
      for(S32 i = 0; i < CORE_PANELS; i++)
      {
         if(stream->writeFlag(updateMask & (PanelDamagedMask << i))) // go through each bit mask
         {
            // Normalize between 0.0 and 1.0 for transmission
            F32 panelHealthRatio = mPanelHealth[i] / mStartingPanelHealth;

            // writeFloatZeroOrNonZero will compensate for low resolution by sending zero only if it is actually zero
            // 4 bits -> 1/16 increments, all we really need - this means that client-side
            // will NOT have the true health, rather a ratio of precision 4 bits
            writeFloatZeroOrNonZero(*stream, panelHealthRatio, 4);
         }
      }
   }

   stream->writeFlag(mAttackedWarningTimer.getCurrent());

   return retMask;
}


#ifndef ZAP_DEDICATED

void CoreItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
   {
      readThisTeam(stream);
      mRotateSpeed = stream->readSignedInt(4);
   }

   if(stream->readFlag())     // Exploding!  Take cover!!
   {
      for(S32 i = 0; i < CORE_PANELS; i++)
         mPanelHealth[i] = 0;

      if(!mHasExploded)    // Just exploded!
      {
         mHasExploded = true;
         disableCollision();
         onItemExploded(getPos());
      }
   }
   else                             // Haven't exploded, getting health
   {
      for(S32 i = 0; i < CORE_PANELS; i++)
      {
         if(stream->readFlag())                    // Panel damaged
         {
            // De-normalize to real health
            bool hadHealth = mPanelHealth[i] > 0;
            mPanelHealth[i] = mStartingPanelHealth * stream->readFloat(4);

            // Check if panel just died
            if(hadHealth && mPanelHealth[i] == 0)  
               doPanelDebris(i);
         }
      }
   }

   mBeingAttacked = stream->readFlag();
}

#endif


bool CoreItem::processArguments(S32 argc, const char **argv, Level *level)
{
   if(argc < 4)         // CoreItem <team> <health> <x> <y>
      return false;

   setTeam(atoi(argv[0]));
   setStartingHealth((F32)atof(argv[1]));

   if(!Parent::processArguments(argc - 2, argv + 2, level))
      return false;

   return true;
}


string CoreItem::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + itos(getTeam()) + " " + 
                 ftos(mStartingHealth * DamageReductionRatio) + " " + geomToLevelCode();
}


bool CoreItem::isBeingAttacked()
{
   return mBeingAttacked;
}


bool CoreItem::collide(BfObject *otherObject)
{
   return true;
}


// Degrade all panels that are still alive by specified amount (expressed as a fraction, not as an absolute amount).
// This damage will not kill a core, but will weaken it to a trivially killed state.
void CoreItem::degradeAllPanels(F32 amount)
{
   if(mHasExploded)
      return;

   // Apply damage to each panel... get them almost dead, but just hold back a tiny bit
   for(S32 i = 0; i < CORE_PANELS; i++)
      damagePanel(i, mStartingPanelHealth * amount, F32_MIN);
}


#ifndef ZAP_DEDICATED
// Client only
void CoreItem::onItemExploded(Point pos)
{
   mCurrentExplosionNumber = 0;
   mExplosionTimer.reset(ExplosionInterval);

   // Start with an explosion at the center.  See idle() for other called explosions
   doExplosion(pos);
}


void CoreItem::onGeomChanged()
{
   Parent::onGeomChanged();
}


#ifndef ZAP_DEDICATED

bool CoreItem::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->addMenuItem(new CounterMenuItem("Hit points:", S32(getStartingHealth() + 0.5),
                              1, 1, S32(CoreItem::DamageReductionRatio), "", "", ""));
   return true;
}


void CoreItem::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   setStartingHealth(F32(attributeMenu->getMenuItem(0)->getIntValue()));
}

#endif



#endif


bool CoreItem::canBeHostile() { return true; }
bool CoreItem::canBeNeutral() { return true; }


/////
// Lua interface
/**
 * @luaclass CoreItem
 * 
 * @brief Objective items in Core games
 */
//               Fn name    Param profiles         Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getCurrentHealth, ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, getFullHealth,    ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, setFullHealth,    ARRAYDEF({{ NUM_GE0, END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(CoreItem, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(CoreItem, LUA_METHODS);

#undef LUA_METHODS


const char *CoreItem::luaClassName = "CoreItem";
REGISTER_LUA_SUBCLASS(CoreItem, Item);


/**
 * @luafunc num CoreItem::getCurrentHealth()
 * 
 * @brief Returns the item's current health. This will be between 0 and the
 * result of getFullHealth().
 * 
 * @return The current health .
 */
S32 CoreItem::lua_getCurrentHealth(lua_State *L) 
{ 
   return returnFloat(L, getTotalCurrentHealth() * DamageReductionRatio);
}


/**
 * @luafunc num CoreItem::getFullHealth()
 * 
 * @brief Returns the item's full health.
 * 
 * @return The total full health.
 */
S32 CoreItem::lua_getFullHealth(lua_State *L) 
{ 
   return returnFloat(L, mStartingHealth * DamageReductionRatio);
}


/**
 * @luafunc CoreItem::setFullHealth(num health)
 * 
 * @brief Sets the item's full health. Has no effect on current health.
 *
 * @desc The maximum health of each panel is the full health of the core divided
 * by the number of panels.
 * 
 * @param health The item's new full health 
 */
S32 CoreItem::lua_setFullHealth(lua_State *L) 
{ 
   checkArgList(L, functionArgs, "CoreItem", "setFullHealth");
   setStartingHealth(getFloat(L, 1));

   return 0;     
}


// Overrides
S32 CoreItem::lua_setTeam(lua_State *L)
{
   S32 oldTeamIndex = this->getTeam();
   S32 results = Parent::lua_setTeam(L);
   S32 newTeamIndex = this->getTeam();

   if(getGame()&&getGame()->getGameType()&&getGame()->getGameType()->getGameTypeId()==CoreGame)
   {
      if(oldTeamIndex>=0&&oldTeamIndex < getGame()->getTeamCount())
      {
         Team* oldTeam = dynamic_cast<Team *>(getGame()->getTeam(oldTeamIndex));
         oldTeam->addScore(-1);
         getGame()->getGameType()->s2cSetTeamScore(oldTeamIndex, oldTeam->getScore());
      }

      if(newTeamIndex>=0)
      {
         Team* newTeam = dynamic_cast<Team *>(getGame()->getTeam(newTeamIndex));
         newTeam->addScore(1);
         getGame()->getGameType()->s2cSetTeamScore(newTeamIndex, newTeam->getScore());
      }
   }

   return results;
}

}; /* namespace Zap */
