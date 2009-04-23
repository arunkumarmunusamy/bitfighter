//-----------------------------------------------------------------------------------
//
// bitFighter - A multiplayer vector graphics space game
// Based on Zap demo released for Torque Network Library by GarageGames.com
//
// Derivative work copyright (C) 2008 Chris Eykamp
// Original work copyright (C) 2004 GarageGames.com, Inc.
// Other code copyright as noted
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful (and fun!),
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//------------------------------------------------------------------------------------

#include "robot.h"
#include "item.h"

#include "gameType.h"
#include "sparkManager.h"
#include "projectile.h"
#include "gameLoader.h"
#include "sfx.h"
#include "UI.h"
#include "UIMenus.h"
#include "UIGame.h"
#include "gameConnection.h"
#include "shipItems.h"
#include "gameWeapons.h"
#include "gameObjectRender.h"
#include "flagItem.h"
#include "goalZone.h"
#include "config.h"
#include "BotNavMeshZone.h"      // For BotNavMeshZone class definition
#include "../glut/glutInclude.h"

#define hypot _hypot    // Kill some warnings

namespace Zap
{

static Vector<GameObject *> fillVector;


// Returns a point to calling Lua function
S32 LuaClass::returnPoint(lua_State *L, Point point)
{
   lua_createtable(L, 0, 2);        // creates a table  with 2 fields
   setfield(L, "x", point.x);        // table.x = x 
   setfield(L, "y", point.y);        // table.y = y 

   return 1;
}

// Returns an into to a calling Lua function
S32 LuaClass::returnInt(lua_State *L, S32 num)
{
   lua_pushinteger(L, num);
   return 1;
}


// Returns an into to a calling Lua function
S32 LuaClass::returnString(lua_State *L, const char *str)
{
   lua_pushstring(L, str);
   return 1;
}


// Returns nil to calling Lua function
S32 LuaClass::returnNil(lua_State *L)
{
   lua_pushnil(L); 
   return 1;
}



// Overwrite Lua's panicky panic function with something that doesn't kill the whole game
// if something goes wrong!  
int LuaClass::luaPanicked(lua_State *L)
{
   string msg = lua_tostring(L, 1);
   lua_getglobal(L, "ERROR");    // <-- what is this for?

   throw(msg);

   return 0;
}


void LuaClass::clearStack(lua_State *L)
{
   lua_pop(L, lua_gettop(L));
}


// Assume that table is at the top of the stack
void LuaClass::setfield (lua_State *L, const char *key, F32 value) 
{
   lua_pushnumber(L, value);
   lua_setfield(L, -2, key);
}

///////////////////////////////////

// Constructor
LuaRobot::LuaRobot(lua_State *L)
{
   lua_atpanic(L, luaPanicked);                 // Register our panic function
   thisRobot = (Robot*)lua_touserdata(L, 1);    // Register our robot

   // Set some enumeration values that we'll need in Lua
   #define setEnum(name) { lua_pushinteger(L, name); lua_setglobal(L, #name); }
   #define setGTEnum(name) { lua_pushinteger(L, GameType::name); lua_setglobal(L, #name); }
      
   // Game Objects
   setEnum(ShipType);
   setEnum(BarrierType);
   setEnum(MoveableType);
   setEnum(BulletType);
   setEnum(ItemType);
   setEnum(ResourceItemType);
   setEnum(EngineeredType);
   setEnum(ForceFieldType);
   setEnum(LoadoutZoneType);
   setEnum(MineType);
   setEnum(TestItemType);
   setEnum(FlagType);
   setEnum(TurretTargetType);
   setEnum(SlipZoneType);
   setEnum(HeatSeekerType);
   setEnum(SpyBugType);
   setEnum(NexusType);
   setEnum(BotNavMeshZoneType);
   setEnum(RobotType);
   setEnum(TeleportType);
   setEnum(GoalZoneType);
   setEnum(AsteroidType);

   // Game Types
   setGTEnum(BitmatchGame);
   setGTEnum(CTFGame);
   setGTEnum(HTFGame);
   setGTEnum(NexusGame);
   setGTEnum(RabbitGame);
   setGTEnum(RetrieveGame);
   setGTEnum(SoccerGame);
   setGTEnum(ZoneControlGame);

   // Scoring Events
   setGTEnum(KillEnemy);
   setGTEnum(KillSelf);
   setGTEnum(KillTeammate);
   setGTEnum(KillEnemyTurret);
   setGTEnum(KillOwnTurret);
   setGTEnum(CaptureFlag);
   setGTEnum(CaptureZone);
   setGTEnum(UncaptureZone);
   setGTEnum(HoldFlagInZone);
   setGTEnum(RemoveFlagFromEnemyZone);
   setGTEnum(RabbitHoldsFlag);
   setGTEnum(RabbitKilled);
   setGTEnum(RabbitKills);
   setGTEnum(ReturnFlagsToNexus);
   setGTEnum(ReturnFlagToZone);
   setGTEnum(LostFlag);
   setGTEnum(ReturnTeamFlag);
   setGTEnum(ScoreGoalEnemyTeam);
   setGTEnum(ScoreGoalHostileTeam);
   setGTEnum(ScoreGoalOwnTeam);

   // Modules
   setEnum(ModuleShield);
   setEnum(ModuleBoost);
   setEnum(ModuleSensor);
   setEnum(ModuleRepair);
   setEnum(ModuleEngineer);
   setEnum(ModuleCloak);

   // Weapons
   setEnum(WeaponPhaser);
   setEnum(WeaponBounce);
   setEnum(WeaponTriple);
   setEnum(WeaponBurst);
   setEnum(WeaponMine);
   setEnum(WeaponSpyBug);
}

// Destructor
LuaRobot::~LuaRobot(){
  logprintf("deleted Lua Object (%p)\n", this);
}


// Define the methods we will expose to Lua
// Methods defined here need to be defined in the LuaRobot in robot.h
#define method(class, name) {#name, &class::name}

Luna<LuaRobot>::RegType LuaRobot::methods[] = {
      method(LuaRobot, getAngle),
      method(LuaRobot, getPosXY),

      method(LuaRobot, getZoneCenterXY),
      method(LuaRobot, getGatewayFromZoneToZone),
      method(LuaRobot, getZoneCount),
      method(LuaRobot, getCurrentZone),
         
      method(LuaRobot, setAngle),
      method(LuaRobot, setAngleXY),
      method(LuaRobot, getAngleXY),
      method(LuaRobot, hasLosXY),

      method(LuaRobot, hasFlag),
      method(LuaRobot, getWaypoint),

      method(LuaRobot, setThrustAng),
      method(LuaRobot, setThrustXY),
      method(LuaRobot, fire),
      method(LuaRobot, setWeapon),
      method(LuaRobot, globalMsg),
      method(LuaRobot, teamMsg),

      method(LuaRobot, logprint),

      method(LuaRobot, findObjects),

      method(LuaRobot, getGameType),
      method(LuaRobot, getFlagCount),
      method(LuaRobot, getWinningScore),
      method(LuaRobot, getGameTimeTotal),
      method(LuaRobot, getGameTimeRemaining),
      method(LuaRobot, getLeadingScore),
      method(LuaRobot, getLeadingTeam),
      method(LuaRobot, getLevelName),
      method(LuaRobot, getGridSize),
      method(LuaRobot, getIsTeamGame),
      method(LuaRobot, getEventScore),

   {0,0}    // End list of methods
};


// Turn to angle a (in radians)
S32 LuaRobot::setAngle(lua_State *L)
{
   if(!lua_isnil(L, 1))
   {
      Move move = thisRobot->getCurrentMove();
      move.angle = luaL_checknumber(L, 1);
      thisRobot->setCurrentMove(move);
   }
   return 0;
}

// Turn towards point XY
S32 LuaRobot::setAngleXY(lua_State *L)
{
   if(lua_isnil(L, 1) || lua_isnil(L, 2))
      return 0;

   F32 x = luaL_checknumber(L, 1);
   F32 y = luaL_checknumber(L, 2);

   Move move = thisRobot->getCurrentMove();
   move.angle = thisRobot->getAngleXY(x, y);
   thisRobot->setCurrentMove(move);

   return 0;
}


// Get angle toward point XY
S32 LuaRobot::getAngleXY(lua_State *L)
{
   F32 x = luaL_checknumber(L, 1);
   F32 y = luaL_checknumber(L, 2);
   lua_pushnumber(L, thisRobot->getAngleXY(x, y));
   return 1;
}


// Thrust at velocity v toward angle a
S32 LuaRobot::setThrustAng(lua_State *L)
{
   F32 vel, ang;

   if(lua_isnil(L, 1) || lua_isnil(L, 2))
   {
      vel = 0;
      ang = 0;
   }
   else
   {
      vel = luaL_checknumber(L, 1);
      ang = luaL_checknumber(L, 2);
   }

   Move move;

   move.up = sin(ang) <= 0 ? -vel * sin(ang) : 0;
   move.down = sin(ang) > 0 ? vel * sin(ang) : 0;
   move.right = cos(ang) >= 0 ? vel * cos(ang) : 0;
   move.left = cos(ang) < 0 ? -vel * cos(ang) : 0;

   thisRobot->setCurrentMove(move);
   
   return 0;
}


// Thrust at velocity v toward point x,y
S32 LuaRobot::setThrustXY(lua_State *L)
{
   F32 vel, ang;

   if(lua_isnil(L, 1) || lua_isnil(L, 2) || lua_isnil(L, 3))
   {
      vel = 0;
      ang = 0;
   }
   else
   {
      vel = luaL_checknumber(L, 1);
      F32 x = luaL_checknumber(L, 2);
      F32 y = luaL_checknumber(L, 3);

      ang = thisRobot->getAngleXY(x,y) - 0 * FloatHalfPi;
   }

   Move move;

   move.up = sin(ang) < 0 ? -vel * sin(ang) : 0;
   move.down = sin(ang) > 0 ? vel * sin(ang) : 0;
   move.right = cos(ang) > 0 ? vel * cos(ang) : 0;
   move.left = cos(ang) < 0 ? -vel * cos(ang) : 0;

   thisRobot->setCurrentMove(move);

  return 0;
}


extern Vector<SafePtr<BotNavMeshZone>> gBotNavMeshZones;

// Get the coords of the center of mesh zone z
S32 LuaRobot::getZoneCenterXY(lua_State *L)
{
   // You give us a nil, we'll give it right back!
   if(lua_isnil(L, 1))
      return returnNil(L);

   S32 z = luaL_checknumber(L, 1);   // Desired zone

   // In case this gets called too early...
   if(gBotNavMeshZones.size() == 0)
      return returnNil(L);

   // Bounds checking...
   if(z < 0 || z >= gBotNavMeshZones.size())
      return returnNil(L);


   Point c = gBotNavMeshZones[z]->getCenter();

   lua_pushnumber(L, c.x);
   lua_pushnumber(L, c.y);
   return 2;
}

// Get the coords of the gateway to the specified zone.  Returns nil, nil if requested zone doesn't border current zone.
S32 LuaRobot::getGatewayFromZoneToZone(lua_State *L)
{
   // You give us a nil, we'll give it right back!
   if(lua_isnil(L, 1) || lua_isnil(L, 2))
      return returnNil(L);


   S32 from = luaL_checknumber(L, 1);     // From this zone
   S32 to = luaL_checknumber(L, 2);     // To this one

   // In case this gets called too early...
   if(gBotNavMeshZones.size() == 0)
      return returnNil(L);

   // Bounds checking...
   if(from < 0 || from >= gBotNavMeshZones.size() || to < 0 || to >= gBotNavMeshZones.size())
      return returnNil(L);

   // Is requested zone a neighbor?
   for(S32 i = 0; i < gBotNavMeshZones[from]->mNeighbors.size(); i++)
   {
      if(gBotNavMeshZones[from]->mNeighbors[i].zoneID == to)
      {
         Rect r(gBotNavMeshZones[from]->mNeighbors[i].borderStart, gBotNavMeshZones[from]->mNeighbors[i].borderEnd);
         lua_pushnumber(L, r.getCenter().x);
         lua_pushnumber(L, r.getCenter().y);
         return 2;
      }
   }

   // Did not find requested neighbor... returning nil
   return returnNil(L);
}



// Get the zone this robot is currently in.  If not in a zone, return nil
S32 LuaRobot::getCurrentZone(lua_State *L)
{
   S32 zone = thisRobot->getCurrentZone();

   if(zone == -1)
      return returnNil(L);

   lua_pushnumber(L, zone);
   return 1;
}

// Get a count of how many nav zones we have
S32 LuaRobot::getZoneCount(lua_State *L)
{
   lua_pushnumber(L, gBotNavMeshZones.size());
   return 1;
}

// Fire current weapon if possible
S32 LuaRobot::fire(lua_State *L)
{
   Move move = thisRobot->getCurrentMove();
   move.fire = true;
   thisRobot->setCurrentMove(move);

   return 0;
}

// Can robot see point XY?
S32 LuaRobot::hasLosXY(lua_State *L)
{
   F32 x, y;

   if(lua_isnil(L, 1) || lua_isnil(L, 2))
      return returnNil(L);

   x = luaL_checknumber(L, 1);
   y = luaL_checknumber(L, 2);

   lua_pushboolean(L, thisRobot->canSeePoint(Point(x,y)) );
   return 1;
}



// Does robot have a flag?
S32 LuaRobot::hasFlag(lua_State *L)
{
   lua_pushboolean(L, (thisRobot->carryingFlag() != GameType::NO_FLAG));
   return 1;
}



// Set weapon  to 1, 2, or 3
S32 LuaRobot::setWeapon(lua_State *L)
{
   U32 weap = luaL_checknumber(L, 1);
      thisRobot->selectWeapon(weap);
   return 0;
}


// Send message to all players
S32 LuaRobot::globalMsg(lua_State *L)
{
   if(lua_isnil(L, 1))
      return 0;

   GameType *gt = gServerGame->getGameType();
   if(gt)
      gt->s2cDisplayChatMessage(true, thisRobot->getName(), lua_tostring(L, 1));
   return 0;
}

// Send message to team (what happens when neutral/enemytoall robot does this???)
S32 LuaRobot::teamMsg(lua_State *L)
{
   if(lua_isnil(L, 1))
      return 0;

   GameType *gt = gServerGame->getGameType();
   if(gt)
      gt->s2cDisplayChatMessage(false, thisRobot->getName(), lua_tostring(L, 1));
   return 0;
}


// Returns current aim angle of ship  --> needed?
S32 LuaRobot::getAngle(lua_State *L)
{
  lua_pushnumber(L, thisRobot->getCurrentMove().angle);
  return 1;
}


// Returns current position of ship
S32 LuaRobot::getPosXY(lua_State *L)
{
  lua_pushnumber(L, thisRobot->getActualPos().x);
  lua_pushnumber(L, thisRobot->getActualPos().y);
  return 2;
}


// Write a message to the server logfile
S32 LuaRobot::logprint(lua_State *L)
{
   if(!lua_isnil(L, 1))
      logprintf("RobotLog %s: %s", thisRobot->getName().getString(), lua_tostring(L, 1));
   return 0; 
}  


S32 LuaRobot::getGameType(lua_State *L)
{
   TNLAssert(gServerGame->getGameType(), "Need Gametype check in getGameType");
   return returnInt(L, gServerGame->getGameType()->getGameType());
}


S32 LuaRobot::getFlagCount(lua_State *L)         { return returnInt(L, gServerGame->getGameType()->getFlagCount()); }
S32 LuaRobot::getWinningScore(lua_State *L)      { return returnInt(L, gServerGame->getGameType()->getWinningScore()); }
S32 LuaRobot::getGameTimeTotal(lua_State *L)     { return returnInt(L, gServerGame->getGameType()->getTotalGameTime()); }
S32 LuaRobot::getGameTimeRemaining(lua_State *L) { return returnInt(L, gServerGame->getGameType()->getRemainingGameTime()); }
S32 LuaRobot::getLeadingScore(lua_State *L)      { return returnInt(L, gServerGame->getGameType()->getLeadingScore()); }
S32 LuaRobot::getLeadingTeam(lua_State *L)       { return returnInt(L, gServerGame->getGameType()->getLeadingTeam()); }

S32 LuaRobot::getLevelName(lua_State *L)         { return returnString(L, gServerGame->getGameType()->mLevelName.getString()); }
S32 LuaRobot::getGridSize(lua_State *L)          { return returnInt(L, gServerGame->getGridSize()); }
S32 LuaRobot::getIsTeamGame(lua_State *L)        { return returnInt(L, gServerGame->getGameType()->isTeamGame()); }


S32 LuaRobot::getEventScore(lua_State *L) 
{ 
   S32 n = lua_gettop(L);  // Number of arguments
   if (n != 1)
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "getEventScore called with %d args, expected 1", n);
      logprintf(msg);
      throw(string(msg)); 
   }

   if(!lua_isnumber(L, 1))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "getEventScore called with non-numeric arg");
      logprintf(msg);
      throw(string(msg)); 
   }

   U32 scoringEvent = luaL_checknumber(L, 1);
   if(scoringEvent > GameType::ScoringEventsCount)
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "getEventScore called with out-of-bounds arg: %d", scoringEvent);
      logprintf(msg);
      throw(string(msg));
   }

   return returnInt(L, gServerGame->getGameType()->getEventScore(GameType::TeamScore, (GameType::ScoringEvent) scoringEvent, 0));
}


extern Rect gServerWorldBounds;

S32 LuaRobot::findObjects(lua_State *L)
{
   //LuaProtectStack x(this); <== good idea, not working right...  ;-(

   S32 n = lua_gettop(L);  // Number of arguments
   if (n != 1)
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "findObjects called with %d args, expected 1", n);
      logprintf(msg);
      throw(string(msg)); 
   }

   if(!lua_isnumber(L, 1))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "findObjects called with non-numeric arg");
      logprintf(msg);
      throw(string(msg)); 
   }

   U32 objectType = luaL_checknumber(L, 1);

   fillVector.clear();

   // ShipType BarrierType MoveableType BulletType ItemType ResourceItemType EngineeredType ForceFieldType LoadoutZoneType MineType TestItemType FlagType TurretTargetType SlipZoneType HeatSeekerType SpyBugType NexusType

   // thisRobot->findObjects(CommandMapVisType, fillVector, gServerWorldBounds);    // Get all globally visible objects
   //thisRobot->findObjects(ShipType, fillVector, Rect(thisRobot->getActualPos(), gServerGame->computePlayerVisArea(thisRobot)) );    // Get other objects on screen-visible area only
   thisRobot->findObjects(objectType, fillVector, gServerWorldBounds );    // Get other objects on screen-visible area only
   
   F32 bestRange = F32_MAX;
   Point bestPoint;

   GameType *gt = gServerGame->getGameType();
   TNLAssert(gt, "Invaid game type!!");

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      // Some special rules for narrowing in on the objects we really want
      if(fillVector[i]->getObjectTypeMask() & ShipType)
      {
         Ship *ship = (Ship*)fillVector[i];

         // If it's dead or cloaked, ignore it
         if((ship->isModuleActive(ModuleCloak) && !ship->areItemsMounted()) || ship->hasExploded)
            continue;
      }
      else if(fillVector[i]->getObjectTypeMask() & FlagType)
      {
         FlagItem *flag = (FlagItem*)fillVector[i];

         // Ignore flags mounted on ships
         if(flag->isMounted())
            continue;

         TNLAssert(gServerGame->getGameType(), "Need to add gameType check in flagCheck/findObjects");

         // Is flag in a zone?  Which kind?
         bool isInZone = flag->getZone();
         bool isInOwnZone = isInZone && flag->getZone()->getTeam() == thisRobot->getTeam();
         bool isNotInOwnZone = !isInOwnZone;
         bool isInEnemyZone = isInZone && !isInOwnZone;
         bool isInLeaderZone = isInZone && flag->getZone()->getTeam() == gServerGame->getGameType()->mLeadingTeam;
         bool isInNoZone = !isInZone;
         //-----
         //closest
         //-----


         if(!isNotInOwnZone)
            continue;

      }
      else if(fillVector[i]->getObjectTypeMask() & GoalZoneType)
      {
         TNLAssert(gServerGame->getGameType(), "Need to add gameType check in goalZoneCheck/findObjects");

         GoalZone *goal = (GoalZone*)fillVector[i];

         bool isOwnZone = goal->getTeam() == thisRobot->getTeam();
         bool isNeutralZone = goal->getTeam() == -1;
         bool isEnemyZone = !(isOwnZone || isNeutralZone);
         bool isNotOwnZone = !isOwnZone;
         bool isLeaderZone = goal->getTeam() == gServerGame->getGameType()->mLeadingTeam; 
         //-----
         bool hasFlag = goal->mHasFlag;
         bool hasNoFlag = !hasFlag;
         //-----
         //closest
         //-----

         // Find only own goalzones
         if(!isOwnZone)
            continue;

         // For now, ignore goalzones with flags in them
         if(!hasNoFlag)
            continue;
      }

      GameObject *potential = fillVector[i];

      // If object needs to be in LOS, uncomment following
      //if( ! thisRobot->canSeePoint(potential->getActualPos()) )   // Can we see it?
      //   continue;      // No

      F32 dist = thisRobot->getActualPos().distanceTo(potential->getActualPos());

      if(dist < bestRange)
      {
         bestPoint  = potential->getActualPos();
         bestRange  = dist;
      }
   }

   // Write the results to Lua
   if(bestRange < F32_MAX)
      return returnPoint(L, bestPoint);

   // else no targets found
   return returnNil(L);
}


extern S32 findZoneContaining(Point p);

// Get next waypoint to head toward when traveling from current location to x,y
// Note that this function will be called frequently by various robots, so any
// optimizations will be helpful.
S32 LuaRobot::getWaypoint(lua_State *L)
{
   S32 n = lua_gettop(L);  // Number of arguments
   if (n != 2)
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "getWaypoint called with %d args, expected 2", n);
      logprintf(msg);
      throw(string(msg)); 
   }
//alternatively: luaL_checktype(L,1,LUA_TNUMBER) , throws an error?
   if(!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "getWaypoint called with non-numeric arg");
      logprintf(msg);
      throw(string(msg)); 
   }

   F32 x = luaL_checknumber(L, 1);
   F32 y = luaL_checknumber(L, 2);

   Point target = Point(x, y);

   S32 targetZone = findZoneContaining(target);       // Where we're going

   // Make sure target is still in the same zone it was in when we created our flightplan.
   // If we're not, our flightplan is invalid, and we need to skip forward and build a fresh one.
   if(targetZone == thisRobot->flightPlanTo)
   {

      // In case our target has moved, replace final point of our flightplan with the current target location
      thisRobot->flightPlan[0] = target;

      // First, let's scan through our pre-calculated waypoints and see if we can see any of them.  
      // If so, we'll just head there with no further rigamarole.  Remember that our flightplan is
      // arranged so the closest points are at the end of the list, and the target is at index 0.
      Point dest;
      bool found = false;
      bool first = true;

      while(thisRobot->flightPlan.size() > 0)
      {
         Point last = thisRobot->flightPlan.last();

         // We'll assume that if we could see the point on the previous turn, we can
         // still see it, even though in some cases, the turning of the ship around a 
         // protruding corner may make it technically not visible.  This will prevent
         // rapidfire recalcuation of the path when it's not really necessary.
         if(first || thisRobot->canSeePoint(last))
         {
            dest = last;
            found = true;
            first = false;
            thisRobot->flightPlan.pop_back();   // Discard now possibly superfluous waypoint
         }
         else
            break;
      }

      // If we found one, that means we found a visible waypoint, and we can head there...
      if(found)
      {
         thisRobot->flightPlan.push_back(dest);    // Put dest back at the end of the flightplan
         return returnPoint(L, dest);
      }
   }

   // We need to calculate a new flightplan
   thisRobot->flightPlan.clear();

   S32 currentZone = thisRobot->getCurrentZone();     // Where we are
   if(currentZone == -1)      // We don't really know where we are... bad news!  Let's find closest visible zone and go that way.
      //return findAndReturnClosestZone(L, thisRobot->getActualPos());
      return returnNil(L);

   if(targetZone == -1)       // Our target is off the map.  See if it's visible from any of our zones, and, if so, go there
      //return findAndReturnClosestZone(L, target);
      return returnNil(L);

   // We're in, or on the cusp of, the zone containing our target.  We're close!!
   if(currentZone == targetZone)
   {
      Point p;
      if(!thisRobot->canSeePoint(target))    // Possible, if we're just on a boundary, and a protrusion's blocking a ship edge
      {
         p = gBotNavMeshZones[targetZone]->getCenter();
         thisRobot->flightPlan.push_back(p);
      }
      else
         p = target;

      thisRobot->flightPlan.push_back(target);
      return returnPoint(L, p);
   }   

   // If we're still here, then we need to find a new path.  Either our original path was invalid for some reason,
   // or the path we had no longer applied to our current location
   thisRobot->flightPlanTo = targetZone;
   thisRobot->flightPlan = AStar::findPath(currentZone, targetZone, target);

   if(thisRobot->flightPlan.size() > 0)
      return returnPoint(L, thisRobot->flightPlan.last());
   else
      return returnNil(L);    // Out of options, end of the road
}
//
//
//// Encapsulate some ugliness
//Point LuaRobot::getNextWaypoint()
//{
//   TNLAssert(thisRobot->flightPlan.size() > 1, "FlightPlan has too few zones!");
//
//   S32 currentzone = thisRobot->getCurrentZone();
//
//   S32 nextzone = thisRobot->flightPlan[thisRobot->flightPlan.size() - 2];    
//
//   // Note that getNeighborIndex could return -1.  It shouldn't, but it could.
//   S32 neighborZoneIndex = gBotNavMeshZones[currentzone]->getNeighborIndex(nextzone);
//   TNLAssert(neighborZoneIndex >= 0, "Invalid neighbor zone index!");
//
//
//            /*   string plan = "";
//               for(S32 i = 0; i < thisRobot->flightPlan.size(); i++)
//               {
//                  char z[100];
//                  itoa(thisRobot->flightPlan[i], z, 10);
//                  plan = plan + " ==>"+z;
//               }*/
//
//
//   S32 i = 0;
//   Point currentwaypoint = gBotNavMeshZones[currentzone]->getCenter();
//   Point nextwaypoint = gBotNavMeshZones[currentzone]->mNeighbors[neighborZoneIndex].borderCenter;
//
//   while(thisRobot->canSeePoint(nextwaypoint))
//   {
//      currentzone = nextzone;
//      currentwaypoint = nextwaypoint;
//      i++;
//
//      if(thisRobot->flightPlan.size() - 2 - i < 0)
//         return nextwaypoint;
//
//      nextzone = thisRobot->flightPlan[thisRobot->flightPlan.size() - 2 - i];  
//      S32 neighborZoneIndex = gBotNavMeshZones[currentzone]->getNeighborIndex(nextzone);
//      TNLAssert(neighborZoneIndex >= 0, "Invalid neighbor zone index!");
//        
//      nextwaypoint = gBotNavMeshZones[currentzone]->mNeighbors[neighborZoneIndex].borderCenter;
//   }
//
//
//   return currentwaypoint;
//
//
//
//   //if(thisRobot->canSeePoint( gBotNavMeshZones[currentZone]->mNeighbors[neighborZoneIndex].center)) 
//   //   return gBotNavMeshZones[currentZone]->mNeighbors[neighborZoneIndex].center;
//   //else if(thisRobot->canSeePoint( gBotNavMeshZones[currentZone]->mNeighbors[neighborZoneIndex].borderCenter)) 
//   //   return gBotNavMeshZones[currentZone]->mNeighbors[neighborZoneIndex].borderCenter;
//   //else
//   //   return gBotNavMeshZones[currentZone]->getCenter();
//}
//

// Another helper function: finds closest zone to a given point
S32 LuaRobot::findClosestZone(Point point)
{
   F32 distsq = F32_MAX;
   S32 closest = -1;

   for(S32 i = 0; i < gBotNavMeshZones.size(); i++)
   {
      Point center = gBotNavMeshZones[i]->getCenter();

      if( gServerGame->getGridDatabase()->pointCanSeePoint(center, point) )
      {
         F32 d = center.distSquared(point);
         if(d < distsq)
         {
            closest = i;
            distsq = d;
         }         
      }
   }

   return closest;
}


S32 LuaRobot::findAndReturnClosestZone(lua_State *L, Point point)
{
   S32 closest = findClosestZone(point);

   if(closest != -1)
      return returnPoint(L, gBotNavMeshZones[closest]->getCenter());
   else
      return returnNil(L);    // Really stuck.
}


/* Control:
// setLoadout(m1, m2, w1, w2, w3)
// useModule(S32) // 1 && 2 && 4 of modules

// About us
author()
description()
weaponOK()  -> weapon ready to fire?



Info:
getWeapRange() -> curr weapon range
getWeapName() -> curr weapon name
getHealth()
getEnergy()
getWeapon() -> returns id of weapon


findAll()

ship.speed
ship.health
ship.name

getPlayerName(ship)
getHealth(ship)

*/


const char LuaRobot::className[] = "LuaRobot";
U32 Robot::mRobotCount = 0;

//------------------------------------------------------------------------
TNL_IMPLEMENT_NETOBJECT(Robot);


// Constructor
Robot::Robot(StringTableEntry robotName, S32 team, Point p, F32 m) : Ship(robotName, team, p, m)
{
   mObjectTypeMask = RobotType | MoveableType | CommandMapVisType | TurretTargetType;  

   S32 junk = this->mCurrentZone;
   mNetFlags.set(Ghostable);
   //getGame()->getGameType()->mRobotList.push_back(this);
   mTeam = team;
   mass = m;            // Ship's mass
   L = NULL;
   mCurrentZone = -1;
   flightPlanTo = -1;

   // Need to provide some time on here to get timer to trigger robot to spawn.  It's timer driven.
   respawnTimer.reset(100, RobotRespawnDelay);     

   hasExploded = true;     // Becase we start off "dead", but will respawn real soon now...

   disableCollision();

   // try http://csl.sublevel3.org/lua/
}
  

Robot::~Robot()
{
   if(getGame() && getGame()->isServer())
      mRobotCount--;

   // Close down our Lua interpreter
   if(L)
      lua_close(L);

   // Remove robot from robotList, as it's now an ex-robot.
   //for(S32 i = 0; i < getGame()->getGameType()->mRobotList.size(); i++)
   //   if(getGame()->getGameType()->mRobotList[i] = this)
   //   {
   //      getGame()->getGameType()->mRobotList.erase(i);
   //      break;
   //   }

   logprintf("Robot terminated [%s]", mFilename.c_str());
}


// Reset everything on the robot back to the factory settings
bool Robot::initialize(Point p)
{
   for(U32 i = 0; i < MoveStateCount; i++)
   {
      mMoveState[i].pos = p;
      mMoveState[i].angle = 0;
      mMoveState[i].vel = Point(0,0);
   }
   updateExtent();

   respawnTimer.clear();

   mHealth = 1.0;       // Start at full health
   hasExploded = false; // Haven't exploded yet!

   mCurrentZone = -1;   // Correct value will be calculated upon first request

   for(S32 i = 0; i < TrailCount; i++)          // Clear any vehicle trails
      mTrail[i].reset();

   mEnergy = (S32) ((F32) EnergyMax * .80);     // Start off with 80% energy
   for(S32 i = 0; i < ModuleCount; i++)         // and all modules disabled
      mModuleActive[i] = false;

   // Set initial module and weapon selections
   mModule[0] = ModuleBoost;
   mModule[1] = ModuleShield;

   mWeapon[0] = WeaponPhaser;
   mWeapon[1] = WeaponMine;
   mWeapon[2] = WeaponBurst;

   hasExploded = false;
   enableCollision();

   mActiveWeaponIndx = 0;
   mCooldown = false;

   // WarpPositionMask triggers the spinny spawning visual effect
   setMaskBits(RespawnMask | HealthMask | LoadoutMask | PositionMask | MoveMask | PowersMask | WarpPositionMask);      // Send lots to the client

   if(L)
      lua_close(L);
   
   L = lua_open();    // Create a new Lua interpreter

   // Register the LuaRobot data type with Lua
   Luna<LuaRobot>::Register(L);
     
   // Push a pointer to this Robot to the Lua stack
   lua_pushlightuserdata(L, (void*)this);

   // And set the global name of this pointer.  This is the name that we'll use to refer
   // to our robot from our Lua code.
   lua_setglobal(L, "Robot");

   try
   {
      // Load the script 
      S32 retCode = luaL_loadfile(L, mFilename.c_str());

      if(retCode)
      {
         logError("Error loading file:" + string(lua_tostring(L, -1)) + ".  Shutting robot down.");
         return false;
      }

      // Run main script body 
      lua_pcall(L, 0, 0, 0);
   }
   catch(string e)
   {
      logError("Error initializing robot: " + e + ".  Shutting robot down.");
      return false;
   }
  
   try
   {
      lua_getglobal(L, "getName");
      lua_call(L, 0, 1);                  // Passing 0 params, getting one back
      mPlayerName = lua_tostring(L, -1);	
      lua_pop(L, 1);
   }
   catch (string e)
   {
      logError("Robot error running getName(): " + e + ".  Shutting robot down.");
      return false;
   }

   return true;
}


void Robot::onAddedToGame(Game *)
{
   // Make them always visible on cmdr map --> del
   if(!isGhost())
      setScopeAlways();

   if(getGame() && getGame()->isServer())
      mRobotCount++;
}


// Basically exists to override Ship::kill(info)
 void Robot::kill(DamageInfo *theInfo)
{
   kill();
}


void Robot::kill()
{
   hasExploded = true;
   respawnTimer.reset();
   setMaskBits(ExplosionMask);
   
   disableCollision();

   // Dump mounted items
   for(S32 i = mMountedItems.size() - 1; i >= 0; i--)
      mMountedItems[i]->onMountDestroyed();
}


bool Robot::processArguments(S32 argc, const char **argv)
{
   if(argc != 2)
      return false;

   mTeam = atoi(argv[0]);     // Need some sort of bounds check here??

   mFilename = "robots/";
   mFilename += argv[1];

   return true;
}


// Some rudimentary robot error logging.  Perhaps, someday, will become a sort of in-game error console.
// For now, though, pass all errors through here.
void Robot::logError(string err)
{
   logprintf("***ROBOT ERROR*** in %s ::: %s", mFilename.c_str(), err.c_str());
}


S32 Robot::getCurrentZone()
{
   // We're in uncharted territory -- try to get the current zone
   if(mCurrentZone == -1)
   {
      mCurrentZone = findZoneContaining(getActualPos());
   }
   return mCurrentZone;
}

// Setter method, not a robot function!
void Robot::setCurrentZone(S32 zone)
{
   mCurrentZone = zone;
}


F32 Robot::getAngleXY(F32 x, F32 y)
{
   return atan2(y - getActualPos().y, x - getActualPos().x);
}

// Process a move.  This will advance the position of the ship, as well as adjust its velocity and angle.
void Robot::processMove(U32 stateIndex)
{
   mMoveState[LastProcessState] = mMoveState[stateIndex];

   F32 maxVel = getMaxVelocity();

   F32 time = mCurrentMove.time * 0.001;
   Point requestVel(mCurrentMove.right - mCurrentMove.left, mCurrentMove.down - mCurrentMove.up);

   requestVel *= maxVel;
   F32 len = requestVel.len();

   // Remove this block to allow robots to exceed speed of 1
   if(len > maxVel)
      requestVel *= maxVel / len;

   Point velDelta = requestVel - mMoveState[stateIndex].vel;
   F32 accRequested = velDelta.len();


   // Apply turbo-boost if active
   F32 maxAccel = (isModuleActive(ModuleBoost) ? BoostAcceleration : Acceleration) * time;
   if(accRequested > maxAccel)
   {
      velDelta *= maxAccel / accRequested;
      mMoveState[stateIndex].vel += velDelta;
   }
   else
      mMoveState[stateIndex].vel = requestVel;

   mMoveState[stateIndex].angle = mCurrentMove.angle;
   move(time, stateIndex, false);
}


extern bool PolygonContains2(const Point *inVertices, int inNumVertices, const Point &inPoint);

// Return coords of nearest ship... and experimental robot routine
bool Robot::findNearestShip(Point &loc)
{
   Vector<GameObject *> foundObjects;
   Point closest;

   Point pos = getActualPos();
   Point extend(2000, 2000);
   Rect r(pos - extend, pos + extend);

   findObjects(ShipType, foundObjects, r);

   if(!foundObjects.size())
      return false;
      
   F32 dist = F32_MAX;
   bool found = false;

   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      F32 d = foundObjects[i]->getActualPos().distanceTo(pos);
      if(d < dist && d > 0)      // d == 0 for self
      {
         dist = d;
         loc = foundObjects[i]->getActualPos();
         found = true;
      }
   }
   return found;
}


bool Robot::canSeePoint(Point point)
{
   // Need to check the two edge points perpendicular to the direction of looking to ensure we have an unobstructed 
   // flight lane to point.  Radius of the robot is mRadius.  This keeps the ship from getting hung up on
   // obstacles that appear visible from the center of the ship, but are actually blocked.

   F32 ang = getActualPos().angleTo(point);
   F32 cosang = cos(ang) * mRadius;
   F32 sinang = sin(ang) * mRadius;

   Point edgePoint1 = getActualPos() + Point(sinang, -cosang);
   Point edgePoint2 = getActualPos() + Point(-sinang, cosang);

   return( 
      gServerGame->getGridDatabase()->pointCanSeePoint(edgePoint1, point) &&
      gServerGame->getGridDatabase()->pointCanSeePoint(edgePoint2, point) );
}


void Robot::idle(GameObject::IdleCallPath path)
{
   //U32 deltaT;

   if(path == GameObject::ServerIdleMainLoop)
   {
      //U32 ms = Platform::getRealMilliseconds();
      //deltaT = (ms - mLastMoveTime);
      //mLastMoveTime = ms;
      //mCurrentMove.time = deltaT;

      // Check to see if we need to respawn this robot
      if(hasExploded)
      {
         if(respawnTimer.update(mCurrentMove.time)) 
            gServerGame->getGameType()->spawnRobot(this);

         return;     
      }
   }

   // Don't process exploded ships
   if(hasExploded)
      return;

   if(path == GameObject::ServerIdleMainLoop)      // Running on server
   {
      // Clear out current move.  It will get set just below with the lua call, but if that function
      // doesn't set the various move components, we want to make sure that they default to 0.
      mCurrentMove.fire = false; 
      mCurrentMove.up = 0;
      mCurrentMove.down = 0;
      mCurrentMove.right = 0;
      mCurrentMove.left = 0;

      try
      {
	      lua_getglobal(L, "getMove");
	      lua_call(L, 0, 0);
      }
      catch (string e)
      {
         logError("Robot error running getMove(): " + e + ".  Shutting robot down.");
         delete this;
         return;
      }
      catch (...)    // Catches any other errors not caught above --> should never happen
      {
         logError("Robot error: Unknown exception.  Shutting robot down");
         delete this;
         return;
      }
      

      // If we've changed the mCurrentMove, then we need to set 
      // the MoveMask to ensure that it is sent to the clients
      setMaskBits(MoveMask);

      processMove(ActualState);

      // Apply impulse vector and reset it
      mMoveState[ActualState].vel += mImpulseVector;
      mImpulseVector.set(0,0);

      // Update the render state on the server to match
      // the actual updated state, and mark the object
      // as having changed Position state.  An optimization
      // here would check the before and after positions
      // so as to not update unmoving ships.

      mMoveState[RenderState] = mMoveState[ActualState];
      setMaskBits(PositionMask);
   }
   else if(path == GameObject::ClientIdleMainRemote)  // Running on client (but not replaying saved game)
   {
      // On the client, update the interpolation of this object, unless we are replaying control moves
      mInterpolating = (getActualVel().lenSquared() < MoveObject::InterpMaxVelocity*MoveObject::InterpMaxVelocity);
      updateInterpolation();
   }

   updateExtent();            // Update the object in the game's extents database
   mLastMove = mCurrentMove;  // Save current move

   // Update module timers
   mSensorZoomTimer.update(mCurrentMove.time);
   mCloakTimer.update(mCurrentMove.time);

   //bool engineerWasActive = isModuleActive(ModuleEngineer);

   if(path == GameObject::ServerIdleMainLoop)    // Was ClientIdleControlReplay  
   {
      // Process weapons and energy on controlled object objects
      processWeaponFire();
      processEnergy();
   }

   if(path == GameObject::ClientIdleMainRemote)       // Probably should be server
   {
      // For ghosts, find some repair targets for rendering the repair effect
      if(isModuleActive(ModuleRepair))
         findRepairTargets();
   }

   if(false && isModuleActive(ModuleRepair))     // Probably should be server
      repairTargets();

   // If we're on the client, do some effects
   if(path == GameObject::ClientIdleMainRemote)
   {
      mWarpInTimer.update(mCurrentMove.time);
      // Emit some particles, trail sections and update the turbo noise
      emitMovementSparks();
      for(U32 i=0; i<TrailCount; i++)
         mTrail[i].tick(mCurrentMove.time);
      updateModuleSounds();
   }
}


void Robot::render(S32 layerIndex)
{
   Parent::render(layerIndex);

//   GameObject *controlObject = gClientGame->getConnectionToServer()->getControlObject();
//   Ship *u = dynamic_cast<Ship *>(controlObject);      // This is the local player's ship
//
//   Point position = u->getRenderPos();
//
//   F32 zoomFrac = 1;
//
//glPopMatrix();
//
//      // Set up the view to show the whole level.
//   Rect mWorldBounds = gClientGame->computeWorldObjectExtents(); // TODO: Cache this value!  ?  Or not?
//
//   Point worldCenter = mWorldBounds.getCenter();
//   Point worldExtents = mWorldBounds.getExtents();
//   worldExtents.x *= UserInterface::canvasWidth / F32(UserInterface::canvasWidth - (UserInterface::horizMargin * 2));
//   worldExtents.y *= UserInterface::canvasHeight / F32(UserInterface::canvasHeight - (UserInterface::vertMargin * 2));
//
//   F32 aspectRatio = worldExtents.x / worldExtents.y;
//   F32 screenAspectRatio = UserInterface::canvasWidth / F32(UserInterface::canvasHeight);
//   if(aspectRatio > screenAspectRatio)
//      worldExtents.y *= aspectRatio / screenAspectRatio;
//   else
//      worldExtents.x *= screenAspectRatio / aspectRatio;
//
   //Point offset = (worldCenter - position) * zoomFrac + position;
//   Point visSize = gClientGame->computePlayerVisArea(u) * 2;
//   Point modVisSize = (worldExtents - visSize) * zoomFrac + visSize;
//
   //Point visScale(UserInterface::canvasWidth / modVisSize.x,
                  //UserInterface::canvasHeight / modVisSize.y );

   //glPushMatrix();
   //glTranslatef(gScreenWidth / 2, gScreenHeight / 2, 0);    // Put (0,0) at the center of the screen

   //glScalef(visScale.x, visScale.y, 1);
   //glTranslatef(-offset.x, -offset.y, 0);

glColor3f(1,1,1);
   S32 xx = this->flightPlanTo;
   glBegin(GL_LINE_STRIP);
   for(S32 i = 0; i < flightPlan.size(); i++)
   {
      glVertex2f(flightPlan[i].x, flightPlan[i].y);
   }
   glEnd();

   //glPopMatrix();
}


};
