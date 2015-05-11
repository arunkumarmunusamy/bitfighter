//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _ZONE_H_
#define _ZONE_H_

#include "polygon.h"          // Parent class


namespace Zap
{

class Zone : public PolygonObject
{
   typedef PolygonObject Parent;

public:
   explicit Zone(lua_State *L = NULL);    // Combined Lua / C++ constructor
   virtual ~Zone();                       // Destructor
   Zone *clone() const;

   virtual void render() const;
   S32 getRenderSortValue();
   virtual bool processArguments(S32 argc, const char **argv, Level *level);

   virtual const Vector<Point> *getCollisionPoly() const;     // More precise boundary for precise collision detection
   virtual bool collide(BfObject *hitObject);

   /////
   // Editor methods
   virtual const char *getEditorHelpString() const;
   virtual const char *getPrettyNamePlural() const;
   virtual const char *getOnDockName() const;
   virtual const char *getOnScreenName() const;

   bool hasTeam();      
   bool canBeHostile(); 
   bool canBeNeutral(); 

   virtual string toLevelCode() const;

   virtual void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;
   virtual void renderDock(const Color &color) const;

   virtual F32 getEditorRadius(F32 currentScale) const;

   TNL_DECLARE_CLASS(Zone);

   //// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Zone);

	static const char *luaClassName;
	static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   int lua_containsPoint(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

// Extends above with some methods related to client/server interaction; Zone itself is server-only
class GameZone : public Zone
{
   typedef Zone Parent;

public:
   GameZone();
   virtual ~GameZone();

   virtual U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   virtual void unpackUpdate(GhostConnection *connection, BitStream *stream);
};


};


#endif
