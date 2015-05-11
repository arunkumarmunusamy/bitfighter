//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "GeomObject.h"
#include "Geometry.h"

using namespace TNL;

namespace Zap
{


// Constructor
GeomObject::GeomObject()
{
   // Do nothing
}


// Destructor
GeomObject::~GeomObject()
{
   // Do nothing
};


// mGeometry will be deleted in destructor; radius default to 0
void GeomObject::setNewGeometry(GeomType geomType, F32 radius)
{
   TNLAssert(!mGeometry.getGeometry(), "This object already has a geometry!");

   switch(geomType)
   {
      case geomPoint:
         mGeometry.setGeometry(new PointGeometry(radius));
         return;

      case geomSimpleLine:
         mGeometry.setGeometry(new SimpleLineGeometry());
         return;

      case geomPolyLine:
         mGeometry.setGeometry(new PolylineGeometry());
         return;

      case geomPolygon:
         mGeometry.setGeometry(new PolygonGeometry());
         return;

      default:
         TNLAssert(false, "Unknown geometry!");
         break;
   }
}


// Basic definitions
GeomType      GeomObject::getGeomType() const        {   return mGeometry.getGeometry()->getGeomType();   }
Point         GeomObject::getVert(S32 index) const   {   return mGeometry.getVert(index);                 }

bool GeomObject::deleteVert(S32 vertIndex)
{   
   if(mGeometry.getGeometry()->deleteVert(vertIndex))
   {
      onGeomChanged();
      return true;
   }

   return false;
}


bool GeomObject::insertVert(Point vertex, S32 vertIndex) 
{   
   if(mGeometry.getGeometry()->insertVert(vertex, vertIndex))
   {
      onGeomChanged();
      return true;
   }

   return false;
}


void GeomObject::setVert(const Point &pos, S32 index)    { mGeometry.getGeometry()->setVert(pos, index); }
                                                                                           
bool GeomObject::anyVertsSelected() const    {   return mGeometry.getGeometry()->anyVertsSelected();        }
S32  GeomObject::getVertCount()     const    {   return mGeometry.getGeometry()->getVertCount();            }
S32  GeomObject::getMinVertCount()  const    {   return mGeometry.getGeometry()->getMinVertCount();         }

void GeomObject::clearVerts()                {   mGeometry.getGeometry()->clearVerts(); onGeomChanged();  }


bool GeomObject::addVertFront(Point vert)
{
   if(mGeometry.getGeometry()->addVertFront(vert))
   {
      onGeomChanged();
      return true;
   }

   return false;
}


bool GeomObject::addVert(const Point &point, bool ignoreMaxPointsLimit) 
{
   if(mGeometry.getGeometry()->addVert(point, ignoreMaxPointsLimit))
   {
      onGeomChanged();
      return true;
   }

   return false;
}


// Vertex selection -- only needed in editor
void GeomObject::selectVert(S32 vertIndex)         {   mGeometry.getGeometry()->selectVert(vertIndex);            }
void GeomObject::aselectVert(S32 vertIndex)        {   mGeometry.getGeometry()->aselectVert(vertIndex);           }
void GeomObject::unselectVert(S32 vertIndex)       {   mGeometry.getGeometry()->unselectVert(vertIndex);          }
void GeomObject::unselectVerts()                   {   mGeometry.getGeometry()->unselectVerts();                  }
     
bool GeomObject::vertSelected(S32 vertIndex) const {   return mGeometry.getGeometry()->vertSelected(vertIndex);   }

// Geometric calculations
Point GeomObject::getCentroid()   const {   return mGeometry.getGeometry()->getCentroid();     }
F32   GeomObject::getLabelAngle() const {   return mGeometry.getGeometry()->getLabelAngle();   }
      

// Geometry operations
const Vector<Point> *GeomObject::getOutline() const       {   return mGeometry.getOutline();    }
const Vector<Point> *GeomObject::getFill() const          {   return mGeometry.getFill();       }

void GeomObject::reverseWinding() { mGeometry.reverseWinding(); }


// Geometric manipulations
void GeomObject::rotateAboutPoint(const Point &center, F32 angle)  {  mGeometry.getGeometry()->rotateAboutPoint(center, angle);   }
void GeomObject::flip(F32 center, bool isHoriz)                    {  mGeometry.getGeometry()->flip(center, isHoriz);             }
void GeomObject::scale(const Point &center, F32 scale)             {  mGeometry.getGeometry()->scale(center, scale);              }

// Move object to location, specifying (optional) vertex to be positioned at pos
void GeomObject::moveTo(const Point &pos, S32 snapVertex)          {  mGeometry.getGeometry()->moveTo(pos, snapVertex);           }
void GeomObject::offset(const Point &offset)                       {  mGeometry.getGeometry()->offset(offset);                    }

// Geom in-out
void GeomObject::packGeom(GhostConnection *connection, BitStream *stream)    {   mGeometry.getGeometry()->packGeom(connection, stream);     }
void GeomObject::unpackGeom(GhostConnection *connection, BitStream *stream)  {   mGeometry.getGeometry()->unpackGeom(connection, stream); onGeomChanged();  }
void GeomObject::setGeom(const Vector<Point> &points)                        {   mGeometry.getGeometry()->setGeom(points); }

void GeomObject::readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize) 
{  
   mGeometry.getGeometry()->readGeom(argc, argv, firstCoord, gridSize); 
   onGeomChanged();
}


GeometryContainer &GeomObject::getGeometry()
{
   return mGeometry;
}


void GeomObject::setGeometry(const Vector<Point> &points)
{
   mGeometry.setGeometry(points);
}


// Function currently only used for testing
bool GeomObject::hasGeometry() const
{
   return mGeometry.getGeometry() != NULL;
}


string GeomObject::geomToLevelCode() const {  return mGeometry.geomToLevelCode();     }
Rect GeomObject::calcExtents() const       { return mGeometry.getGeometry()->calcExtents(); }


// Settings
void GeomObject::disableTriangulation() {   mGeometry.getGeometry()->disableTriangulation();   }


Point GeomObject::getPos()       const { return getVert(0); }
Point GeomObject::getRenderPos() const { return getPos();   }


void GeomObject::setPos(const Point &pos)
{
   setVert(pos, 0);  
   setExtent(calcExtents());
}


void GeomObject::onGeomChanging()
{
   onGeomChanged();
}


void GeomObject::onGeomChanged()
{
   // This will update any other internal data our geometry may have,
   // like a centroid or triangulated polygon fill
   mGeometry.getGeometry()->onPointsChanged();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
GeometryContainer::GeometryContainer()
{
   mGeometry = NULL;
}


// Copy constructor
GeometryContainer::GeometryContainer(const GeometryContainer &container)
{
   const Geometry *old = container.mGeometry;

   TNLAssert(container.mGeometry, "Expected object to have geometry!");

   switch(container.mGeometry->getGeomType())
   {
      case geomPoint:
         mGeometry = new PointGeometry(*static_cast<const PointGeometry *>(old));
         break;
      case geomSimpleLine:
         mGeometry = new SimpleLineGeometry(*static_cast<const SimpleLineGeometry *>(old));
         break;
      case geomPolyLine:
         mGeometry = new PolylineGeometry(*static_cast<const PolylineGeometry *>(old));
         break;
      case geomPolygon:
         mGeometry = new PolygonGeometry(*static_cast<const PolygonGeometry *>(old));
         break;
      default:
         TNLAssert(false, "Invalid value!");
         break;
   }
}


// Destructor
GeometryContainer::~GeometryContainer()
{
   delete mGeometry;
}


Geometry *GeometryContainer::getGeometry() const
{
   return mGeometry;
}


void GeometryContainer::setGeometry(Geometry *geometry)
{
   delete mGeometry;
   mGeometry = geometry;
}


void GeometryContainer::reverseWinding()    
{
   mGeometry->reverseWinding();
}


void GeometryContainer::setGeometry(const Vector<Point> &points)
{
   mGeometry->setGeom(points);
}


const Vector<Point> *GeometryContainer::getOutline() const
{
   return mGeometry->getOutline();
}


const Vector<Point> *GeometryContainer::getFill() const    
{
   return mGeometry->getFill();
}


Point GeometryContainer::getVert(S32 index) const   
{   
   return mGeometry->getVert(index);  
}


string GeometryContainer::geomToLevelCode() const
{  
   return mGeometry->geomToLevelCode();
}


};
