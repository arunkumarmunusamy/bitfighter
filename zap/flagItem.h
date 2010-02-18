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

#ifndef _FLAGITEM_H_
#define _FLAGITEM_H_

#include "item.h"

namespace Zap
{

class FlagItem : public Item
{
private:
   typedef Item Parent;
   Point mInitialPos;                   // Where flag was "born"
   void push(lua_State *L) {  Lunar<FlagItem>::push(L, this); }
   void flagDropped();

protected:
   U32 mFlagCount;                      // How many flags does this represet?
   Timer mDroppedTimer;                 // Make flags have a tiny bit of delay before they can be picked up again
   static const U32 dropDelay = 500;    // in ms

public:
   FlagItem(Point pos = Point());                                    // C++ constructor
   FlagItem(Point pos, bool collidable, float radius, float mass);   // Alternate C++ constructor

   void initialize();      // Set inital values of things

   virtual bool processArguments(S32 argc, const char **argv);

   virtual void onAddedToGame(Game *theGame);
   virtual void renderItem(Point pos);
   virtual void sendHome();

   virtual void onMountDestroyed();
   virtual void onItemDropped(Ship *ship);
   virtual bool collide(GameObject *hitObject);
   virtual bool isAtHome();
   Timer mTimer;                       // Used for games like HTF where time a flag is held is important

   virtual U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   virtual void unpackUpdate(GhostConnection *connection, BitStream *stream);
   virtual void idle(GameObject::IdleCallPath path);


   TNL_DECLARE_CLASS(FlagItem);

   ///// Lua Interface

   FlagItem(lua_State *L) { /* Do nothing */ };    //  Lua constructor

   static const char className[];
   static Lunar<FlagItem>::RegType methods[];

   S32 getClassID(lua_State *L) { return returnInt(L, FlagType); }
   
   S32 getTeamIndx(lua_State *L) { return returnInt(L, FlagType); }           // Index of owning team (-1 for neutral flag)
   S32 isInInitLoc(lua_State *L) { return returnBool(L, isAtHome()); }        // Is flag in it's initial location?
   S32 isInCaptureZone(lua_State *L) { return returnBool(L, isInZone()); }    // Is flag in a team's capture zone?
   S32 isOnShip(lua_State *L) { return returnBool(L, mIsMounted); }           // Is flag being carried by a ship?
};

extern void renderFlag(Point pos, Color flagColor);
extern void renderFlag(Point pos, Color flagColor, Color mastColor, F32 alpha);


////////////////////////////////


class FlagSpawn
{
private:
   Point mPos;

public:
   static const S32 defaultRespawnTime = 30;    // in seconds

   FlagSpawn(Point pos, S32 delay);    // C++ constructor (no lua constructor)
   Point getPos() { return mPos; }
   Timer timer;
};

};

#endif


