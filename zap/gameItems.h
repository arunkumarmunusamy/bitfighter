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


#ifndef _GAME_ITEMS_H_
#define _GAME_ITEMS_H_

#include "item.h"
#include "ship.h"
#include "gameObjectRender.h"
#include "../glut/glutInclude.h"
#include "luaObject.h"



namespace Zap
{

class RepairItem : public PickupItem
{
private:
   typedef PickupItem Parent;
   S32 mRepopDelay;

public:
   RepairItem(Point p = Point());   // Constructor
   bool processArguments(S32 argc, const char **argv);
   bool pickup(Ship *theShip);
   void onClientPickup();
   U32 getRepopDelay();
   void renderItem(Point pos);

   TNL_DECLARE_CLASS(RepairItem);
};



static const S32 AsteroidDesigns = 4;
static const S32 AsteroidPoints = 12;

static const F32 mRenderSize[] = { .8, .4, .2, -1 };      // Must end in -1
static const S32 mRenderSizes = sizeof(mRenderSize) / sizeof(F32) - 1;

static const S32 mSizeIndexLength = sizeof(mRenderSize) / sizeof(S32) - 1;
static const F32 AsteroidRadius = 89;

static const S8 AsteroidCoords[AsteroidDesigns][AsteroidPoints][2] =   // <== Wow!  A 3D array!
{
  { { 80, -43}, { 47, -84 }, { 5, -58 }, { -41, -81 }, { -79, -21 }, { -79, -0 }, { -79, 10 }, { -79, 47 }, { -49, 78 }, { 43, 78 }, { 80, 40 }, { 46, -0 } },
  { { -41, -83 }, { 18, -83 }, { 81, -42 }, { 83, -42 }, { 7, -2 }, { 81, 38 }, { 41, 79 }, { 10, 56 }, { -48, 79 }, { -80, 15 }, { -80, -43 }, { -17, -43 } },
  { { -2, -56 }, { 40, -79 }, { 81, -39 }, { 34, -19 }, { 82, 22 }, { 32, 83 }, { -21, 59 }, { -40, 82 }, { -80, 42 }, { -57, 2 }, { -79, -38 }, { -31, -79 } }, 
  { { 42, -82 }, { 82, -25 }, { 82, 5 }, { 21, 80 }, { -19, 80 }, { -8, 5 }, { -48, 79 }, { -79, 16 }, { -39, -4 }, { -79, -21 }, { -19, -82 }, { -4, -82 } }, 
};


class LuaAsteroid;

class Asteroid : public Item, public LuaObject
{

typedef Item Parent;

private:
   S32 mSizeIndex;    
   bool hasExploded;
   S32 mDesign;

   LuaAsteroid *mLuaProxy;

public:
   Asteroid();     // Constructor

   void renderItem(Point pos);
   bool getCollisionPoly(U32 state, Vector<Point> &polyPoints);
   bool getCollisionCircle(U32 state, Point center, F32 radius);
   bool collide(GameObject *otherObject);

   void damageObject(DamageInfo *theInfo);
   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);
   void emitAsteroidExplosion(Point pos);

   S32 getSizeIndex() { return mSizeIndex; }
   S32 getSizeCount() { return mRenderSizes; }

   TNL_DECLARE_CLASS(Asteroid);

   ///// Lua interface

   public:
   Asteroid(lua_State *L);    // Constructor

   static const char className[];

   static Lunar<Asteroid>::RegType methods[];

   S32 getClassID(lua_State *L) { return returnInt(L, AsteroidType); }

   S32 getSize(lua_State *L);        // Index of current asteroid size (0 = initial size, 1 = next smaller, 2 = ...) (returns int)
   S32 getSizeCount(lua_State *L);   // Number of indexes of size we can have (returns int)
   S32 getLoc(lua_State *L);         // Center of asteroid (returns point)
   S32 getRad(lua_State *L);         // Radius of asteroid (returns number)
   S32 getVel(lua_State *L);         // Speed of asteroid (returns point)
};


class TestItem : public Item, public LuaObject
{

public:
   TestItem();     // Constructor

   void renderItem(Point pos);
   void damageObject(DamageInfo *theInfo);
   bool getCollisionPoly(U32 state, Vector<Point> &polyPoints);

   TNL_DECLARE_CLASS(TestItem);

   ///// Lua Interface

   TestItem(lua_State *L);             //  Lua constructor

   static const char className[];

   static Lunar<TestItem>::RegType methods[];

   S32 getClassID(lua_State *L) { return returnInt(L, TestItemType); }

   S32 getLoc(lua_State *L);    // Center of testItem (returns point)
   S32 getRad(lua_State *L);     // Radius of testItem (returns number)
   S32 getVel(lua_State *L);    // Speed of testItem (returns point)
};


class ResourceItem : public Item
{
public:
   ResourceItem();      // Constructor

   void renderItem(Point pos);
   bool collide(GameObject *hitObject);
   void damageObject(DamageInfo *theInfo);
   
   TNL_DECLARE_CLASS(ResourceItem);
};

};

#endif
