//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "gridDB.h"
#include "moveObject.h"    // For def of ActualState
#include "Level.h"

#include "GeomUtils.h"

#include "tnlLog.h"

namespace Zap
{

U32 GridDatabase::mQueryId = 0;
ClassChunker<DatabaseBucketEntry> *GridDatabase::mChunker = NULL;
U32 GridDatabase::mCountGridDatabase = 0;

static U32 getNextId() 
{
   static U32 nextId = 0;
   return nextId++;
}

// Constructor
GridDatabase::GridDatabase()
{
   if(mChunker == NULL)
      mChunker = new ClassChunker<DatabaseBucketEntry>();        // Static shared by all databases, reference counted and deleted in destructor

   mCountGridDatabase++;

   for(U32 i = 0; i < BucketRowCount; i++)
      for(U32 j = 0; j < BucketRowCount; j++)
         mBuckets[i][j].nextInBucket = NULL;

   mDatabaseId = getNextId();
}


// Destructor
GridDatabase::~GridDatabase()       
{
   removeEverythingFromDatabase();

   TNLAssert(mChunker != NULL || mCountGridDatabase != 0, "Running GridDatabase destructor without initalizing?");

   mCountGridDatabase--;

   if(mCountGridDatabase == 0)
   {
      delete mChunker;
      mChunker = NULL;
   }
}


// This sort will put points on top of lines on top of polygons...  as they should be
// We'll also put walls on the bottom, as this seems to work best in practice
S32 QSORT_CALLBACK geometricSort(DatabaseObject * &a, DatabaseObject * &b)
{
   if(isWallType(a->getObjectTypeNumber()))
      return 1;
   if(isWallType(b->getObjectTypeNumber()))
      return -1;

   return( b->getGeomType() - a->getGeomType() );
}


static void sortObjects(Vector<DatabaseObject *> &objects)
{
   if(objects.size() >= 2)       // No point sorting unless there are two or more objects!

      // Cannot use Vector.sort() here because I couldn't figure out how to cast shared_ptr as pointer (*)
      //sort(objects.getStlVector().begin(), objects.getStlVector().begin() + objects.size(), geometricSort);
      qsort(&objects[0], objects.size(), sizeof(BfObject *), (qsort_compare_func) geometricSort);
}


// Fill this database with objects from existing database
void GridDatabase::copyObjects(const GridDatabase *source)
{
   // Preallocate some memory to make copying a little more efficient
   mAllObjects.reserve(source->mAllObjects.size());
   mGoalZones .reserve(source->mGoalZones.size());
   mFlags     .reserve(source->mFlags.size());
   mSpyBugs   .reserve(source->mSpyBugs.size());
   mPolyWalls .reserve(source->mPolyWalls.size());
   mWallitems .reserve(source->mWallitems.size());

   for(S32 i = 0; i < source->mAllObjects.size(); i++)
      addToDatabase(source->mAllObjects[i]->clone());

   sortObjects(mAllObjects);
}


// Adds an object to the database; checks to ensure object is not added twice.
// Private function
void GridDatabase::addToDatabase(DatabaseObject *object)
{
   TNLAssert(object->mDatabase != this, "Already added to database, trying to add to same database again!");
   TNLAssert(!object->mDatabase,        "Already added to database, trying to add to different database!");
   TNLAssert(object->getExtentSet(),    "Object extents were never set!");
   TNLAssert(!object->mBucketList,      "BucketList must be NULL");

   // WallItems should not be added to the database during a regular game, but the editor will add them...
   //TNLAssert(object->getObjectTypeNumber() != WallItemTypeNumber, "Should not add wall items to the database!");

   if(object->mDatabase)      // Should never happen
      return;

   object->mDatabase = this;

   static IntRect bins;
   fillBins(object->getExtent(), bins);

   for(S32 x = bins.minx; bins.maxx - x >= 0; x++)
      for(S32 y = bins.miny; bins.maxy - y >= 0; y++)
      {
         DatabaseBucketEntry *be = mChunker->alloc();
         DatabaseBucketEntryBase *base = &mBuckets[x & BucketMask][y & BucketMask];
         be->theObject = object;
         if(base->nextInBucket)
            base->nextInBucket->prevInBucket = be;
         be->nextInBucket = base->nextInBucket;
         be->prevInBucket = base;
         base->nextInBucket = be;
         be->nextInBucketForThisObject = object->mBucketList;
         object->mBucketList = be;
      }

   // Add the object to our non-spatial "database" as well
   mAllObjects.push_back(object);

   U8 type = object->getObjectTypeNumber();

   if(type == GoalZoneTypeNumber)
      mGoalZones.push_back(object);
   else if(type == FlagTypeNumber)
      mFlags.push_back(object);
   else if(type == SpyBugTypeNumber)
      mSpyBugs.push_back(object);
   else if(type == PolyWallTypeNumber)
      mPolyWalls.push_back(object);
   else if(type == WallItemTypeNumber)
      mWallitems.push_back(object);

   //sortObjects(mAllObjects);  // problem: Barriers in-game don't have mGeometry (it is NULL)
}


// Bulk add items to database
void GridDatabase::addToDatabase(const Vector<DatabaseObject *> &objects)
{
   for(S32 i = 0; i < objects.size(); i++)
      addToDatabase(objects[i]);
}


// Bulk add items to database -- different sig (could probably be handled with a template!)
void GridDatabase::addToDatabase(const Vector<BfObject *> &objects)
{
   for(S32 i = 0; i < objects.size(); i++)
      addToDatabase(objects[i]);
}



// Removes and deletes all objects in database
void GridDatabase::removeEverythingFromDatabase()
{
   for(S32 x = 0; x < BucketRowCount; x++)
   {
      for(S32 y = 0; y < BucketRowCount; y++)
      {
         for(DatabaseBucketEntry *walk = mBuckets[x & BucketMask][y & BucketMask].nextInBucket; walk; )
         {
            DatabaseBucketEntry *rem = walk;
            walk->theObject->mDatabase = NULL;  // make sure object don't point to this database anymore
            walk->theObject->mBucketList = NULL;
            walk = rem->nextInBucket;
            mChunker->free(rem);
         }
         mBuckets[x & BucketMask][y & BucketMask].nextInBucket = NULL;
      }
   }

   // Clear out our specialty lists -- since objects are also in mAllObjects, they'll be deleted below
   mGoalZones.clear();
   mFlags.clear();
   mSpyBugs.clear();
   mPolyWalls.clear();
   mWallitems.clear();

   for(S32 i = 0; i < mAllObjects.size(); i++)
      mAllObjects[i]->deleteThyself();

   mAllObjects.clear();
}


// Don't use this with a sorted list!
static void eraseObject_fast(Vector<DatabaseObject *> *objects, DatabaseObject *objectToDelete)
{
   for(S32 i = 0; i < objects->size(); i++)
      if(objects->get(i) == objectToDelete)
      {
         objects->erase_fast(i);     
         return;
      }
}


// Delete by index
void GridDatabase::removeFromDatabase(S32 index, bool deleteObject)
{
   DatabaseObject *obj = mAllObjects[index];

   removeFromDatabase(obj, deleteObject);
}


// Delete by object
void GridDatabase::removeFromDatabase(DatabaseObject *object, bool deleteObject)
{
   TNLAssert(object->mDatabase == this || object->mDatabase == NULL, "Trying to remove Object from wrong database");
   if(object->mDatabase != this)
      return;

   const Rect &extents = object->mExtent;
   object->mDatabase = NULL;

   static IntRect bins;
   fillBins(extents, bins);

   while(object->mBucketList)
   {
      DatabaseBucketEntry *b = object->mBucketList;
      TNLAssert(b->theObject == object, "Object mismatch");
      TNLAssert(b->prevInBucket->nextInBucket == b, "Broken linked list");
      if(b->nextInBucket)
         b->nextInBucket->prevInBucket = b->prevInBucket;
      b->prevInBucket->nextInBucket = b->nextInBucket;
      object->mBucketList = b->nextInBucketForThisObject;
      mChunker->free(b);
   }

   // Find and delete object from our non-spatial databases
   for(S32 i = 0; i < mAllObjects.size(); i++)
      if(mAllObjects[i] == object)
      {
         mAllObjects.erase(i);            // mAllObjects is sorted, so we can't use erase_fast
         break;
      }


   U8 type = object->getObjectTypeNumber();

   if(type == GoalZoneTypeNumber)
      eraseObject_fast(&mGoalZones, object);
   else if(type == FlagTypeNumber)
      eraseObject_fast(&mFlags, object);
   else if(type == SpyBugTypeNumber)
      eraseObject_fast(&mSpyBugs, object);
   else if(type == PolyWallTypeNumber)
      eraseObject_fast(&mPolyWalls, object);
   else if(type == WallItemTypeNumber)
      eraseObject_fast(&mWallitems, object);

   if(deleteObject)
      object->deleteThyself();
}


void GridDatabase::findObjects(Vector<DatabaseObject *> &fillVector) const
{
   fillVector.resize(mAllObjects.size());

   for(S32 i = 0; i < mAllObjects.size(); i++)
      fillVector[i] = mAllObjects[i];
}


// Faster than above, but results can't be modified
const Vector<DatabaseObject *> *GridDatabase::findObjects_fast() const
{
   return &mAllObjects;
}


// Faster than above, but results can't be modified, and only works with selected types at the moment
const Vector<DatabaseObject *> *GridDatabase::findObjects_fast(U8 typeNumber) const
{
   if(typeNumber == GoalZoneTypeNumber)
      return &mGoalZones;

   if(typeNumber == FlagTypeNumber)
      return &mFlags;

   if(typeNumber == SpyBugTypeNumber)
      return &mSpyBugs;

   if(typeNumber == PolyWallTypeNumber)
      return &mPolyWalls;

   if(typeNumber == WallItemTypeNumber)
      return &mWallitems;

   TNLAssert(false, "This type not currently supported!  Sorry dude!");
   return NULL;
}


void GridDatabase::findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector, const Rect *extents, const IntRect *bins) const
{
   static Vector<U8> types;
   types.resize(1);

   types[0] = typeNumber;

   findObjects(types, fillVector, extents, bins);
}


void GridDatabase::findObjects(Vector<U8> typeNumbers, Vector<DatabaseObject *> &fillVector, const Rect *extents, const IntRect *bins) const
{
   mQueryId++;    // Used to prevent the same item from being found in multiple buckets

   for(S32 x = bins->minx; bins->maxx - x >= 0; x++)
      for(S32 y = bins->miny; bins->maxy - y >= 0; y++)
         for(DatabaseBucketEntry *walk = mBuckets[x & BucketMask][y & BucketMask].nextInBucket; walk; walk = walk->nextInBucket)
         {
            DatabaseObject *theObject = walk->theObject;

            if(theObject->mLastQueryId != mQueryId &&                         // Object hasn't been queried; and
               testTypes(typeNumbers, theObject->getObjectTypeNumber()) &&    // is of the right type; and
               (!extents || theObject->mExtent.intersects(*extents)) )        // overlaps our extents (if passed)
            {
               walk->theObject->mLastQueryId = mQueryId;    // Flag the object so we know we've already visited it
               fillVector.push_back(walk->theObject);       // And save it as a found item
            }
         }
}


// Find all objects in database of type typeNumber
void GridDatabase::findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector) const
{
   // If the user is looking for a type we maintain a list for, it will be faster to use that list than to cycle through the general item list.
   TNLAssert(typeNumber != GoalZoneTypeNumber && typeNumber != FlagTypeNumber && 
             typeNumber != SpyBugTypeNumber   && typeNumber != PolyWallTypeNumber, 
             "Can use findObjects_fast()?  If not, uncomment the appropriate block below; it will perform better!");

   for(S32 i = 0; i < mAllObjects.size(); i++)
      if(mAllObjects[i]->getObjectTypeNumber() == typeNumber)
         fillVector.push_back(mAllObjects[i]);
}


// Translates extents into bins to search
void GridDatabase::fillBins(const Rect &extents, IntRect &bins) const
{
   bins.minx = S32(extents.min.x) >> BucketWidthBitShift;
   bins.miny = S32(extents.min.y) >> BucketWidthBitShift;
   bins.maxx = S32(extents.max.x) >> BucketWidthBitShift;
   bins.maxy = S32(extents.max.y) >> BucketWidthBitShift;

   if(U32(bins.maxx - bins.minx) >= BucketRowCount)
      bins.maxx = bins.minx + BucketRowCount - 1;

   if(U32(bins.maxy - bins.miny) >= BucketRowCount)
      bins.maxy = bins.miny + BucketRowCount - 1;
}


// Find all objects in &extents that are of type typeNumber
void GridDatabase::findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector, const Rect &extents) const
{
   static IntRect bins;
   fillBins(extents, bins);

   findObjects(typeNumber, fillVector, &extents, &bins);
}


void GridDatabase::findObjects(TestFunc testFunc, Vector<DatabaseObject *> &fillVector, const Rect *extents, const IntRect *bins, bool sameQuery) const
{
   TNLAssert(this, "findObjects 'this' is NULL");
   if(!sameQuery)
      mQueryId++;    // Used to prevent the same item from being found in multiple buckets

   for(S32 x = bins->minx; bins->maxx - x >= 0; x++)
      for(S32 y = bins->miny; bins->maxy - y >= 0; y++)
         for(DatabaseBucketEntry *walk = mBuckets[x & BucketMask][y & BucketMask].nextInBucket; walk; walk = walk->nextInBucket)
         {
            DatabaseObject *theObject = walk->theObject;

            if(theObject->mLastQueryId != mQueryId &&                      // Object hasn't been queried; and
               (testFunc(theObject->getObjectTypeNumber())) &&             // is of the right type; and
               (!extents || theObject->mExtent.intersects(*extents)) )     // overlaps our extents (if passed)
            {
               walk->theObject->mLastQueryId = mQueryId;    // Flag the object so we know we've already visited it
               fillVector.push_back(walk->theObject);       // And save it as a found item
            }
         }
}


// Find all objects in database using derived type test function
void GridDatabase::findObjects(TestFunc testFunc, Vector<DatabaseObject *> &fillVector) const
{
   for(S32 i = 0; i < mAllObjects.size(); i++)
      if(testFunc(mAllObjects[i]->getObjectTypeNumber()))
         fillVector.push_back(mAllObjects[i]);
}


// Find all objects in database using derived type test function
void GridDatabase::findObjects(const Vector<U8> &types, Vector<DatabaseObject *> &fillVector, const Rect &extents) const
{
   static IntRect bins;
   fillBins(extents, bins);

   findObjects(types, fillVector, &extents, &bins);
}


// Find all objects in database using derived type test function
void GridDatabase::findObjects(const Vector<U8> &types, Vector<DatabaseObject *> &fillVector) const
{
   for(S32 i = 0; i < mAllObjects.size(); i++)
      if(testTypes(types, mAllObjects[i]->getObjectTypeNumber()))
         fillVector.push_back(mAllObjects[i]);
}


bool GridDatabase::testTypes(const Vector<U8> &types, U8 objectType) const
{
   for(S32 i = 0; i < types.size(); i++)
      if(types[i] == objectType)
         return true;

   return false;
}


// Find all objects in &extents derived type test function
void GridDatabase::findObjects(TestFunc testFunc, Vector<DatabaseObject *> &fillVector, const Rect &extents, bool sameQuery) const
{
   static IntRect bins;
   fillBins(extents, bins);

   findObjects(testFunc, fillVector, &extents, &bins, sameQuery);
}


void GridDatabase::dumpObjects()
{
   for(S32 x = 0; x < BucketRowCount; x++)
      for(S32 y = 0; y < BucketRowCount; y++)
         for(DatabaseBucketEntry *walk = mBuckets[x & BucketMask][y & BucketMask].nextInBucket; walk; walk = walk->nextInBucket)
         {
            DatabaseObject *object = walk->theObject;
            logprintf("Found object in (%d,%d) with extents %s", x, y, object->getExtent().toString().c_str());
            logprintf("Obj coords: %s", static_cast<BfObject *>(object)->getPos().toString().c_str());
         }
}


// Return the first non-UnknownType object, or -1 if none are found
static S32 findFirstNonUnknownTypeObject(const Vector<DatabaseObject *> &allObjects)
{
   for(S32 i = 0; i < allObjects.size(); i++)
      if(allObjects[i]->getObjectTypeNumber() != UnknownTypeNumber)
         return i;

   return -1;
}


// Get the extents of every object in the database
Rect GridDatabase::getExtents()
{
   if(mAllObjects.size() == 0)     // No objects ==> no extents!
      return Rect();

   Rect rect;

   // Think we can delete from HERE...   inserted this comment 27-Jan-2012  #########################################
   // To the best of my knowledge, the assert below has never fired 5/24/2014 -Wat

   // All this rigamarole is to make world extent correct for levels that do not overlap (0,0)
   // The problem is that the GameType is treated as an object, and has the extent (0,0), and
   // a mask of UnknownType.  Fortunately, the GameType tends to be first, so what we do is skip
   // all objects until we find an UnknownType object, then start creating our extent from there.
   // We have to assign theRect to an extent object initially to avoid getting the default coords
   // of (0,0) that are assigned by the constructor.


   //S32 first = findFirstNonUnknownTypeObject(mAllObjects);

   TNLAssert(findFirstNonUnknownTypeObject(mAllObjects) == 0, 
             "I think this should never happen -- how would an object with UnknownTypeNumber get in the database?? \
             if it does, please document it and remove this assert, along withthe rect = line below -Wat");

   //if(first == -1)      // No suitable objects found, return empty extents
   //   return Rect();

   // ...to HERE

   rect = mAllObjects[0]->getExtent();

   // Now start unioning the extents of remaining objects.  Should be all of them.
   for(S32 i = 1; i < mAllObjects.size(); i++)
      rect.unionRect(mAllObjects[i]->getExtent());

   return rect;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
DatabaseObject::DatabaseObject() 
{
   initialize();
}


// Copy constructor
DatabaseObject::DatabaseObject(const DatabaseObject &t) : Parent(t)
{  
   initialize();
   mObjectTypeNumber = t.mObjectTypeNumber; 
   mExtent = t.mExtent;
   mExtentSet = t.mExtentSet;
}


// Destructor
DatabaseObject::~DatabaseObject()
{
   TNLAssert(!mDatabase, "Must remove from database when deleting this object");
   // Do nothing
}


// Code that needs to run for both constructor and copy constructor
void DatabaseObject::initialize() 
{
   mLastQueryId = 0; 
   mExtent = Rect(); 
   mExtentSet = false;
   mDatabase = NULL;
   mBucketList = NULL;
}


// Find objects along a ray, returning first discovered object, along with time of
// that collision and a Point representing the normal angle at intersection point
//             (at least I think that's what's going on here - CE)
DatabaseObject *GridDatabase::findObjectLOS(U8 typeNumber, U32 stateIndex,
                                            const Point &rayStart, const Point &rayEnd, 
                                            float &collisionTime, Point &surfaceNormal) const
{
   return findObjectLOS(typeNumber, stateIndex, true, rayStart, rayEnd, collisionTime, surfaceNormal);
}


// Format is a passthrough to polygonLineIntersect().  Will be true for most items, false for walls in editor.
DatabaseObject *GridDatabase::findObjectLOS(U8 typeNumber, U32 stateIndex, bool format,
                                            const Point &rayStart, const Point &rayEnd,
                                            float &collisionTime, Point &surfaceNormal) const
{
   Rect queryRect(rayStart, rayEnd);

   // Use a local copy here, most callers expect our global fillVector to remain unchanged
   static Vector<DatabaseObject *> fillVector;  
   fillVector.clear();

   findObjects(typeNumber, fillVector, queryRect);

   return findObjectLOS(fillVector, stateIndex, format, rayStart, rayEnd, collisionTime, surfaceNormal);
}


DatabaseObject *GridDatabase::findObjectLOS(TestFunc testFunc, U32 stateIndex, bool format,
                                            const Point &rayStart, const Point &rayEnd, 
                                            F32 &collisionTime, Point &surfaceNormal) const
{
   Rect queryRect(rayStart, rayEnd);

   // Use a local copy here, most callers expect our global fillVector to remain unchanged
   static Vector<DatabaseObject *> fillVector;  
   fillVector.clear();

   findObjects(testFunc, fillVector, queryRect);

   return findObjectLOS(fillVector, stateIndex, format, rayStart, rayEnd, collisionTime, surfaceNormal);
}


// This variant only searches one of the items in the passed vector
DatabaseObject *GridDatabase::findObjectLOS(const Vector<DatabaseObject *> &objList, U32 stateIndex, bool format,
                                            const Point &rayStart, const Point &rayEnd, 
                                            F32 &collisionTime, Point &surfaceNormal) const
{
   Point collisionPoint;

   collisionTime = 1;      // collisionTime will be an F32 between 0 and 1 inclusive -- so this is its max value
   DatabaseObject *retObject = NULL;

   // Temp vars used to return a value from checkCollision*ForCollision
   Point norm;    
   F32 ct = collisionTime;

   Rect rect;

   for(S32 i = 0; i < objList.size(); i++)
   {
      if(!objList[i]->isCollisionEnabled())     // Skip collision-disabled objects
         continue;

      if(objList[i]->checkForCollision(rayStart, rayEnd, format, stateIndex, ct, norm))
      {
         if(ct < 0)        // Special condition... found something, but not what we want.  Don't do circle check.
            continue;

         // Found object closer than any we've found so far
         if(ct < collisionTime)
         {
            collisionTime = ct;
            surfaceNormal = norm;
            retObject = objList[i];
         }
      }
   }

   if(retObject)
      surfaceNormal.normalize();

   return retObject;
}


DatabaseObject *GridDatabase::findObjectLOS(TestFunc testFunc, U32 stateIndex,
                                            const Point &rayStart, const Point &rayEnd,
                                            float &collisionTime, Point &surfaceNormal) const
{
   return findObjectLOS(testFunc, stateIndex, true, rayStart, rayEnd, collisionTime, surfaceNormal);
}


bool GridDatabase::pointCanSeePoint(const Point &point1, const Point &point2)
{
   F32 time;
   Point coll;

   return(findObjectLOS((TestFunc)isWallType, ActualState, true, point1, point2, time, coll) == NULL);
}


void GridDatabase::computeSelectionMinMax(Point &min, Point &max)
{
   min.set( F32_MAX,  F32_MAX);
   max.set(-F32_MAX, -F32_MAX);

   for(S32 i = 0; i < mAllObjects.size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(mAllObjects[i]);

      if(obj->isSelected())
      {
         for(S32 j = 0; j < obj->getVertCount(); j++)
         {
            Point v = obj->getVert(j);

            if(v.x < min.x)   min.x = v.x;
            if(v.x > max.x)   max.x = v.x;
            if(v.y < min.y)   min.y = v.y;
            if(v.y > max.y)   max.y = v.y;
         }
      }
   }
}


S32 GridDatabase::getObjectCount() const
{
   return mAllObjects.size();
}


// Return count of objects of specified type.  Only supports certain types at the moment.
S32 GridDatabase::getObjectCount(U8 typeNumber) const
{
   if(typeNumber == GoalZoneTypeNumber)
      return mGoalZones.size();

   if(typeNumber == FlagTypeNumber)
      return mFlags.size();

   if(typeNumber == SpyBugTypeNumber)
      return mSpyBugs.size();

   if(typeNumber == PolyWallTypeNumber)
      return mPolyWalls.size();

   if(typeNumber == WallItemTypeNumber)
      return mWallitems.size();


   TNLAssert(false, "Unsupported type!");
   return 0;
}


bool GridDatabase::hasObjectOfType(U8 typeNumber) const
{
   if(typeNumber == GoalZoneTypeNumber)
      return mGoalZones.size() > 0;

   if(typeNumber == FlagTypeNumber)
      return mFlags.size() > 0;

   if(typeNumber == SpyBugTypeNumber)
      return mSpyBugs.size() > 0;

   if(typeNumber == PolyWallTypeNumber)
      return mPolyWalls.size() > 0;

   if(typeNumber == WallItemTypeNumber)
      return mWallitems.size() > 0;

   for(S32 i = 0; i < mAllObjects.size(); i++)
      if(mAllObjects[i]->getObjectTypeNumber() == typeNumber)
         return true;

   return false;
}


// Kind of hacky, kind of useful.  Only used by BotZones, and ony works because all zones are added at one time, the list does not change,
// and the index of the bot zones is stored as an ID by the zone.  If we added and removed zones from our list, this would probably not
// be a reliable way to access a specific item.  We could probably phase this out by passing pointers to zones rather than indices.
DatabaseObject *GridDatabase::getObjectByIndex(S32 index) const
{  
   if(index < 0 || index >= mAllObjects.size())
      return NULL;
   else
      return mAllObjects[index]; 
} 


void GridDatabase::updateExtents(DatabaseObject *object, const Rect &newExtents)
{
   // Does the equivalent of the following, but more efficiently:
   // removeFromDatabase();    
   // addToDatabase();

   S32 minxold, minyold, maxxold, maxyold;
   S32 minx, miny, maxx, maxy;

   Rect oldExtents = object->getExtent();

   minxold = S32(oldExtents.min.x) >> BucketWidthBitShift;
   minyold = S32(oldExtents.min.y) >> BucketWidthBitShift;
   maxxold = S32(oldExtents.max.x) >> BucketWidthBitShift;
   maxyold = S32(oldExtents.max.y) >> BucketWidthBitShift;

   minx    = S32(newExtents.min.x) >> BucketWidthBitShift;
   miny    = S32(newExtents.min.y) >> BucketWidthBitShift;
   maxx    = S32(newExtents.max.x) >> BucketWidthBitShift;
   maxy    = S32(newExtents.max.y) >> BucketWidthBitShift;

   // Don't do anything if the buckets haven't changed...
   if((minxold - minx) | (minyold - miny) | (maxxold - maxx) | (maxyold - maxy))
   {
      // They are different... remove and readd to database, but don't touch mAllObjects
      if(U32(maxx - minx) >= BucketRowCount)        maxx    = minx    + BucketRowCount - 1;
      if(U32(maxy - miny) >= BucketRowCount)        maxy    = miny    + BucketRowCount - 1;
      if(U32(maxxold >= minxold) + BucketRowCount)  maxxold = minxold + BucketRowCount - 1;
      if(U32(maxyold >= minyold) + BucketRowCount)  maxyold = minyold + BucketRowCount - 1;


      // Don't use x <= maxx, it will endless loop if maxx = S32_MAX and x overflows
      // Instead, use maxx - x >= 0, it will better handle overflows and avoid endless loop (MIN_S32 - MAX_S32 = +1)

      // Remove from the extents database for current extents...
      while(object->mBucketList)
      {
         DatabaseBucketEntry *b = object->mBucketList;
         TNLAssert(b->theObject == object, "Object mismatch");
         TNLAssert(b->prevInBucket->nextInBucket == b, "Broken linked list");
         if(b->nextInBucket)
            b->nextInBucket->prevInBucket = b->prevInBucket;
         b->prevInBucket->nextInBucket = b->nextInBucket;
         object->mBucketList = b->nextInBucketForThisObject;
         mChunker->free(b);
      }

      // ...and re-add for the new extent
      for(S32 x = minx; maxx - x >= 0; x++)
         for(S32 y = miny; maxy - y >= 0; y++)
         {
            DatabaseBucketEntry *be = mChunker->alloc();
            DatabaseBucketEntryBase *base = &mBuckets[x & BucketMask][y & BucketMask];
            be->theObject = object;
            if(base->nextInBucket)
               base->nextInBucket->prevInBucket = be;
            be->nextInBucket = base->nextInBucket;
            be->prevInBucket = base;
            base->nextInBucket = be;
            be->nextInBucketForThisObject = object->mBucketList;
            object->mBucketList = be;
         }
   }
}


////////////////////////////////////////
////////////////////////////////////////

void DatabaseObject::addToDatabase(GridDatabase *database)
{
   TNLAssert(mExtentSet, "Extent has not been set on this object!");    // Extent should set before adding the object

   if(isDatabasable())
      database->addToDatabase(this);
}


bool DatabaseObject::isInDatabase()
{
   return mDatabase != NULL;
}


bool DatabaseObject::isDeleted() 
{
   return mObjectTypeNumber == DeletedTypeNumber;
}


void DatabaseObject::removeFromDatabase(bool deleteObject)
{
   if(!mDatabase)
      return;

   getDatabase()->removeFromDatabase(this, deleteObject);
}


bool DatabaseObject::isDatabasable()
{
   return true;
}


const Vector<Point> *DatabaseObject::getCollisionPoly() const
{
   return NULL;
}


// Overridden by BfObject
void DatabaseObject::deleteThyself()
{
   delete this;
}


// Overridden by WallItem
bool DatabaseObject::checkForCollision(const Point &rayStart, const Point &rayEnd, bool format, U32 stateIndex,
                                       F32 &collisionTime, Point &surfaceNormal) const
{
   const Vector<Point> *poly = getCollisionPoly();

   if(poly)
   {
      if(poly->size() == 0)    // This can happen in the editor when a wall segment is completely hidden by another
      {
         collisionTime = -1;
         return true;
      }

      return polygonIntersectsSegmentDetailed(&poly->get(0), poly->size(), format, rayStart, rayEnd, collisionTime, surfaceNormal);
   }

   else  // No collisionPoly... try a collisionCircle
   {
      F32 radius;
      Point center;

      if(getCollisionCircle(stateIndex, center, radius))
      {
         if(!circleIntersectsSegment(center, radius, rayStart, rayEnd, collisionTime))
            return false;

         surfaceNormal = (rayStart + (rayEnd - rayStart) * collisionTime) - center;
         return true;
      }
   }
}


bool DatabaseObject::getCollisionCircle(U32 stateIndex, Point &point, F32 &radius) const
{
   return false;
}


bool DatabaseObject::isCollisionEnabled() const
{
   return true;
}


U8 DatabaseObject::getObjectTypeNumber() const
{
   return mObjectTypeNumber;
}


GridDatabase *DatabaseObject::getDatabase() const
{
   return mDatabase;
}


Rect DatabaseObject::getExtent() const
{
   return mExtent;
}


bool DatabaseObject::getExtentSet() const
{
   return mExtentSet;
}


// Update object's extents in the database -- will not add object to database if it's not already in it
void DatabaseObject::setExtent(const Rect &extents)
{
   // The following is some debugging code for seeing the ridiculous number of duplicate calls we make to this function
   // This duplication is probably a sign that there is a problem in the model.  Deal with it another day.
   //static string last = "", lastx = "";
   //logprintf("Updating %p extent to %s", this, extents.toString().c_str());
   //lastx = itos((int)(this)) + " " + extents.toString();
   //if(lastx == last) { logprintf("SAME======================="); }
   //last = lastx;

   GridDatabase *gridDB = getDatabase();

   if(gridDB)
      gridDB->updateExtents(this, extents);

   mExtent.set(extents);
   mExtentSet = true;
}


DatabaseObject *DatabaseObject::clone() const
{
   TNLAssert(false, "Clone method not implemented!");
   return NULL;
}


};

// Reusable container for searching gridDatabases
// Has to be outside of Zap namespace seems to help with debugging showing what's inside fillVector  (debugger forgets to add Zap::)
Vector<Zap::DatabaseObject *> fillVector;
Vector<Zap::DatabaseObject *> fillVector2;


