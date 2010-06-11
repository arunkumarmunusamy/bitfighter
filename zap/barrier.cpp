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

#include "barrier.h"
#include "gameObjectRender.h"
#include "SweptEllipsoid.h"      // For polygon triangulation

#include "glutInclude.h"
#include <math.h>

using namespace TNL;

namespace Zap
{

TNL_IMPLEMENT_NETOBJECT(Barrier);

U32 Barrier::mBarrierChangeIndex = 1;


// Given the points in vec, figure out where the ends of the walls should be (they'll need to be extended slighly in some cases
// for better rendering).  Set extendAmt to 0 to see why it's needed.
// Populates barrierEnds with the results.
void constructBarrierEndPoints(const Vector<Point> &vec, F32 width, Vector<Point> &barrierEnds)
{
   barrierEnds.clear();    // local static vector

   if(vec.size() == 1)     // Protect against bad data
      return;

   bool loop = (vec.first() == vec.last());      // Does our barrier form a closed loop?

   Vector<Point> edgeVector;
   for(S32 i = 0; i < vec.size() - 1; i++)
   {
      Point e = vec[i+1] - vec[i];
      e.normalize();
      edgeVector.push_back(e);
   }

   Point lastEdge = edgeVector[edgeVector.size() - 1];
   Vector<F32> extend;

   for(S32 i = 0; i < edgeVector.size(); i++)
   {
      Point curEdge = edgeVector[i];
      double cosTheta = curEdge.dot(lastEdge);

      // Do some bounds checking.  Crazy, I know, but trust me, it's worth it!
      if (cosTheta > 1.0)
         cosTheta = 1.0;
      else if(cosTheta < -1.0)  
         cosTheta = -1.0;

      cosTheta = abs(cosTheta);     // Seems to reduce "end gap" on acute junction angles
      
      F32 extendAmt = width * 0.5 * tan( acos(cosTheta) / 2 );
      if(extendAmt > 0.01)
         extendAmt -= 0.01;
      extend.push_back(extendAmt);
   
      lastEdge = curEdge;
   }

   F32 first = extend[0];
   extend.push_back(first);

   for(S32 i = 0; i < edgeVector.size(); i++)
   {
      F32 extendBack = extend[i];
      F32 extendForward = extend[i+1];
      if(i == 0 && !loop)
         extendBack = 0;
      if(i == edgeVector.size() - 1 && !loop)
         extendForward = 0;

      Point start = vec[i] - edgeVector[i] * extendBack;
      Point end = vec[i+1] + edgeVector[i] * extendForward;
      barrierEnds.push_back(start);
      barrierEnds.push_back(end);
   }
}


void constructBarriers(Game *theGame, const Vector<F32> &barrier, F32 width, bool solid)
{
   Vector<Point> tmp;
   Vector<Point> vec;

   // Convert the list of floats into a list of points
   for(S32 i = 1; i < barrier.size(); i += 2)
      tmp.push_back( Point(barrier[i-1], barrier[i]) );

   // Remove collinear points to make rendering nicer and datasets smaller
   for(S32 i = 0; i < tmp.size(); i++)
   {
      S32 j = i;
      while(i > 0 && i < tmp.size() - 1 && (tmp[j] - tmp[j-1]).ATAN2() == (tmp[i+1] - tmp[i]).ATAN2())
         i++;

      vec.push_back(tmp[i]);
   }

   if(vec.size() <= 1)
      return;

   if(solid)   // This is a solid polygon
   {
      if(vec.first() == vec.last())      // Does our barrier form a closed loop?
         vec.erase(vec.size() - 1);          // If so, remove last vertex

      Barrier *b = new Barrier(vec, width, true);
      b->addToGame(theGame);
   }
   else        // This is a standard series of segments
   {
      // First, fill a vector with barrier segments
      Vector<Point> barrierEnds;
      constructBarrierEndPoints(vec, width, barrierEnds);

      Vector<Point> pts;
      // Then add individual segments to the game
      for(S32 i = 0; i < barrierEnds.size(); i += 2)
      {
         pts.clear();
         pts.push_back(barrierEnds[i]);
         pts.push_back(barrierEnds[i+1]);

         Barrier *b = new Barrier(pts, width, false);    // false = not solid
         b->addToGame(theGame);
      }
   }
}


// Simply takes a segment and "puffs it out" to a rectangle of a specified width.  Does not modify endpoints.
void expandCenterlineToOutline(const Point &start, const Point &end, F32 width, Vector<Point> &cornerPoints)
{
   Point dir = end - start;
   Point crossVec(dir.y, -dir.x);
   crossVec.normalize(width * 0.5);

   cornerPoints.push_back(Point(start.x + crossVec.x, start.y + crossVec.y));
   cornerPoints.push_back(Point(end.x + crossVec.x, end.y + crossVec.y));
   cornerPoints.push_back(Point(end.x - crossVec.x, end.y - crossVec.y));
   cornerPoints.push_back(Point(start.x - crossVec.x, start.y - crossVec.y));
}


// Takes a list of vertices and converts them into a list of lines representing the edges of an object
void populateEdgeLines(const Vector<Point> &mRenderOutlineGeometry, Vector<Point> &mRenderLineSegments)
{
   mRenderLineSegments.clear();

   S32 last = mRenderOutlineGeometry.size() - 1;      // Remember: mRenderOutlineGeometry is a vector of points representing lines
   for(S32 i = 0; i < mRenderOutlineGeometry.size(); i++)
   {
      mRenderLineSegments.push_back(mRenderOutlineGeometry[last]);
      mRenderLineSegments.push_back(mRenderOutlineGeometry[i]);
      last = i;
   }
}



// Clears out overlapping barrier lines for better rendering appearance, modifies lineSegmentPoints.
// This is effectively called on every pair of potentially intersecting barriers, and lineSegmentPoints gets 
// refined as each additional intersecting barrier gets processed.
void clipRenderLinesToPoly(const Vector<Point> &polyPoints, Vector<Point> &lineSegmentPoints)
{
   Vector<Point> clippedSegments;

   // Loop through all the segments
   for(S32 i = 0; i < lineSegmentPoints.size(); i+= 2)
   {
      Point rp1 = lineSegmentPoints[i];
      Point rp2 = lineSegmentPoints[i + 1];

      Point cp1 = polyPoints[polyPoints.size() - 1];
      for(S32 j = 0; j < polyPoints.size(); j++)
      {
         Point cp2 = polyPoints[j];
         Point ce = cp2 - cp1;
         Point n(-ce.y, ce.x);

         n.normalize();
         F32 distToZero = n.dot(cp1);

         F32 d1 = n.dot(rp1);
         F32 d2 = n.dot(rp2);

         // Setting the following comparisons to >= will cause collinear end segments to go away, but will
         // cause overlapping walls to disappear
         bool d1in = (d1 > distToZero);
         bool d2in = (d2 > distToZero);

         if(!d1in && !d2in) // Both points are outside this edge of the poly...
         {
            // ...so add them to the render poly
            clippedSegments.push_back(rp1);
            clippedSegments.push_back(rp2);
            break;
         }
         else if((d1in && !d2in) || (d2in && !d1in))
         {
            // Find the clip intersection point:
            F32 t = (distToZero - d1) / (d2 - d1);
            Point clipPoint = rp1 + (rp2 - rp1) * t;

            if(d1in)
            {
               clippedSegments.push_back(clipPoint);
               clippedSegments.push_back(rp2);
               rp2 = clipPoint;
            }
            else
            {
               clippedSegments.push_back(rp1);
               clippedSegments.push_back(clipPoint);
               rp1 = clipPoint;
            }
         }

         // If both are in, go to the next edge
         cp1 = cp2;
      }
   }

   lineSegmentPoints = clippedSegments;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor --> gets called from constructBarriers above
Barrier::Barrier(const Vector<Point> &points, F32 width, bool solid)
{
   mObjectTypeMask = BarrierType | CommandMapVisType;
   mPoints = points;

   if(points.size() < 2)      // Invalid barrier!
   {
      delete this;
      return;
   }

   Rect r(points[0], points[1]);

   for(S32 i = 2; i < points.size(); i++)
      r.expand(points[i]);
   mWidth = width;

   if(points.size() == 2)    // It's a regular segment, so apply width
      r.expand(Point(width, width));

   setExtent(r);
   mLastBarrierChangeIndex = 0;

    mSolid = solid;

   if(mSolid)
       Triangulate::Process(mPoints, mRenderFillGeometry);
   else
       getCollisionPoly(mRenderFillGeometry);   // Fill mRenderFillGeometry

   getCollisionPoly(mRenderOutlineGeometry);    // Outline is the same for both barrier geometries
}


void Barrier::onAddedToGame(Game *theGame)
{
  getGame()->mObjectsLoaded++;
}


// Processes mPoints and fills polyPoints 
bool Barrier::getCollisionPoly(Vector<Point> &polyPoints)
{
   if(mPoints.size() == 2)    // It's a regular segment, so apply width
      expandCenterlineToOutline(mPoints[0], mPoints[1], mWidth, polyPoints);     // Fills polyPoints with 4 points
   else                       // Otherwise, our collisionPoly is just our points!
      polyPoints = mPoints;

   return true;
}


extern Color gWallOutlineColor;
extern Color gWallFillColor;

void Barrier::render(S32 layerIndex)
{
   if(layerIndex == 0)           // First, draw the fill
   {
      glColor(gWallFillColor);
      if(mSolid)                 // Rendering is a bit different for solid polys
      {
         for(S32 i = 0; i < mRenderFillGeometry.size(); i+=3)
         {
            glBegin(GL_POLYGON);
               for(S32 j = i; j < i+3; j++)
                  glVertex(mRenderFillGeometry[j]);
            glEnd();
         }
      }
      else                       // Normal wall
      {
         glBegin(GL_POLYGON);
            for(S32 i = 0; i < mRenderFillGeometry.size(); i++)
               glVertex(mRenderFillGeometry[i]);
         glEnd();
      }

   }
   else if(layerIndex == 1)      // Second, draw the outlines
   {
      // This needs to be run on all walls every time a new segment is added, but it need not run every time a wall is drawn
      if(mLastBarrierChangeIndex != mBarrierChangeIndex)    
      {
         mLastBarrierChangeIndex = mBarrierChangeIndex;

         populateEdgeLines(mRenderOutlineGeometry, mRenderLineSegments);

         static Vector<DatabaseObject *> fillObjects;
         fillObjects.clear();

         findObjects(BarrierType, fillObjects, getExtent());      // Find all potentially colliding wall segments (fillObjects)

         for(S32 i = 0; i < fillObjects.size(); i++)
         {
            mRenderOutlineGeometry.clear();
            if(fillObjects[i] != this && 
                     dynamic_cast<GameObject *>(fillObjects[i])->getCollisionPoly(mRenderOutlineGeometry))
               clipRenderLinesToPoly(mRenderOutlineGeometry, mRenderLineSegments);
         }
      }

      // Actual outline rendering code here:
      glColor(gWallOutlineColor);
      glBegin(GL_LINES);
         for(S32 i = 0; i < mRenderLineSegments.size(); i++)
            glVertex(mRenderLineSegments[i]);
      glEnd();
   }
}

};
