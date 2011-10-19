//-----------------------------------------------------------------------------------
//
// Bitfighter - A multiplayer vector graphics space game
// Based on Zap demo released for Torque Network Library by GarageGames.com
//
// Derivative work copyright (C) 2008-2009 Chris Eykamp
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

#include "flagItem.h"
#include "goalZone.h"
#include "gameType.h"
#include "game.h"
#include "stringUtils.h"      // For itos
#include "gameConnection.h"
#include "gameObjectRender.h"

#ifndef ZAP_DEDICATED
#include "SDL/SDL_opengl.h"
#endif

namespace Zap
{

TNL_IMPLEMENT_NETOBJECT(FlagItem);

// C++ constructor
FlagItem::FlagItem(Point pos) : Parent(pos, true, (F32)Ship::CollisionRadius) // radius was 20
{
   initialize();
}


// Alternate constructor, currently used by HuntersFlag
FlagItem::FlagItem(Point pos, bool collidable, float radius, float mass) : Parent(pos, collidable, radius, mass)
{
   initialize();
}


// Alternate constructor, currently used by dropping flags in hunterGame
FlagItem::FlagItem(Point pos, Point vel, bool useDropDelay) : Parent(pos, true, (F32)Ship::CollisionRadius, 4)
{
   initialize();

   setActualVel(vel);
   if(useDropDelay)
      mDroppedTimer.reset(DROP_DELAY);
}



FlagItem *FlagItem::clone() const
{
   return new FlagItem(*this);
}


void FlagItem::initialize()
{
   mTeam = -1;
   mIsAtHome = true;    // All flags start off at home!

   mNetFlags.set(Ghostable);
   mObjectTypeNumber = FlagTypeNumber;
   setZone(NULL);
}


const char FlagItem::className[] = "FlagItem";      // Class name as it appears to Lua scripts

// Define the methods we will expose to Lua
Lunar<FlagItem>::RegType FlagItem::methods[] =
{
   // Standard gameItem methods
   method(FlagItem, getClassID),
   method(FlagItem, getLoc),
   method(FlagItem, getRad),
   method(FlagItem, getVel),
   method(FlagItem, getTeamIndx),

   // item methods
   method(FlagItem, isInCaptureZone),
   method(FlagItem, getCaptureZone),
   method(FlagItem, isOnShip),
   method(FlagItem, getShip),

   // Flag specific methods
   method(FlagItem, isInInitLoc),

   {0,0}    // End method list
};


void FlagItem::onAddedToGame(Game *theGame)
{ 
   Parent::onAddedToGame(theGame);
   theGame->getGameType()->addFlag(this);    
}


void FlagItem::setZone(GoalZone *goalZone)
{
   // If the item on which we're setting the zone is a flag (which, at this point, it always will be),
   // we want to make sure to update the zone itself.  This is mostly a convenience for robots searching
   // for objects that meet certain criteria, such as for zones that contain a flag.
   FlagItem *flag = dynamic_cast<FlagItem *>(this);

   if(flag)
   {
      GoalZone *zone = ((goalZone == NULL) ? flag->getZone() : goalZone);

      if(zone)
         zone->setHasFlag(goalZone != NULL);
   }

   // Now we can get around to setting the zone, which is what we came here to do
   mZone = goalZone;
   setMaskBits(ZoneMask);
}


GoalZone *FlagItem::getZone()
{
	return mZone;
}


S32 FlagItem::getCaptureZone(lua_State *L) 
{ 
   if(mZone.isValid()) 
   {
      mZone->push(L); 
      return 1;
   } 
   else 
      return returnNil(L); 
}


static bool isTeamFlagSpawn(Game *game, S32 team)
{
   return game->getGameType()->isTeamFlagGame() && team >= 0 && team < game->getTeamCount();
}


bool FlagItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 3)         // FlagItem <team> <x> <y> {time}
      return false;

   mTeam = atoi(argv[0]);
   //if(! getGame()->getGameType()->checkTeamRange(mTeam)) mTeam = -1;
   
   if(!Parent::processArguments(argc-1, argv+1, game))
      return false;

   S32 time = (argc >= 4) ? atoi(argv[4]) : 0;     // Flag spawn time is possible 4th argument.  This time only turns out to be important in Nexus games at the moment.

   mInitialPos = mMoveState[ActualState].pos;

   // Now add the flag starting point to the list of flag spawn points
   GameType *gt = game->getGameType();
   if(gt)
   {
      FlagSpawn spawn = FlagSpawn(mInitialPos, time);

      if(isTeamFlagSpawn(game, mTeam))
      {
         Team *team = dynamic_cast<Team *>(game->getTeam(mTeam));
         if(team)
            team->addFlagSpawn(spawn);
      }
      else                                                                        
         gt->addFlagSpawn(spawn);
   }

   return true;
}


string FlagItem::toString(F32 gridSize) const
{
   return string(getClassName()) + " " + itos(mTeam) + " " + geomToString(gridSize);
}


U32 FlagItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);

   if(stream->writeFlag(updateMask & ZoneMask))
   {
      if(mZone.isValid())
      {
         S32 index = connection->getGhostIndex(mZone);
         if(stream->writeFlag(index != -1))
            stream->writeInt(index, GhostConnection::GhostIdBitSize);
         else
            retMask |= ZoneMask;
      }
      else
         stream->writeFlag(false);
   }

   if(updateMask & InitialMask)
      writeThisTeam(stream);

   return retMask;
}


void FlagItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())     // ZoneMask
   {
      bool hasZone = stream->readFlag();
      if(hasZone)
         mZone = (GoalZone *) connection->resolveGhost(stream->readInt(GhostConnection::GhostIdBitSize));
      else
         mZone = NULL;
   }

   if(mInitial)
      readThisTeam(stream);
}


void FlagItem::idle(GameObject::IdleCallPath path)
{
   Parent::idle(path);

   if(isGhost()) 
      return;
}


void FlagItem::mountToShip(Ship *theShip)
{
   Parent::mountToShip(theShip);

   if(mIsMounted)    // Will be true unless something went wrong in mountToShip
      mIsAtHome = false;
}


// Only called from from sendHome()
const Vector<FlagSpawn> *FlagItem::getSpawnPoints()
{
   Game *game = getGame();
   GameType *gt = game->getGameType();

   TNLAssert(gt, "Invalid gameType!");

   if(isTeamFlagSpawn(game, mTeam))    
      return ((Team *)(game->getTeam(mTeam)))->getFlagSpawns();
   else             
      return gt->getFlagSpawns();
}


void FlagItem::sendHome()
{
   // Now that we have flag spawn points, we'll simply redefine "initial pos" as a random selection of the flag spawn points
   // Everything else should remain as it was

   // First, make a temp list of valid spawn points -- start with a list of all spawn points, then remove any occupied ones

   Vector<FlagSpawn> spawnPoints = *getSpawnPoints();    // Makes a copy?  Hopefully!
   Game *game = getGame();
   GameType *gt = game->getGameType();

   // Now remove the occupied spots from our list of potential spawns
   for(S32 i = 0; i < gt->mFlags.size(); i++)
   {
      FlagItem *flag = gt->mFlags[i];
      if(flag->isAtHome() && (flag->mTeam < 0 || flag->mTeam == mTeam || !gt->isTeamFlagGame()))
      {
         // Need to remove this flag's spawnpoint from the list of potential spawns... it's occupied, after all...
         // Note that if two spawnpoints are on top of one another, this will remove the first, leaving the other
         // on the unoccupied list, unless a second flag at this location removes it from the list on a subsequent pass.
         for(S32 j = 0; j < spawnPoints.size(); j++)
            if(spawnPoints[j].getPos() == flag->mInitialPos)
            {
               spawnPoints.erase_fast(j);
               break;
            }
      }
   }

   if(spawnPoints.size() == 0)      // Protect from crash if this happens, which it shouldn't, but has
   {
      TNLAssert(false, "No flag spawn points!");
      logprintf(LogConsumer::LogError, "LEVEL ERROR!! Level %s has no flag spawn points for team %d\n**Please submit this level to the devs!**", 
         gServerGame->getCurrentLevelFileName().getString(), mTeam);
      //mInitialPos = Point(0,0);      --> Leave mInitialPos as it was... it will probably be better than (0,0)
   }
   else
   {
      S32 spawnIndex = TNL::Random::readI() % spawnPoints.size();
      mInitialPos = spawnPoints[spawnIndex].getPos();
   }

   mMoveState[ActualState].pos = mMoveState[RenderState].pos = mInitialPos;
   mMoveState[ActualState].vel = mMoveState[RenderState].vel = Point(0,0);
   mIsAtHome = true;
   setMaskBits(PositionMask);
   updateExtent();
}


void FlagItem::renderItem(const Point &pos)
{
   Point offset;

   if(mIsMounted)
      offset.set(15, -15);

   renderFlag(pos + offset, getGame()->getTeamColor(mTeam));
}


void FlagItem::renderDock()
{
#ifndef ZAP_DEDICATED
   glPushMatrix();
      glTranslate(getVert(0));
      glScale(0.6f);
      renderFlag(0, 0, getGame()->getTeamColor(mTeam));
   glPopMatrix();   
#endif
}


F32 FlagItem::getEditorRadius(F32 currentScale)
{
   return 18 * currentScale;
}


// Runs on both client and server
bool FlagItem::collide(GameObject *hitObject)
{
   // Flag never collides if it is mounted or is set to be not collideable for some reason
   if(mIsMounted || !mIsCollideable)
      return false;

   // Flag always collides with walls and forcefields
   if(isFlagCollideableType(hitObject->getObjectTypeNumber()))
      return true;

   // No other collision detection happens on the client -- From here on out, it's server only!
   if(isGhost())
      return false;

   // The only things we'll collide with (aside from walls and forcefields above) is ships and robots
   if( !(isShipType(hitObject->getObjectTypeNumber())) )
      return false;

   // Ignore collisions that occur to recently dropped flags.  Make sure flag is ready to be picked up! 
   if(mDroppedTimer.getCurrent())    
      return false;

   // We've hit a ship or robot  (remember, robot is a subtype of ship, so this will work for both)
   // We'll need to make sure the ship is a valid entity and that it hasn't exploded
   Ship *ship = dynamic_cast<Ship *>(hitObject);
   if(!ship || (ship->hasExploded))
      return false;

   GameType *gt = getGame()->getGameType();

   if(!gt)     // Something is wrong...
      return false;     
   
   // Finally!
   gt->shipTouchFlag(ship, this);

   return false;
}


void FlagItem::onMountDestroyed()
{
   if(mMount->getOwner())
      mMount->getOwner()->mStatistics.mFlagDrop++;

   onItemDropped();
}


};
