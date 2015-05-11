//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "BotNavMeshZone.h"

#include "ship.h"                   // For Ship::CollisionRadius
#include "Teleporter.h"             // For Teleporter::TELEPORTER_RADIUS
#include "gameObjectRender.h"
#include "barrier.h"                // For Barrier methods in generating zones
#include "EngineeredItem.h"         // For Turret and ForceFieldProjector methods in generating zones
#include "GeomUtils.h"
#include "MathUtils.h"

#include "tnlLog.h"

#include "../recast/RecastAlloc.h"
#include <clipper.hpp>

#include <vector>
#include <math.h>


// triangulate wants quit on error
// will probably crash on error when this function exits
//extern "C" void triexit(int status)
//{
//   TNLAssert(false, "Triangulate error");
//   logprintf(LogConsumer::LogError, "While generating bot zones, Triangulate error");
//   // here may be a good place to print lots of data.
//   //throw;  // any better ways?
//}



namespace Zap
{

// Declare our statics
static const S32 MAX_ZONES = 10000;                              // Don't make this go above S16 max - 1 (32,766), AStar::findPath is limited
const S32 BotNavMeshZone::BufferRadius = Ship::CollisionRadius;  // Radius to buffer objects when creating the holes for zones

// Extra padding around the game extents to allow outsize zones to be created.
// Make sure we always have 50 for good measure
const S32 BotNavMeshZone::LevelZoneBuffer = MAX(BufferRadius * 2, 50);


// Constructor
BotNavMeshZone::BotNavMeshZone(S32 id)
{
   mObjectTypeNumber = BotNavMeshZoneTypeNumber;
   setNewGeometry(geomPolygon);

   mZoneId = id;
}


// Destructor
BotNavMeshZone::~BotNavMeshZone()
{
   removeFromDatabase(false);
}


// Return the center of this zone
Point BotNavMeshZone::getCenter()
{
   return getExtent().getCenter();     // Good enough for government work
}


void BotNavMeshZone::renderLayer(S32 layerIndex)    
{
#ifndef ZAP_DEDICATED
   if(layerIndex == 0)
      renderNavMeshZone(getOutline(), getFill(), getCentroid(), mZoneId);

   else if(layerIndex == 1)
      renderNavMeshBorders(mNeighbors);
#endif
}


// Use this to help keep track of which robots are where
// Only gets run on the server, never on client, obviously, because that's where the bots are!!!
//bool BotNavMeshZone::collide(BfObject *hitObject)
//{
//   // This does not get run anymore, it is in a seperate database.
//   if(hitObject->getObjectTypeNumber() == RobotShipTypeNumber)     // Only care about robots...
//   {
//      Robot *r = (Robot *) hitObject;
//      r->setCurrentZone(mZoneId);
//   }
//   return false;
//}



void BotNavMeshZone::addToZoneDatabase(GridDatabase *botZoneDatabase)
{
   setExtent(calcExtents());
   DatabaseObject::addToDatabase(botZoneDatabase);    // not a static, just looks like one from here!
}


// More precise boundary for precise collision detection
const Vector<Point> *BotNavMeshZone::getCollisionPoly() const
{
   return getOutline();
}


// Returns index of neighboring zone, or -1 if zone is not a neighbor
S32 BotNavMeshZone::getNeighborIndex(S32 zoneID)
{
   for(S32 i = 0; i < mNeighbors.size(); i++)
      if(mNeighbors[i].zoneID == zoneID)
         return i;

   return -1;
}


struct rcEdge
{
   unsigned short vert[2];    // from, to verts
   unsigned short poly[2];    // left, right poly
};


// Build connections between zones using the adjacency data created in recast
bool BotNavMeshZone::buildBotNavMeshZoneConnectionsRecastStyle(const Vector<BotNavMeshZone *> *allZones, 
                                                               rcPolyMesh &mesh, const Vector<S32> &polyToZoneMap)
{
   if(allZones->size() == 0)      // Nothing to do!
      return true;

   // We'll reuse these objects throughout the following block, saving the cost of creating and destructing them
   Point bordStart, bordEnd, bordCen;
   Rect rect;
   NeighboringZone neighbor;

   /////////////////////////////
   // Based on recast's interpretation of code by Eric Lengyel:
   // http://www.terathon.com/code/edges.php
   
   int maxEdgeCount = mesh.npolys*mesh.nvp;
   unsigned short* firstEdge = (unsigned short*)rcAlloc(sizeof(unsigned short)*(mesh.nverts + maxEdgeCount), RC_ALLOC_TEMP);
   if (!firstEdge)      // Allocation went bad
      return false;
   unsigned short* nextEdge = firstEdge + mesh.nverts;
   int edgeCount = 0;
   
   rcEdge* edges = (rcEdge*)rcAlloc(sizeof(rcEdge)*maxEdgeCount, RC_ALLOC_TEMP);
   if (!edges)
   {
      rcFree(firstEdge);
      return false;
   }
   
   for(int i = 0; i < mesh.nverts; i++)
      firstEdge[i] = RC_MESH_NULL_IDX;
   
   // First process edges where 1st node < 2nd node
   for(int i = 0; i < mesh.npolys; ++i)
   {
      unsigned short* t = &mesh.polys[i*mesh.nvp];

      // Skip "missing" polygons
      if(*t == U16_MAX)
         continue;

      for (int j = 0; j < mesh.nvp; ++j)
      {
         unsigned short v0 = t[j];           // jth vert

         if(v0 == RC_MESH_NULL_IDX)
            break;

         unsigned short v1 = (j+1 >= mesh.nvp || t[j+1] == RC_MESH_NULL_IDX) ? t[0] : t[j+1];  // j+1th vert

         if (v0 < v1)      
         {
            rcEdge& edge = edges[edgeCount];       // edge connecting v0 and v1
            edge.vert[0] = v0;
            edge.vert[1] = v1;
            edge.poly[0] = (unsigned short)i;      // left poly
            edge.poly[1] = (unsigned short)i;      // right poly, will be recalced later -- the fact that both are the same is used as a marker

            nextEdge[edgeCount] = firstEdge[v0];        // Next edge on the previous vert now points to whatever was in firstEdge previously
            firstEdge[v0] = (unsigned short)edgeCount;  // First edge of this vert 

            edgeCount++;                           // edgeCount never resets -- each edge gets a unique id
         }
      }
   }
   
   // Now process edges where 2nd node is > 1st node
   for(int i = 0; i < mesh.npolys; ++i)
   {
      unsigned short* t = &mesh.polys[i*mesh.nvp];

      // Skip "missing" polygons
      if(*t == U16_MAX)
         continue;

      for(int j = 0; j < mesh.nvp; ++j)
      {
         unsigned short v0 = t[j];
         if(v0 == RC_MESH_NULL_IDX)
            break;

         unsigned short v1 = (j+1 >= mesh.nvp || t[j+1] == RC_MESH_NULL_IDX) ? t[0] : t[j+1];

         if(v0 > v1)      
         {
            for(unsigned short e = firstEdge[v1]; e != RC_MESH_NULL_IDX; e = nextEdge[e])
            {
               rcEdge& edge = edges[e];
               if(edge.vert[1] == v0 && edge.poly[0] == edge.poly[1])
               {
                  edge.poly[1] = (unsigned short)i;
                  break;
               }
            }
         }
      }
   }

   // Now create our neighbor data
   for(int i = 0; i < edgeCount; i++)
   {
      const rcEdge& e = edges[i];

      if(e.poly[0] != e.poly[1])      // Should normally be the case
      {
         U16 *v;

         v = &mesh.verts[e.vert[0] * 2];
         neighbor.borderStart.set(v[0] - mesh.offsetX, v[1] - mesh.offsetY);

         v = &mesh.verts[e.vert[1] * 2];
         neighbor.borderEnd.set(v[0] - mesh.offsetX, v[1] - mesh.offsetY);

         neighbor.borderCenter.set((neighbor.borderStart + neighbor.borderEnd) * 0.5);

         neighbor.zoneID = polyToZoneMap[e.poly[1]];  
         allZones->get(polyToZoneMap[e.poly[0]])->mNeighbors.push_back(neighbor);  // (copies neighbor implicitly)

         neighbor.zoneID = polyToZoneMap[e.poly[0]];   
         allZones->get(polyToZoneMap[e.poly[1]])->mNeighbors.push_back(neighbor);
      }
   }
   
   rcFree(firstEdge);
   rcFree(edges);
   
   return true;
}


static Vector<DatabaseObject *> zones;

// Returns index of zone containing specified point
static BotNavMeshZone *findZoneTouchingCircle(const GridDatabase *botZoneDatabase, const Point &centerPoint, F32 radius)
{
   Rect rect(centerPoint, radius);
   zones.clear();
   botZoneDatabase->findObjects(BotNavMeshZoneTypeNumber, zones, rect);

   const Vector<Point> *poly;
   Point c;

   // Pick the first zone within our radius
   for(S32 i = 0; i < zones.size(); i++)
   {
      BotNavMeshZone *zone = static_cast<BotNavMeshZone *>(zones[i]);
      poly = zone->getOutline();

      if(zone && polygonCircleIntersect(poly->address(), poly->size(), centerPoint, radius * radius, c))
         return zone;   
   }

   return NULL;
}


#ifdef TNL_DEBUG
#  define LOG_TIMER
#endif

static bool mergeBotZoneBuffers(const Vector<DatabaseObject *> &barriers,
                                const Vector<DatabaseObject *> &turrets,
                                const Vector<DatabaseObject *> &forceFieldProjectors, 
                                F32 bufferRadius,   PolyTree &solution)
{

   Vector<Vector<Point> > inputPolygons;

   // Add barriers (PolyWalls are Barriers on the server)
   for(S32 i = 0; i < barriers.size(); i++)
   {
      if(barriers[i]->getObjectTypeNumber() != BarrierTypeNumber)
         continue;

      Barrier *barrier = static_cast<Barrier *>(barriers[i]);

      inputPolygons.push_back(Vector<Point>());
      barrier->getBufferForBotZone(bufferRadius, inputPolygons.last());
   }

   // Add turrets
   for (S32 i = 0; i < turrets.size(); i++)
   {
      if(turrets[i]->getObjectTypeNumber() != TurretTypeNumber)
         continue;

      Turret *turret = static_cast<Turret *>(turrets[i]);

      inputPolygons.push_back(Vector<Point>());
      turret->getBufferForBotZone(bufferRadius, inputPolygons.last());
   }

   // Add forcefield projectors
   for (S32 i = 0; i < forceFieldProjectors.size(); i++)
   {
      if(forceFieldProjectors[i]->getObjectTypeNumber() != ForceFieldProjectorTypeNumber)
         continue;

      ForceFieldProjector *forceFieldProjector = static_cast<ForceFieldProjector *>(forceFieldProjectors[i]);

      inputPolygons.push_back(Vector<Point>());
      forceFieldProjector->getBufferForBotZone(bufferRadius, inputPolygons.last());
   }


   // Here we round the botzone points before clipper takes ahold.  This is because
   // the older editor would save identical points with floating point rounding errors.
   // These errors can sometimes create issues with clipping and triangulation, usually
   // by creating not strictly-simple polygons or self-intersecting lines - these then
   // crash poly2tri in triangulation.  For a good reference to these issues see:
   //    http://www.angusj.com/delphi/clipper/documentation/Docs/Overview/Rounding.htm
   //
   // This doesn't seem to be needed anymore since updating to clipper 6 with the
   // StrictlySimple(true) flag.  I decided to leave it because it does seem to make
   // clipper's job a little easier and saves some processor time
   //
   for(S32 i=0; i < inputPolygons.size(); i++)
      for(S32 j=0; j < inputPolygons[i].size(); j++)
      {
         inputPolygons[i][j].x = (F32)floor(inputPolygons[i][j].x);
         inputPolygons[i][j].y = (F32)floor(inputPolygons[i][j].y);
      }

   return mergePolysToPolyTree(inputPolygons, solution);
}


// Populate allZones -- we'll use this for efficiency, saving us the trouble of repeating this operation in multiple places.  
// We can retrieve them using BotNavMeshZone::getBotZones().
void BotNavMeshZone::populateZoneList(GridDatabase *botZoneDatabase, Vector<BotNavMeshZone *> *allZones)
{
   const Vector<DatabaseObject *> *objects = botZoneDatabase->findObjects_fast();

   allZones->resize(objects->size());

   for(S32 i = 0; i < objects->size(); i++)
      allZones->get(i) = static_cast<BotNavMeshZone *>(objects->get(i));
}


// Only runs on server
static void linkTeleportersBotNavMeshZoneConnections(const GridDatabase *botZoneDatabase,
                                                     const Vector<pair<Point, const Vector<Point> *> > &teleporterData)
{
   NeighboringZone neighbor;
   // Now create paths representing the teleporters
   Point origin, dest;

   F32 triggerRadius = F32(Teleporter::TELEPORTER_RADIUS - Ship::CollisionRadius);

   for(S32 i = 0; i < teleporterData.size(); i++)
   {
      origin = teleporterData[i].first;
      BotNavMeshZone *origZone = findZoneTouchingCircle(botZoneDatabase, origin, triggerRadius);

      if(origZone != NULL)
      for(S32 j = 0; j < teleporterData[i].second->size(); j++)     // Review each teleporter destination
      {
         dest = teleporterData[i].second->get(j);
         BotNavMeshZone *destZone = findZoneTouchingCircle(botZoneDatabase, dest, triggerRadius);

         if(destZone != NULL && origZone != destZone)      // Ignore teleporters that begin and end in the same zone
         {
            // Teleporter is one way path
            neighbor.zoneID = destZone->getZoneId();
            neighbor.borderStart.set(origin);
            neighbor.borderEnd.set(dest);
            neighbor.borderCenter.set(origin);

            // Teleport instantly, at no cost -- except this is wrong... if teleporter has multiple dests, actual cost could be quite high.
            // This should be the average of the costs of traveling from each dest zone to the target zone
            neighbor.distTo = 0;
            neighbor.center.set(origin);

            origZone->mNeighbors.push_back(neighbor);
         }
      }  // for loop iterating over teleporter dests
   } // for loop iterating over teleporters
}


// Server only
// Use the Triangle library to create zones.  Aggregate triangles with Recast
bool BotNavMeshZone::buildBotMeshZones(GridDatabase *botZoneDatabase, Vector<BotNavMeshZone *> *allZones,
                                       const Rect *worldExtents, const Vector<DatabaseObject *> &barrierList,
                                       const Vector<DatabaseObject *> &turretList, const Vector<DatabaseObject *> &forceFieldProjectorList,
                                       const Vector<pair<Point, const Vector<Point> *> > &teleporterData, bool triangulateZones)
{
#ifdef LOG_TIMER
   U32 starttime = Platform::getRealMilliseconds();
#endif

   Rect bounds(worldExtents);      // Modifiable copy
   allZones->deleteAndClear();

   bounds.expandToInt(Point(LevelZoneBuffer, LevelZoneBuffer));      // Provide a little breathing room

   // Make sure level isn't too big for zone generation, which uses 16 bit ints
   if(bounds.getHeight() >= (F32)U16_MAX || bounds.getWidth() >= (F32)U16_MAX)
   {
      logprintf(LogConsumer::LogLevelError, "Level too big for zone generation! (max allowed dimension is %d)", U16_MAX);
      return false;
   }

   Vector<F32> holes;
   PolyTree solution;

   // Merge bot zone buffers from barriers, turrets, and forcefield projectors
   // The Clipper library is the work horse here.  Its output is essential for the
   // triangulation.  The output contains the upscaled Clipper points (you will need to downscale)
   if(!mergeBotZoneBuffers(barrierList, turretList, forceFieldProjectorList, (F32)BufferRadius, solution))
      return false;


#ifdef LOG_TIMER
   U32 done1 = Platform::getRealMilliseconds();
#endif

   // Tessellate!
   // This will downscale the Clipper output and use poly2tri to triangulate
   Vector<Point> outputTriangles;  // Every 3 points is a triangle
   if(!Triangulate::processComplex(outputTriangles, bounds, solution))
      return false;

#ifdef LOG_TIMER
   U32 done2 = Platform::getRealMilliseconds();
#endif

   bool recastPassed = false;
   rcPolyMesh mesh;

   if(bounds.getWidth() < U16_MAX && bounds.getHeight() < U16_MAX)
   {
      mesh.offsetX = -1 * (int)floor(bounds.min.x + 0.5f);
      mesh.offsetY = -1 * (int)floor(bounds.min.y + 0.5f);

      // This works because bounds is always passed by reference.  Is this really needed?
      bounds.offset(Point(mesh.offsetX, mesh.offsetY));

      // Merge!  into convex polygons
      recastPassed = Triangulate::mergeTriangles(outputTriangles, mesh);
   }

   populateZoneList(botZoneDatabase, allZones);     // Populate allZones from botZoneDatabase

   // So here we are.  If recastPassed, our triangles were successfully aggregated into zones, but will need further polishing below.  If it failed 
   // (which will happen rarely, if ever), the aggregation failed and our zones are just the unaggregated raw triangles that we created before 
   // attempting mergeTriangles.  

   if(recastPassed)
   {
      BotNavMeshZone *botzone = NULL;
      bool addedZones = false;
   
      const S32 bytesPerVertex = sizeof(U16);      // Recast coords are U16s
      Vector<S32> polyToZoneMap;
      polyToZoneMap.resize(mesh.npolys);

      // Visualize rcPolyMesh
      for(S32 i = 0; i < mesh.npolys; i++)
      {
         S32 j = 0;
         botzone = NULL;

         while(j < mesh.nvp)
         {
            if(mesh.polys[(i * mesh.nvp + j)] == U16_MAX)
               break;

            const U16 *vert = &mesh.verts[mesh.polys[(i * mesh.nvp + j)] * bytesPerVertex];

            if(vert[0] == U16_MAX)
               break;

            if(botZoneDatabase->getObjectCount() >= MAX_ZONES)      // Don't add too many zones...
               break;

            if(j == 0)     // Add new zone because... why?
            {
               botzone = new BotNavMeshZone(botZoneDatabase->getObjectCount());

               // Triangulation only needed for display on local client... it is expensive to compute for so many zones,
               // and there is really no point if they will never be viewed.  Once disabled, triangluation cannot be re-enabled
               // for this object.
               if(!triangulateZones)
                  botzone->disableTriangulation();

               polyToZoneMap[i] = botzone->getZoneId();
            }

            botzone->addVert(Point(vert[0] - mesh.offsetX, vert[1] - mesh.offsetY));
            j++;
         }
   
         if(botzone != NULL)
         {
            botzone->addToZoneDatabase(botZoneDatabase);
            addedZones = true;
         }
      }

#ifdef LOG_TIMER
      logprintf("Recast built %d zones!", botZoneDatabase->getObjectCount());
#endif              

      if(addedZones)
         populateZoneList(botZoneDatabase, allZones);     // Repopulate allZones with the zones we modified above

      buildBotNavMeshZoneConnectionsRecastStyle(allZones, mesh, polyToZoneMap);
      linkTeleportersBotNavMeshZoneConnections(botZoneDatabase, teleporterData);
   }

   // If recast failed, build zones from the underlying triangle geometry.  This bit could be made more efficient by using the adjacnecy
   // data from Triangle, but it should only run rarely, if ever.
   else  // recastPassed == false
   {
      TNLAssert(false, "Recast failed -- please report this level to the devs, and pick continue to build zones from triangle output");
      logprintf(LogConsumer::LogLevelError, "There were problems with bot nav zone creation -- please report this level to the devs!");

      for(S32 i = 0; i < outputTriangles.size(); i+=3)
      {
         if(botZoneDatabase->getObjectCount() >= MAX_ZONES)      // Don't add too many zones...
            break;

         BotNavMeshZone *botzone = new BotNavMeshZone(i/3);
         // Triangulation only needed for display on local client... it is expensive to compute for so many zones,
         // and there is really no point if they will never be viewed.  Once disabled, triangluation cannot be re-enabled
         // for this object.
         if(!triangulateZones)
            botzone->disableTriangulation();

         botzone->addVert(outputTriangles[i]);
         botzone->addVert(outputTriangles[i+1]);
         botzone->addVert(outputTriangles[i+2]);

         botzone->addToZoneDatabase(botZoneDatabase);
      }

      buildBotNavMeshZoneConnections(allZones);
      linkTeleportersBotNavMeshZoneConnections(botZoneDatabase, teleporterData);
   }

#ifdef LOG_TIMER
   U32 done3 = Platform::getRealMilliseconds();

   logprintf("Timings: %d %d %d", done1-starttime, done2-done1, done3-done2);
#endif

   return true;
}


// Only runs on server
// TODO can be combined with buildBotNavMeshZoneConnectionsRecastStyle() ?
void BotNavMeshZone::buildBotNavMeshZoneConnections(const Vector<BotNavMeshZone *> *allZones)
{
   if(allZones->size() == 0)      // Nothing to do!
      return;

   // We'll reuse these objects throughout the following block, saving the cost of creating and destructing them
   Point bordStart, bordEnd, bordCen;
   Rect rect;
   NeighboringZone neighbor;

   // Figure out which zones are adjacent to which, and find the "gateway" between them
   for(S32 i = 0; i < zones.size() - 1; i++)
   {
      for(S32 j = i + 1; j < zones.size(); j++)
      {
         // Do zones i and j touch?  First a quick and dirty bounds check:
         if(!zones[i]->getExtent().intersectsOrBorders(zones[j]->getExtent()))
            continue;

         if(zonesTouch(allZones->get(i)->getOutline(), allZones->get(j)->getOutline(), 1.0, bordStart, bordEnd))
         {
            rect.set(bordStart, bordEnd);
            bordCen.set(rect.getCenter());

            // Zone j is a neighbor of i
            neighbor.zoneID = j;
            neighbor.borderStart.set(bordStart);
            neighbor.borderEnd.set(bordEnd);
            neighbor.borderCenter.set(bordCen);

            neighbor.distTo = allZones->get(i)->getExtent().getCenter().distanceTo(bordCen);     // Whew!
            neighbor.center.set(allZones->get(j)->getCenter());
            allZones->get(i)->mNeighbors.push_back(neighbor);

            // Zone i is a neighbor of j
            neighbor.zoneID = i;
            neighbor.borderStart.set(bordStart);
            neighbor.borderEnd.set(bordEnd);
            neighbor.borderCenter.set(bordCen);

            neighbor.distTo = allZones->get(j)->getExtent().getCenter().distanceTo(bordCen);     
            neighbor.center.set(allZones->get(i)->getCenter());
            allZones->get(j)->mNeighbors.push_back(neighbor);
         }
      }
   }
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
NeighboringZone::NeighboringZone()
{
   zoneID = 0;
   distTo = 0;
}


////////////////////////////////////////
////////////////////////////////////////


// Rough guess as to distance from fromZone to toZone
F32 AStar::heuristic(const Vector<BotNavMeshZone *> *zones, S32 fromZone, S32 toZone)
{
   return zones->get(fromZone)->getCenter().distanceTo(zones->get(toZone)->getCenter());
}


// Returns a path, including the startZone and targetZone 
Vector<Point> AStar::findPath(const Vector<BotNavMeshZone *> *zones, S32 startZone, S32 targetZone, const Point &target)
{
   // Because of these variables...
   static U16 onClosedList = 0;
   static U16 onOpenList;

   // ...these arrays can be reused without further initialization
   static U16 whichList[MAX_ZONES];       // Record whether a zone is on the open or closed list
   static S16 openList[MAX_ZONES + 1]; 
   static S16 openZone[MAX_ZONES]; 
   static S16 parentZones[MAX_ZONES]; 

   static F32 Fcost[MAX_ZONES];   
   static F32 Gcost[MAX_ZONES];    
   static F32 Hcost[MAX_ZONES];   

   S16 numberOfOpenListItems = 0;
   bool foundPath;

   S32 newOpenListItemID = 0;         // Used for creating new IDs for zones to make heap work

   Vector<Point> path;

   // This block here lets us repeatedly reuse the whichList array without resetting it or recreating it
   // which, for larger numbers of zones should be a real time saver.  It's not clear if it is particularly
   // more efficient for the zone counts we typically see in Bitfighter levels.
   if(onClosedList > U16_MAX - 3 ) // Reset whichList when we've run out of headroom
   {
      for(S32 i = 0; i < MAX_ZONES; i++) 
         whichList[i] = 0;
      onClosedList = 0;   
   }
   onClosedList = onClosedList + 2; // Changing the values of onOpenList and onClosed list is faster than redimming whichList() array
   onOpenList = onClosedList - 1;

   Gcost[startZone] = 0;         // That's the cost of going from the startZone to the startZone!
   Fcost[0] = Hcost[0] = heuristic(zones, startZone, targetZone);

   numberOfOpenListItems = 1;    // Start with one open item: the startZone

   openList[1] = 0;              // Start with 1 item in the open list (must be index 1), which is maintained as a binary heap
   openZone[0] = startZone;

   // Loop until a path is found or deemed nonexistent.
   while(true)
   {
      if(numberOfOpenListItems == 0)      // List is empty, we're done
      {
         foundPath = false; 
         break;
      }  
      else
      {
         // The open list is not empty, so take the first cell off of the list.
         //   Since the list is a binary heap, this will be the lowest F cost cell on the open list.
         S32 parentZone = openZone[openList[1]];

         if(parentZone == targetZone)
         {
            foundPath = true; 
            break;
         }

         whichList[parentZone] = onClosedList;   // add the item to the closed list
         numberOfOpenListItems--;   

         //   Open List = Binary Heap: Delete this item from the open list, which
         // is maintained as a binary heap. For more information on binary heaps, see:
         //   http://www.policyalmanac.org/games/binaryHeaps.htm
            
         //   Delete the top item in binary heap and reorder the heap, with the lowest F cost item rising to the top.
         openList[1] = openList[numberOfOpenListItems + 1];   // Move the last item in the heap up to slot #1
         S16 v = 1; 

         //   Loop until the new item in slot #1 sinks to its proper spot in the heap.
         while(true) // ***
         {
            S16 u = v;      
            if (2 * u + 1 < numberOfOpenListItems) // if both children exist
            {
               // Check if the F cost of the parent is greater than each child,
               // and select the lowest of the two children
               if(Fcost[openList[u]] >= Fcost[openList[2*u]]) 
                  v = 2 * u;
               if(Fcost[openList[v]] >= Fcost[openList[2*u+1]]) 
                  v = 2 * u + 1;      
            }
            else if (2 * u < numberOfOpenListItems) // if only child (#1) exists
            {
                // Check if the F cost of the parent is greater than child #1   
               if(Fcost[openList[u]] >= Fcost[openList[2*u]]) 
                  v = 2 * u;
            }

            if(u != v) // If parent's F is > one of its children, swap them...
            {
               S16 temp = openList[u];
               openList[u] = openList[v];
               openList[v] = temp;         
            }
            else
               break; // ...otherwise, exit loop
         } // ***

         // Check the adjacent zones. (Its "children" -- these path children
         //   are similar, conceptually, to the binary heap children mentioned
         //   above, but don't confuse them. They are different.
         // Add these adjacent child squares to the open list
         //   for later consideration if appropriate.

         Vector<NeighboringZone> neighboringZones = zones->get(parentZone)->mNeighbors;

         for(S32 a = 0; a < neighboringZones.size(); a++)
         {
            NeighboringZone zone = neighboringZones[a];
            S32 zoneID = zone.zoneID;

            //   Check if zone is already on the closed list (items on the closed list have
            //   already been considered and can now be ignored).
            if(whichList[zoneID] == onClosedList) 
               continue;

            //   Add zone to the open list if it's not already on it
            TNLAssert(newOpenListItemID < MAX_ZONES, "Too many nav zones... try increasing MAX_ZONES!");
            if(whichList[zoneID] != onOpenList && newOpenListItemID < MAX_ZONES) 
            {   
               // Create a new open list item in the binary heap
               newOpenListItemID = newOpenListItemID + 1;   // Give each new item a unique id
               S32 m = numberOfOpenListItems + 1;
               openList[m] = newOpenListItemID;             // Place the new open list item (actually, its ID#) at the bottom of the heap
               openZone[newOpenListItemID] = zoneID;        // Record zone as a newly opened

               Hcost[openList[m]] = heuristic(zones, zoneID, targetZone);
               Gcost[zoneID] = Gcost[parentZone] + zone.distTo;
               Fcost[openList[m]] = Gcost[zoneID] + Hcost[openList[m]];
               parentZones[zoneID] = parentZone; 

               // Move the new open list item to the proper place in the binary heap.
               // Starting at the bottom, successively compare to parent items,
               // swapping as needed until the item finds its place in the heap
               // or bubbles all the way to the top (if it has the lowest F cost).
               while(m > 1 && Fcost[openList[m]] <= Fcost[openList[m/2]]) 
               {
                  S16 temp = openList[m/2];
                  openList[m/2] = openList[m];
                  openList[m] = temp;
                  m = m/2;
               }

               // Finally, put zone on the open list
               whichList[zoneID] = onOpenList;
               numberOfOpenListItems++;
            }

            // If zone is already on the open list, check to see if this 
            //   path to that cell from the starting location is a better one. 
            //   If so, change the parent of the cell and its G and F costs.

            else // zone was already on the open list
            {
               // Figure out the G cost of this possible new path
               S32 tempGcost = (S32)(Gcost[parentZone] + zone.distTo);
               
               // If this path is shorter (G cost is lower) then change
               // the parent cell, G cost and F cost.
               if(tempGcost < Gcost[zoneID])
               {
                  parentZones[zoneID] = parentZone; // Change the square's parent
                  Gcost[zoneID] = (F32)tempGcost;        // and its G cost         

                  // Because changing the G cost also changes the F cost, if
                  // the item is on the open list we need to change the item's
                  // recorded F cost and its position on the open list to make
                  // sure that we maintain a properly ordered open list.

                  for(S32 i = 1; i <= numberOfOpenListItems; i++) // Look for the item in the heap
                  {
                     if(openZone[openList[i]] == zoneID) 
                     {
                        Fcost[openList[i]] = Gcost[zoneID] + Hcost[openList[i]]; // Change the F cost
                        
                        // See if changing the F score bubbles the item up from it's current location in the heap
                        S32 m = i;
                        while(m > 1 && Fcost[openList[m]] < Fcost[openList[m/2]]) 
                        {
                           S16 temp = openList[m/2];
                           openList[m/2] = openList[m];
                           openList[m] = temp;
                           m = m/2;
                        }
                   
                        break; 
                     } 
                  }
               }

            } // else if whichList(zoneID) = onOpenList   
         } // for loop looping through neighboring zones list
      }  

      // If target is added to open list then path has been found.
      if(whichList[targetZone] == onOpenList)
      {
         foundPath = true; 
         break;
      }
   }

   // Save the path if it exists
   if(!foundPath)
   {      
      TNLAssert(path.size() == 0, "Expected empty path!");
      return path;
   }

   // Working backwards from the target to the starting location by checking
   // each cell's parent, figure out the length of the path.
   //
   // Fortunately, we want our list to have the closest zone last (see getWaypoint),
   // so it all works out nicely.
   //
   // We'll store both the zone center and the gateway to the neighboring zone.  This
   // will help keep the robot from getting hung up on blocked but technically visible
   // paths, such as when we are trying to fly around a protruding wall stub.

   path.push_back(target);                               // First point is the actual target itself
   path.push_back(zones->get(targetZone)->getCenter());  // Second is the center of the target's zone
      
   S32 zone = targetZone;

   while(zone != startZone)
   {
      path.push_back(findGateway(zones, parentZones[zone], zone));   // Don't switch findGateway arguments, some path is one way (teleporters).
      zone = parentZones[zone];                                      // Find the parent of the current cell
      path.push_back(zones->get(zone)->getCenter());
   }

   path.push_back(zones->get(startZone)->getCenter());
   return path;
}


// Return a point representing gateway between zones
Point AStar::findGateway(const Vector<BotNavMeshZone *> *zones, S32 zone1, S32 zone2)
{
   S32 neighborIndex = zones->get(zone1)->getNeighborIndex(zone2);
   TNLAssert(neighborIndex >= 0, "Invalid neighbor index!!");

   return zones->get(zone1)->mNeighbors[neighborIndex].borderCenter;
}


};


