//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "barrier.h"

#include "WallSegmentManager.h"
#include "BotNavMeshZone.h"         // For BufferRadius
#include "gameObjectRender.h"
#include "game.h"
#include "Colors.h"
#include "stringUtils.h"
#include "GeomUtils.h"

#include "tnlLog.h"

#include <cmath>


using namespace TNL;

namespace Zap
{

using namespace LuaArgs;

Vector<Point> Barrier::mRenderLineSegments;



// Constructor
WallRec::WallRec(F32 width, bool solid, const Vector<F32> &verts)
{
   this->width = width;
   this->solid = solid;

   this->verts.resize(verts.size());

   for(S32 i = 0; i < verts.size(); i++)
      this->verts[i] = verts[i];
}


// Constructor
WallRec::WallRec(const WallItem *wallItem)
{
   width = (F32)wallItem->getWidth();
   solid = false;

   for(S32 i = 0; i < wallItem->getVertCount(); i++)
   {
      verts.push_back(wallItem->getVert(i).x);
      verts.push_back(wallItem->getVert(i).y);
   }
}


// Constructor
WallRec::WallRec(const PolyWall *polyWall)
{
   width = 1;      // Doesn't really matter... will be ignored
   solid = true;

   for(S32 i = 0; i < polyWall->getVertCount(); i++)
   {
      verts.push_back(polyWall->getVert(i).x);
      verts.push_back(polyWall->getVert(i).y);
   }
}


// Runs on server or on client, never in editor
// Generates a list of barriers, which are then added to the game one-by-one
// Barriers will either be a simple 2-point segment, or a longer list of vertices defining a polygon
void WallRec::constructWalls(Game *game) const
{
   Vector<Point> vec = floatsToPoints(verts);

   if(vec.size() < 2)
      return;

   if(solid)   // This is a polywall
   {
      if(vec.first() == vec.last())      // Does our barrier form a closed loop?
         vec.erase(vec.size() - 1);      // If so, remove last vertex

      Barrier *b = new Barrier(vec, width, true);
      b->addToGame(game, game->getGameObjDatabase());
   }
   else        // This is a standard series of segments
   {
      // First, fill a vector with barrier segments
      Vector<Point> barrierEnds;
      constructBarrierEndPoints(&vec, width, barrierEnds);

      Vector<Point> pts;
      // Then add individual segments to the game
      for(S32 i = 0; i < barrierEnds.size(); i += 2)
      {
         pts.clear();
         pts.push_back(barrierEnds[i]);
         pts.push_back(barrierEnds[i+1]);

         Barrier *b = new Barrier(pts, width, false);    // false = not solid
         b->addToGame(game, game->getGameObjDatabase());
      }
   }
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor --> gets called from constructBarriers above
Barrier::Barrier(const Vector<Point> &points, F32 width, bool solid)
{
   mObjectTypeNumber = BarrierTypeNumber;
   mPoints = points;

   if(points.size() < 2)      // Invalid barrier!
   {
      //delete this;    // Sam: cannot "delete this" in constructor, as "new" still returns non-NULL address
      logprintf(LogConsumer::LogWarning, "Invalid barrier detected (has only one point).  Disregarding...");
      return;
   }

   Rect extent(points);

   F32 w = abs(width);

   mWidth = w;                      // Must be positive to avoid problem with bufferBarrierForBotZone
   w = w * 0.5f + 1;                // Divide by 2 to avoid double size extents, add 1 to avoid rounding errors

   if(points.size() == 2)           // It's a regular segment, need to make a little larger to accomodate width
      extent.expand(Point(w, w));


   setExtent(extent);

   mSolid = solid;

   if(mSolid)  // Polywall
   {
      if(isWoundClockwise(mPoints))         // All walls must be CCW to clip correctly
         mPoints.reverse();

      Triangulate::Process(mPoints, mRenderFillGeometry);

      if(mRenderFillGeometry.size() == 0)    // Geometry is bogus; perhaps duplicated points, or other badness
      {
         logprintf(LogConsumer::LogWarning, "Invalid barrier detected (polywall with invalid geometry).  Disregarding...");
         return;
      }

      setNewGeometry(geomPolygon);
   }
   else     // Normal wall
   {
      if(mPoints.size() == 2 && mPoints[0] == mPoints[1])      // Test for zero-length barriers
         mPoints[1] += Point(0, 0.5);                          // Add vertical vector of half a point so we can see outline in-game

      if(mPoints.size() == 2 && mWidth != 0)                   // It's a regular segment, so apply width
         expandCenterlineToOutline(mPoints[0], mPoints[1], mWidth, mRenderFillGeometry);     // Fills mRenderFillGeometry with 4 points

      setNewGeometry(geomPolyLine);
   }

   // Outline is the same for regular walls and polywalls
   mRenderOutlineGeometry = getCollisionPoly(); 

   GeomObject::setGeom(*mRenderOutlineGeometry);
}

// Destructor
Barrier::~Barrier()
{
   // Do nothing
}


// Processes mPoints and fills polyPoints 
const Vector<Point> *Barrier::getCollisionPoly() const
{
   if(mSolid)
      return &mPoints;
   else
      return &mRenderFillGeometry;
}


bool Barrier::collide(BfObject *otherObject)
{
   return true;
}


// Server only -- fills points
void Barrier::getBufferForBotZone(F32 bufferRadius, Vector<Point> &points) const
{
   // Use a clipper library to buffer polywalls; should be counter-clockwise by here
   if(mSolid)
      offsetPolygon(&mPoints, points, bufferRadius);

   // If a barrier, do our own buffer
   // Puffs out segment to the specified width with a further buffer for bot zones, has an inset tangent corner cut
   else
   {
      const Point &start = mPoints[0];
      const Point &end   = mPoints[1];
      Point difference   = end - start;

      Point crossVector(difference.y, -difference.x);          // Create a point whose vector from 0,0 is perpenticular to the original vector
      crossVector.normalize((mWidth * 0.5f) + bufferRadius);   // Reduce point so the vector has length of barrier width + ship radius

      Point parallelVector(difference.x, difference.y);        // Create a vector parallel to original segment
      parallelVector.normalize(bufferRadius);                  // Reduce point so vector has length of ship radius

      // For octagonal zones
      //   create extra vectors that are offset full offset to create 'cut' corners
      //   (FloatSqrtHalf * BotNavMeshZone::BufferRadius)  creates a tangent to the radius of the buffer
      //   we then subtract a little from the tangent cut to shorten the buffer on the corners and allow zones to be created when barriers are close
      Point crossPartial = crossVector;
      crossPartial.normalize((FloatSqrtHalf * bufferRadius) + (mWidth * 0.5f) - (0.3f * bufferRadius));

      Point parallelPartial = parallelVector;
      parallelPartial.normalize((FloatSqrtHalf * bufferRadius) - (0.3f * bufferRadius));

      // Now add/subtract perpendicular and parallel vectors to buffer the segments
      points.push_back((start - parallelVector)  + crossPartial);
      points.push_back((start - parallelPartial) + crossVector);
      points.push_back((end   + parallelPartial) + crossVector);
      points.push_back((end   + parallelVector)  + crossPartial);
      points.push_back((end   + parallelVector)  - crossPartial);
      points.push_back((end   + parallelPartial) - crossVector);
      points.push_back((start - parallelPartial) - crossVector);
      points.push_back((start - parallelVector)  - crossPartial);
   }
}


void Barrier::clearRenderItems()
{
   mRenderLineSegments.clear();
}


// Merges wall outlines together, client only
// This is used for barriers and polywalls
void Barrier::prepareRenderingGeometry(Game *game)    // static
{
   mRenderLineSegments.clear();

   Vector<DatabaseObject *> barrierList;

   game->getGameObjDatabase()->findObjects((TestFunc)isWallType, barrierList);

   clipRenderLinesToPoly(barrierList, mRenderLineSegments);
}


// Clears out overlapping barrier lines for better rendering appearance, modifies lineSegmentPoints.
// This is effectively called on every pair of potentially intersecting barriers, and lineSegmentPoints gets 
// refined as each additional intersecting barrier gets processed.
void Barrier::clipRenderLinesToPoly(const Vector<DatabaseObject *> &barrierList, Vector<Point> &lineSegmentPoints)  
{
   Vector<Vector<Point> > solution;

   unionBarriers(barrierList, solution);

   unpackPolygons(solution, lineSegmentPoints);
}


// Combines multiple barriers into a single complex polygon... fills solution
bool Barrier::unionBarriers(const Vector<DatabaseObject *> &barriers, Vector<Vector<Point> > &solution)
{
   Vector<const Vector<Point> *> inputPolygons;
   Vector<Point> points;

   for(S32 i = 0; i < barriers.size(); i++)
   {
      if(barriers[i]->getObjectTypeNumber() != BarrierTypeNumber)
         continue;

      inputPolygons.push_back(static_cast<Barrier *>(barriers[i])->getCollisionPoly());
   }

   return mergePolys(inputPolygons, solution);
}


// Render wall fill only for this wall; all edges rendered in a single pass later
void Barrier::renderLayer(S32 layerIndex)
{
#ifndef ZAP_DEDICATED
   if(layerIndex == 0)           // First pass: draw the fill
      renderWallFill(&mRenderFillGeometry, *getGame()->getSettings()->getWallFillColor(), mSolid);
#endif
}


// Render all edges for all barriers... faster to do it all at once than try to sort out whose edges are whose
void Barrier::renderEdges(S32 layerIndex, const Color &outlineColor)  // static
{
   if(layerIndex == 1)
      renderWallEdges(mRenderLineSegments, outlineColor);
}


S32 Barrier::getRenderSortValue()
{
   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

// WallItem is child of LineItem... the only thing LineItem brings to the party is width

// Combined C++/Lua constructor
WallItem::WallItem(lua_State *L)
{
   mObjectTypeNumber = WallItemTypeNumber;
   mWidth = Barrier::DEFAULT_BARRIER_WIDTH;
   mAlreadyAdded = false;

   setNewGeometry(geomPolyLine);
   
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { LINE, INT, END }}, 2 };
      S32 profile = checkArgList(L, constructorArgList, "WallItem", "constructor");
      if(profile == 1)
      {
         setWidth(lua_tointeger(L, -1));     // Grab this before it gets popped
         lua_pop(L, 1);                      // Clean up stack for setGeom, which only expects points
         lua_setGeom(L);
      }
   }

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
WallItem::~WallItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


WallItem *WallItem::clone() const
{
   return new WallItem(*this);
}


// Client (i.e. editor) only; walls processed in ServerGame::processPseudoItem() on server
// BarrierMaker <width> <x> <y> <x> <y> ...
bool WallItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 6)         // "BarrierMaker" keyword, width, and two or more x,y pairs
      return false;

   setWidth(atoi(argv[1]));

   readGeom(argc, argv, 2, game->getLegacyGridSize());

   updateExtentInDatabase();

   return true;
}


string WallItem::toLevelCode() const
{
   return appendId("BarrierMaker") + " " + itos(getWidth()) + " " + geomToLevelCode();
}


static void setWallSelected(GridDatabase *database, S32 serialNumber, bool selected)
{
   // Find the associated segment(s) and mark them as selected (or not)
   if(database)
      database->getWallSegmentManager()->setSelected(serialNumber, selected);
}


static F32 getWallEditorRadius(F32 currentScale)
{
   return (F32)EditorObject::VERTEX_SIZE;   // Keep vertex hit targets the same regardless of scale
}


void WallItem::changeWidth(S32 amt)
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


void WallItem::onGeomChanged()
{
   // Fill extendedEndPoints from the vertices of our wall's centerline, or from PolyWall edges
   processEndPoints();

   GridDatabase *db = getDatabase();

   if(db)
      db->getWallSegmentManager()->onWallGeomChanged(db, this, isSelected(), getSerialNumber());

   onPointsChanged();        // Recalculates centroid
   Parent::onGeomChanged();
}


void WallItem::onItemDragging()
{
   // Do nothing -- this is here to override Parent::onItemDragging(), onGeomChanged() should only be called after move is complete
}


// WallItems are not really added to the game in the sense of other objects; rather their geometry is used
// to create Barriers that are added directly.  Here we will mark the item as added (to catch errors in Lua
// scripts that attempt to modify an added item), but we have no need to pass the event handler up the stack
// to superclass event handlers.
void WallItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
   mAlreadyAdded = true;
}


// Only called in editor during preview mode -- basicaly prevents parent class from rendering spine of wall
void WallItem::render()
{
   // Do nothing
}


void WallItem::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
#ifndef ZAP_DEDICATED
   const Color *color = NULL;
   if(!isSelected() && !isLitUp())
      color = getEditorRenderColor();

   renderWallOutline(this, getOutline(), color, currentScale, snappingToWallCornersEnabled, renderVertices);
#endif
}


void WallItem::processEndPoints()
{
#ifndef ZAP_DEDICATED
   constructBarrierEndPoints(getOutline(), (F32)getWidth(), extendedEndPoints);     // Fills extendedEndPoints
#endif
}


Rect WallItem::calcExtents()
{
   // mExtent was already calculated when the wall was inserted into the segmentManager...
   // All we need to do here is override the default calcExtents, to avoid clobbering our already good mExtent.
   return getExtent();     
}


const char *WallItem::getOnScreenName()     { return "Wall";  }
const char *WallItem::getOnDockName()       { return "Wall";  }
const char *WallItem::getPrettyNamePlural() { return "Walls"; }
const char *WallItem::getEditorHelpString() { return "Walls define the general form of your level."; }

const char *WallItem::getInstructionMsg(S32 attributeCount) { return "[+] and [-] to change width"; }


void WallItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{ 
   keys.push_back("Width");   values.push_back(itos(getWidth()));
}


bool WallItem::hasTeam()      { return false; }
bool WallItem::canBeHostile() { return false; }
bool WallItem::canBeNeutral() { return false; }


const Color *WallItem::getEditorRenderColor() const
{
   return &Colors::gray50;    // Color of wall spine in editor
}


// Size of object in editor 
F32 WallItem::getEditorRadius(F32 currentScale)
{
   return getWallEditorRadius(currentScale);
}


void WallItem::scale(const Point &center, F32 scale)
{
   Parent::scale(center, scale);

   // Adjust the wall thickness
   // Scale might not be accurate due to conversion to S32
   setWidth(S32(getWidth() * scale));
}


// Needed to provide a valid signature
S32 WallItem::getWidth() const
{
   return mWidth;
}


void WallItem::setWidth(S32 width) 
{         
   if(width < Barrier::MIN_BARRIER_WIDTH)
      mWidth = Barrier::MIN_BARRIER_WIDTH;

   else if(width > Barrier::MAX_BARRIER_WIDTH)
      mWidth = Barrier::MAX_BARRIER_WIDTH; 

   else
      mWidth = width; 
}


void WallItem::setSelected(bool selected)
{
   Parent::setSelected(selected);
   
   // Find the associated segment(s) and mark them as selected (or not)
   setWallSelected(getDatabase(), getSerialNumber(), selected);
}


// Here to provide a valid signature in WallItem
void WallItem::addToGame(Game *game, GridDatabase *database)
{
   Parent::addToGame(game, database);

   // Convert the wallItem in to a wallRec, an abbreviated form of wall that represents both regular walls and polywalls, and 
   // is convenient to transmit to the clients
   game->addWall(WallRec(this));

   onAddedToGame(game);
}


/////
// Lua interface
/**
 * @luafunc WallItem::WallItem()
 * @luafunc WallItem::WallItem(geom geometry, int thickness)
 * @luaclass WallItem
 * 
 * @brief Traditional wall item.
 * 
 * @descr A WallItem is a traditional wall consisting of a series of
 * straight-line segments. WallItems have a width setting that expands the walls
 * outward on the edges, but not the ends. The game may make slight adjustments
 * to the interior vertices of a wall to improve visual appearance. Collinear
 * vertices may be deleted to simplify wall geometry.
 * 
 * @geom WallItem geometry consists of two or more points forming a linear
 * sequence, each consecutive pair defining a straight-line segment. WallItem
 * geometry can cross or form loops with no adverse consequences.
 */
//               Fn name       Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getWidth,     ARRAYDEF({{      END }}), 1 ) \
   METHOD(CLASS, setWidth,     ARRAYDEF({{ INT, END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(WallItem, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(WallItem, LUA_METHODS);

#undef LUA_METHODS


const char *WallItem::luaClassName = "WallItem";
REGISTER_LUA_SUBCLASS(WallItem, BfObject);

/**
 * @luafunc int WallItem::getWidth()
 * 
 * @brief Returns the WallItem's width setting.
 * 
 * @descr Walls have a default width of 50.
 * 
 * @return The WallItem's width.
 */
S32 WallItem::lua_getWidth(lua_State *L)     
{ 
   return returnInt(L, getWidth()); 
}


/**
 * @luafunc WallItem::setWidth(int width)
 * 
 * @brief Sets the WallItem's width.
 * 
 * @descr Walls have a default width of 50.
 * 
 * @param width The WallItem's new width.
 */
S32 WallItem::lua_setWidth(lua_State *L)     
{ 
   checkIfHasBeenAddedToTheGame(L);

   checkArgList(L, functionArgs, "WallItem", "setWidth");

   setWidth(getInt(L, 1));

   return 0; 
}


void WallItem::checkIfHasBeenAddedToTheGame(lua_State *L)
{
   if(mAlreadyAdded)
   {
      ScriptContext context = getScriptContext(L);

      if(context != PluginContext)     // Plugins can alter walls that are already in-game... levelgens cannot
      {
         const char *msg = "Can't modify a wall that's already been added to a game!";
         logprintf(LogConsumer::LogError, msg);
         THROW_LUA_EXCEPTION(L, msg);
      }
   }
}


// Some Lua method overrides.  Because walls are... special.

S32 WallItem::lua_setPos(lua_State *L)
{
   checkIfHasBeenAddedToTheGame(L);
   return Parent::lua_setPos(L);
}


S32 WallItem::lua_setGeom(lua_State *L)
{
   checkIfHasBeenAddedToTheGame(L);
   return Parent::lua_setGeom(L);
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(PolyWall);

/**
 * @luafunc PolyWall::PolyWall()
 * @luafunc PolyWall::PolyWall(polyGeom)
 */
// Combined Lua/C++ constructor
PolyWall::PolyWall(lua_State *L)
{
   mObjectTypeNumber = PolyWallTypeNumber;
   mAlreadyAdded = false;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
   
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { POLY, END }}, 2 };

      S32 profile = checkArgList(L, constructorArgList, "PolyWall", "constructor");

      if(profile == 1)
         lua_setGeom(L);
   }
}


PolyWall::~PolyWall()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


PolyWall *PolyWall::clone() const
{
   PolyWall *polyWall = new PolyWall(*this);
   return polyWall;
}


S32 PolyWall::getRenderSortValue()
{
   return -1;
}


void PolyWall::renderDock()
{
   renderPolygonFill(getFill(), &Colors::EDITOR_WALL_FILL_COLOR);
   renderPolygonOutline(getOutline(), getGame()->getSettings()->getWallOutlineColor());
}


bool PolyWall::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 7)            // Need "Polywall" keyword, and at least 3 points
      return false;

   S32 offset = 0;

   if(!stricmp(argv[0], "BarrierMakerS"))
   {
      logprintf(LogConsumer::LogLevelError, "BarrierMakerS has been deprecated.  Please use PolyWall instead.");
      offset = 1;
   }

   readGeom(argc, argv, 1 + offset, game->getLegacyGridSize());

   return true;
}


string PolyWall::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + geomToLevelCode();
}


// Size of object in editor 
F32 PolyWall::getEditorRadius(F32 currentScale)
{
   return getWallEditorRadius(currentScale);
}


const char *PolyWall::getOnScreenName()     { return "PolyWall";  }
const char *PolyWall::getOnDockName()       { return "PolyWall";  }
const char *PolyWall::getPrettyNamePlural() { return "PolyWalls"; }
const char *PolyWall::getEditorHelpString() { return "Polygonal wall item lets you be creative with your wall design."; }


void PolyWall::setSelected(bool selected)
{
   Parent::setSelected(selected);

   setWallSelected(getDatabase(), getSerialNumber(), selected);
}


// Only called from editor
void PolyWall::onGeomChanged()
{
   GridDatabase *db = getDatabase();

   if(db)      // db might be NULL if PolyWall hasn't yet been added to the editor (e.g. if it's still a figment of Lua's fancy)
   {
      db->getWallSegmentManager()->onWallGeomChanged(db, this, isSelected(), getSerialNumber());
      Parent::onGeomChanged();
   }
}


void PolyWall::addToGame(Game *game, GridDatabase *database)
{
   Parent::addToGame(game, database);

   // Convert the wallItem in to a wallRec, an abbreviated form of wall that represents both regular walls and polywalls, and 
   // is convenient to transmit to the clients
   game->addWall(WallRec(this));

   onAddedToGame(game);
}


void PolyWall::onItemDragging()
{
   // Do nothing -- this is here to override PolygonObject::onItemDragging(), onGeomChanged() should only be called after move is complete
}


// PolyWalls are not really added to the game in the sense of other objects; rather their geometry is used
// to create Barriers that are added directly.  Here we will mark the item as added (to catch errors in Lua
// scripts that attempt to modify an added item), but we have no need to pass the event handler up the stack
// to superclass event handlers.
void PolyWall::onAddedToGame(Game *game)
{
  Parent::onAddedToGame(game);
  mAlreadyAdded = true;
}


/////
// Lua interface

/**
 * @luaclass PolyWall
 * 
 * @brief Polygonal wall item.
 * 
 * @descr A PolyWall is a wall consisting of a filled polygonal shape. 
 * 
 * @geom PolyWall geometry is a typical polygon.
 */
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(PolyWall, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(PolyWall, LUA_METHODS);

#undef LUA_METHODS


const char *PolyWall::luaClassName = "PolyWall";
REGISTER_LUA_SUBCLASS(PolyWall, BfObject);


void PolyWall::checkIfHasBeenAddedToTheGame(lua_State *L)
{
   if(mAlreadyAdded)
   {
      ScriptContext context = getScriptContext(L);

      if(context != PluginContext)     // Plugins can alter walls that are already in-game... levelgens cannot
      {
         const char *msg = "Can't modify a PolyWall that's already been added to a game!";
         logprintf(LogConsumer::LogError, msg);
         THROW_LUA_EXCEPTION(L, msg);
      }
   }
}


// Lua method overrides.  Because walls are... special.

S32 PolyWall::lua_setPos(lua_State *L)
{
   checkIfHasBeenAddedToTheGame(L);
   return Parent::lua_setPos(L);
}


S32 PolyWall::lua_setGeom(lua_State *L)
{
   checkIfHasBeenAddedToTheGame(L);
   return Parent::lua_setGeom(L);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
WallEdge::WallEdge(const Point &start, const Point &end) 
{ 
   mStart = start; 
   mEnd   = end; 
   mPoints.reserve(2);
   mPoints.push_back(start);
   mPoints.push_back(end);

   setExtent(Rect(start, end));

   // Set some things required by DatabaseObject
   mObjectTypeNumber = WallEdgeTypeNumber;
}


// Destructor
WallEdge::~WallEdge()
{
    // Make sure object is out of the database
   removeFromDatabase(false); 
}


Point *WallEdge::getStart() { return &mStart; }
Point *WallEdge::getEnd()   { return &mEnd;   }


const Vector<Point> *WallEdge::getCollisionPoly() const
{
   return &mPoints;
}


bool WallEdge::getCollisionCircle(U32 stateIndex, Point &point, float &radius) const
{
   return false;
}


////////////////////////////////////////
////////////////////////////////////////

// Regular constructor
WallSegment::WallSegment(GridDatabase *gridDatabase, const Point &start, const Point &end, F32 width, S32 owner) 
{ 
   // Calculate segment corners by expanding the extended end points into a rectangle
   expandCenterlineToOutline(start, end, width, mCorners);  // ==> Fills mCorners 
   init(gridDatabase, owner);
}


// PolyWall constructor
WallSegment::WallSegment(GridDatabase *gridDatabase, const Vector<Point> &points, S32 owner)
{
   mCorners = points;

   if(isWoundClockwise(points))
      mCorners.reverse();

   init(gridDatabase, owner);
}


// Intialize, only called from constructors above
void WallSegment::init(GridDatabase *database, S32 owner)
{
   // Recompute the edges based on our new corner points
   resetEdges();   

   mObjectTypeNumber = WallSegmentTypeNumber;

   setExtent(Rect(mCorners));

   // Add item to database, set its extents
   addToDatabase(database);
   
   // Drawing filled wall requires that points be triangluated
   Triangulate::Process(mCorners, mTriangulatedFillPoints);    // ==> Fills mTriangulatedFillPoints

   mOwner = owner; 
   invalid = false; 
   mSelected = false;
}


// Destructor
WallSegment::~WallSegment()
{ 
   // Make sure object is out of the database -- but don't delete it since we're destructing
   if(getDatabase())
      getDatabase()->removeFromDatabase(this, false); 
}


// Returns serial number of owner
S32 WallSegment::getOwner()
{
   return mOwner;
}


void WallSegment::invalidate()
{
   invalid = true;
}
 

// Resets edges of a wall segment to their factory settings; i.e. 4 simple walls representing a simple outline
void WallSegment::resetEdges()
{
   cornersToEdges(mCorners, mEdges);
}


void WallSegment::renderFill(const Point &offset, const Color &color)
{
#ifndef ZAP_DEDICATED
   if(mSelected)
      renderWallFill(&mTriangulatedFillPoints, color, offset, true);       // Use true because all segment fills are triangulated
   else
      renderWallFill(&mTriangulatedFillPoints, color, true);
#endif
}


bool WallSegment::isSelected()
{
   return mSelected;
}


void WallSegment::setSelected(bool selected)
{
   mSelected = selected;
}


const Vector<Point> *WallSegment::getCorners()                { return &mCorners; }
const Vector<Point> *WallSegment::getEdges()                  { return &mEdges; }
const Vector<Point> *WallSegment::getTriangulatedFillPoints() { return &mTriangulatedFillPoints; }


const Vector<Point> *WallSegment::getCollisionPoly() const
{
   return &mCorners;
}


bool WallSegment::getCollisionCircle(U32 stateIndex, Point &point, float &radius) const
{
   return false;
}


};
