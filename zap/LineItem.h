//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _LINEITEM_H_
#define _LINEITEM_H_

#include "BfObject.h"

using namespace std;

namespace Zap
{

class EditorAttributeMenuUI;

class LineItem : public CentroidObject
{
   typedef CentroidObject Parent;

private:
   Vector<Point> mRenderPoints;     // Precomputed points used for rendering linework

   S32 mWidth;
   bool mGlobal;    // If global, then all teams will see it

public:
   explicit LineItem(lua_State *L = NULL);   // Combined C++ / Lua constructor
   virtual ~LineItem();                      // Destructor
   LineItem *clone() const;

   virtual void render() const;
   bool shouldRender() const;

   S32 getRenderSortValue();

   bool processArguments(S32 argc, const char **argv, Level *level); // Create objects from parameters stored in level file
   void onAddedToGame(Game *theGame);
   virtual void onGhostAvailable(GhostConnection *connection);
   virtual void onGhostAddBeforeUpdate(GhostConnection *connection);

   bool isVisibleToTeam(S32 teamIndex) const;


   const Vector<Point> *getCollisionPoly() const;                    // More precise boundary for precise collision detection
   bool collide(BfObject *hitObject);
   void idle(BfObject::IdleCallPath path);
   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);
   F32 getUpdatePriority(GhostConnection *connection, U32 updateMask, S32 updateSkips);

   virtual void setGeom(lua_State *L, S32 stackIndex);

   virtual void onGeomChanged();

   /////
   // Editor methods
   string toLevelCode() const;
   virtual void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;
   virtual const Color &getEditorRenderColor() const;


   // Thickness-related
   virtual void setWidth(S32 width);
   virtual void setWidth(S32 width, S32 min, S32 max);
   virtual S32 getWidth() const;
   void changeWidth(S32 amt);  

#ifndef ZAP_DEDICATED
   bool startEditingAttrs(EditorAttributeMenuUI *attributeMenu);    // Called when we start editing to get menus populated
   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);     // Called when we're done to retrieve values set by the menu

   void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);
#endif

   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString() const;
   const char *getPrettyNamePlural() const;
   const char *getOnDockName() const;
   const char *getOnScreenName() const;

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   F32 getEditorRadius(F32 currentScale) const;

   static const S32 MIN_LINE_WIDTH;
   static const S32 MAX_LINE_WIDTH;

   TNL_DECLARE_CLASS(LineItem);
   TNL_DECLARE_RPC(s2cSetGeom, (Vector<Point> geom));

   ///// Lua Interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(LineItem);

	static const char *luaClassName;
	static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_setGlobal(lua_State *L);
   S32 lua_getGlobal(lua_State *L);
};


};

#endif


