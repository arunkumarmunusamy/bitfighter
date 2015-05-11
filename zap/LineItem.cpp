//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "LineItem.h"

#include "Level.h"
#include "game.h"
#include "ship.h"
#include "GameObjectRender.h"    // For renderPolyLineVertices()
#include "stringUtils.h"         // For itos
#include "tnlGhostConnection.h"

#ifndef ZAP_DEDICATED
#  include "GameObjectRender.h"  // For renderPolyLineVertices()
#  include "RenderUtils.h"
#  include "ClientGame.h"
#  include "UIQuickMenu.h"       // For EditorAttributeMenuUI def
#endif


#include <math.h>

namespace Zap
{

using namespace LuaArgs;

TNL_IMPLEMENT_NETOBJECT(LineItem);

TNL_IMPLEMENT_NETOBJECT_RPC(LineItem, s2cSetGeom,
      (Vector<Point> geom), (geom),
      NetClassGroupGameMask, RPCGuaranteedOrderedBigData, RPCToGhost, 0)
{
   GeomObject::setGeom(geom);
   updateExtentInDatabase();
}

const S32 LineItem::MIN_LINE_WIDTH = 1;
const S32 LineItem::MAX_LINE_WIDTH = 255;

// Combined C++ / Lua constructor
/**
 * @luafunc LineItem::LineItem()
 * @luafunc LineItem::LineItem(geom)
 * @luafunc LineItem::LineItem(geom, teamIndex)
 */
LineItem::LineItem(lua_State *L)
{ 
   mNetFlags.set(Ghostable);
   setNewGeometry(geomPolyLine);
   mObjectTypeNumber = LineTypeNumber;

   mWidth = 2;
   mGlobal = true;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
   
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { SIMPLE_LINE, END }, { SIMPLE_LINE, TEAM_INDX, END }}, 3 };

      S32 profile = checkArgList(L, constructorArgList, "LineItem", "constructor");

      if(profile == 1)
         setGeom(L, 1);

      else if(profile == 2)
      {
         setGeom(L, 1);
         setTeam(L, 2);
      }
   }
}


// Destructor
LineItem::~LineItem()
{ 
   LUAW_DESTRUCTOR_CLEANUP;
}


LineItem *LineItem::clone() const
{
   return new LineItem(*this);
}


F32 LineItem::getEditorRadius(F32 currentScale) const
{
   return (F32)EditorObject::VERTEX_SIZE;
}


void LineItem::render() const
{
#ifndef ZAP_DEDICATED
   RenderUtils::drawLine(getOutline(), getColor());
#endif
}


bool LineItem::shouldRender() const
{
   if(mGlobal)
      return true;

#ifndef ZAP_DEDICATED
   //S32 ourTeam = static_cast<ClientGame*>(getGame())->getCurrentTeamIndex();

   //// Don't render opposing team's line items
   // Always render all teams when in editor
   // ourTeam == TEAM_NEUTRAL when in editor
   //if(ourTeam != getTeam() && getTeam() != TEAM_NEUTRAL && ourTeam != TEAM_NEUTRAL)
   //   return false;
#endif

   // Render item regardless of team when in editor (local remote ClientInfo will be NULL)
   return true;
}


void LineItem::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices) const
{
#ifndef ZAP_DEDICATED
   if(isSelected() || isLitUp())
      RenderUtils::drawLine(getOutline());
   else
      RenderUtils::drawLine(getOutline(), getEditorRenderColor());

   if(renderVertices)
      GameObjectRender::renderPolyLineVertices(this, snappingToWallCornersEnabled, currentScale);
#endif
}


const Color &LineItem::getEditorRenderColor() const
{ 
   return getColor(); 
}


// This object should be drawn below others
S32 LineItem::getRenderSortValue()
{
   return 1;
}


// Create objects from parameters stored in level file
// LineItem <team> <width> <x> <y> ...
bool LineItem::processArguments(S32 argc, const char **argv, Level *level)
{
   if(argc < 6)
      return false;

   setTeam (atoi(argv[0]));
   setWidth(atoi(argv[1]));

   int firstCoord = 2;
   if(strcmp(argv[2], "Global") == 0)
   {
      mGlobal = true;
      firstCoord = 3;
   }
   else
      mGlobal = false;

   readGeom(argc, argv, firstCoord, level->getLegacyGridSize());

   updateExtentInDatabase();

   return true;
}


string LineItem::toLevelCode() const
{
   string out = string(appendId(getClassName())) + " " + itos(getTeam()) + " " + itos(getWidth());

   if(mGlobal)
      out += " Global";

   out += " " + geomToLevelCode();

   return out;
}


void LineItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

   if(!isGhost())
      setScopeAlways();
}


void LineItem::onGhostAvailable(GhostConnection* connection)
{
   Parent::onGhostAvailable(connection);

   RefPtr<NetEvent> theEvent = TNL_RPC_CONSTRUCT_NETEVENT(this, s2cSetGeom, (*GeomObject::getOutline()));
   connection->postNetEvent(theEvent);
}


void LineItem::onGhostAddBeforeUpdate(GhostConnection* connection)
{
   Parent::onGhostAddBeforeUpdate(connection);
   updateExtentInDatabase();
}


bool LineItem::isVisibleToTeam(S32 teamIndex) const
{
   // LineItems are only visible to those on the same team, unless they're neutral or "global"
   return mGlobal || getTeam() == teamIndex || getTeam() == TEAM_NEUTRAL;
}


const Vector<Point> *LineItem::getCollisionPoly() const
{
   return NULL;
}


// Handle collisions with a LineItem.  Easy, there are none.
bool LineItem::collide(BfObject *hitObject)
{
   return false;
}


void LineItem::idle(BfObject::IdleCallPath path)
{
   // Do nothing
}


U32 LineItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   //stream->writeRangedU32(mWidth, 0, MAX_LINE_WIDTH);
   writeThisTeam(stream);
   stream->write(mGlobal);

   return 0;
}


void LineItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   //mWidth = stream->readRangedU32(0, MAX_LINE_WIDTH);
   readThisTeam(stream);
   mGlobal = stream->readFlag();  // Set this client side
}


F32 LineItem::getUpdatePriority(GhostConnection *connection, U32 updateMask, S32 updateSkips)
{
   F32 basePriority = Parent::getUpdatePriority(connection, updateMask, updateSkips);

   // Lower priority for initial update.  This is to work around network-heavy loading of levels
   // with many LineItems, which will stall the client and prevent you from moving your ship
   if(isInitialUpdate())
      return basePriority - 1000.f;

   // Normal priority otherwise so Geom changes are immediately visible to all clients
   return basePriority;
}


S32 LineItem::getWidth() const
{
   return mWidth;
}


void LineItem::setWidth(S32 width, S32 min, S32 max)
{
   // Bounds check
   if(width < min)
      width = min;
   else if(width > max)
      width = max; 

   mWidth = width; 
}


void LineItem::setWidth(S32 width) 
{         
   setWidth(width, LineItem::MIN_LINE_WIDTH, LineItem::MAX_LINE_WIDTH);
}


void LineItem::changeWidth(S32 amt)
{
   S32 width = mWidth;

   if(amt > 0)
      width += amt - (S32) width % amt;    // Handles rounding
   else
   {
      amt *= -1;
      width -= ((S32) width % amt) ? (S32) width % amt : amt;      // Dirty, ugly thing
   }

   setWidth(width);
   onGeomChanged();
}


void LineItem::setGeom(lua_State *L, S32 stackIndex)
{
   Parent::setGeom(L, stackIndex);

   if(!isGhost())
      s2cSetGeom(*GeomObject::getOutline());
}


void LineItem::onGeomChanged()
{
   Parent::onGeomChanged();
}


#ifndef ZAP_DEDICATED

// Get the menu looking like what we want
bool LineItem::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(mGlobal ? 1 : 0);

   return true;
}


// Retrieve the values we need from the menu
void LineItem::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mGlobal = attributeMenu->getMenuItem(0)->getIntValue();    // Returns 0 or 1
}


// Render some attributes when item is selected but not being edited
void LineItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("Global");    values.push_back(mGlobal ? "Yes" : "No");
}

#endif


const char *LineItem::getOnScreenName()     const  { return "Line";      }
const char *LineItem::getPrettyNamePlural() const  { return "LineItems"; }
const char *LineItem::getOnDockName()       const  { return "LineItem";  }
const char *LineItem::getEditorHelpString() const  { return "Draws a line on the map.  Visible only to team, or to all if neutral."; }

bool LineItem::hasTeam()      { return true; }
bool LineItem::canBeHostile() { return true; }
bool LineItem::canBeNeutral() { return true; }


/////
// Lua interface

/**
 * @luaclass LineItem
 * 
 * @brief Decorative line visible to one or all teams. Has no specific game
 * function.
 * 
 * @descr If a non-global LineItem is assigned to a team, it will only be
 * visible to players on that team. If the LineItem is neutral (that is, `team
 * == Team.Neutral`, the default), it will be visible to all players regardless
 * of team or globalness.
 * 
 * @geom The geometry of a LineItem is a polyline (i.e. 2 or more points)
 */
//               Fn name       Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
      METHOD(CLASS, setGlobal, ARRAYDEF({{ BOOL,    END }}), 1 ) \
      METHOD(CLASS, getGlobal, ARRAYDEF({{          END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(LineItem, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(LineItem, LUA_METHODS);

#undef LUA_METHODS


/**
 * @luafunc void LineItem::setGlobal(bool global)
 *
 * @brief Sets the LineItem's global parameter.
 *
 * @descr LineItems are normally viewable by all players in a game. If you wish
 * to only let the LineItem be viewable to the owning team, set to `false`. Make
 * sure you call setTeam() on the LineItem first. Global is on by default.
 *
 * @param global `false` if this LineItem should be viewable only by the owning
 * team, otherwise viewable by all teams.
 */
S32 LineItem::lua_setGlobal(lua_State *L)
{
   checkArgList(L, functionArgs, "LineItem", "setGlobal");
   mGlobal = getBool(L, 1);

   setMaskBits(0x80000000);  // Update to clients,  dummy mask because of no mask bits used on packUpdate

   return 0;
}


/**
 * @luafunc bool LineItem::getGlobal()
 *
 * @brief Returns the LineItem's global parameter.
 *
 * @return 'true' if global is enabled, 'false' if not.
 */
S32 LineItem::lua_getGlobal(lua_State *L)
{
   return returnBool(L, mGlobal);
}


const char *LineItem::luaClassName = "LineItem";
REGISTER_LUA_SUBCLASS(LineItem, BfObject);

};
