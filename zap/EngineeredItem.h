// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _ENGINEEREDITEM_H_
#define _ENGINEEREDITEM_H_

#include "item.h"             // Parent
#include "Engineerable.h"     // Parent

#include "TeamConstants.h"    // For TEAM_NEUTRAL constant
#include "WeaponInfo.h"

#include "gtest/gtest_prod.h"

namespace Zap
{

class EngineeredItem : public Item, public Engineerable
{
private:
   typedef Item Parent;

   static const F32 EngineeredItemRadius;

   Rect calcExtents() const;

   virtual F32 getSelectionOffsetMagnitude();         // Provides base magnitude for getEditorSelectionOffset()

protected:
   static const F32 DamageReductionFactor;

   F32 mHealth;
   Point mAnchorNormal;
   bool mIsDestroyed;
   S32 mOriginalTeam;

   bool mSnapped;             // Item is snapped to a wall

   S32 mHealRate;             // Rate at which items will heal themselves, defaults to 0;  Heals at 10% per mHealRate seconds.
   Timer mHealTimer;          // Timer for tracking mHealRate

   Vector<Point> mCollisionPolyPoints;    // Used on server, also used for rendering on client -- computed when item is added to game
   void computeObjectGeometry();          // Populates mCollisionPolyPoints

   // Figure out where to mount this item during construction... and move it there
   void findMountPoint(const Level *level, const Point &pos);     


   BfObject *mMountSeg;    // Object we're mounted to in the editor (don't care in the game)

   enum MaskBits
   {
      InitialMask   = Parent::FirstFreeMask << 0,
      HealthMask    = Parent::FirstFreeMask << 1,
      HealRateMask  = Parent::FirstFreeMask << 2,
      FirstFreeMask = Parent::FirstFreeMask << 3
   };

public:
   EngineeredItem(S32 team = TEAM_NEUTRAL, const Point &anchorPoint = Point(0,0), const Point &anchorNormal = Point(1,0));  // Constructor
   virtual ~EngineeredItem();                                                                                               // Destructor

   virtual bool processArguments(S32 argc, const char **argv, Level *level);

   virtual void onAddedToGame(Game *theGame);

   static bool checkDeploymentPosition(const Vector<Point> &thisBounds, const GridDatabase *gb);
   void onConstructed();

   virtual void onDestroyed();
   virtual void onDisabled();
   virtual void onEnabled();

   virtual Vector<Point> getObjectGeometry(const Point &anchor, const Point &normal) const;

   virtual void setPos(lua_State *L, S32 stackIndex);
   virtual void setPos(const Point &p);

#ifndef ZAP_DEDICATED
   Point getEditorSelectionOffset(F32 currentScale);
#endif

   bool isEnabled() const;    // True if still active, false otherwise

   void explode();
   bool isDestroyed();

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void setHealRate(S32 rate);
   S32 getHealRate() const;

   void damageObject(DamageInfo *damageInfo);
   bool collide(BfObject *hitObject);
   void setHealth(F32 health);
   F32 getHealth() const;
   void healObject(S32 time);
   void mountToWall(const Point &pos, const GridDatabase *gameObjectDatabase, const GridDatabase *wallEdgeDatabase);

   void onGeomChanged();

   void getBufferForBotZone(F32 bufferRadius, Vector<Point> &points) const;

   // Figure out where to put our turrets and forcefield projectors.  Will return NULL if no mount points found.
   // Pass NULL if there is no excludedWallList.
   static BfObject *findAnchorPointAndNormal(const GridDatabase *gameObjectDatabase, 
                                             const GridDatabase *wallEdgeDatabase,
                                             const Point &pos, F32 snapDist, 
                                             bool format, Point &anchor, Point &normal);

   BfObject *getMountSegment() const;
   void setMountSegment(BfObject *mountSeg);

   //// Is item sufficiently snapped?  
   void setSnapped(bool snapped);
   bool isSnapped() const;


   ///// Editor methods
   virtual string toLevelCode() const;
   virtual void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);

#ifndef ZAP_DEDICATED
   bool startEditingAttrs(EditorAttributeMenuUI *attributeMenu);
   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);
#endif


	///// Lua interface
	LUAW_DECLARE_CLASS(EngineeredItem);

	static const char *luaClassName;
	static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   // More Lua methods that are inherited by turrets and forcefield projectors
   S32 lua_getHealth(lua_State *L);
   S32 lua_setHealth(lua_State *L);
   S32 lua_isActive(lua_State *L);
   S32 lua_getMountAngle(lua_State *L);
   S32 lua_getDisabledThreshold(lua_State *L);
   S32 lua_getHealRate(lua_State *L);
   S32 lua_setHealRate(lua_State *L);
   S32 lua_setEngineered(lua_State *L);
   S32 lua_getEngineered(lua_State *L);

   // Some overrides
   S32 lua_setGeom(lua_State *L);

   ///// Testing
   friend class LevelLoaderTest;
   FRIEND_TEST(LevelLoaderTest, EngineeredItemMounting2);
};


////////////////////////////////////////
////////////////////////////////////////

class ForceField : public BfObject
{
   typedef BfObject Parent;

private:
   Point mStart, mEnd;
   Vector<Point> mOutline;    

   Timer mDownTimer;
   bool mFieldUp;

protected:
   enum MaskBits {
      InitialMask   = Parent::FirstFreeMask << 0,
      StatusMask    = Parent::FirstFreeMask << 1,
      FirstFreeMask = Parent::FirstFreeMask << 2
   };

public:
   static const S32 FieldDownTime = 250;
   static const S32 MAX_FORCEFIELD_LENGTH = 2500;

   static const F32 ForceFieldHalfWidth;

   ForceField(S32 team = -1, Point start = Point(), Point end = Point());
   virtual ~ForceField();
   ForceField *clone() const;

   bool collide(BfObject *hitObject);
   bool intersects(ForceField *forceField);     // Return true if forcefields intersect
   void onAddedToGame(Game *theGame);
   void idle(BfObject::IdleCallPath path);

   void setStartAndEndPoints(const Point &start, const Point &end);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   const Vector<Point> *getCollisionPoly() const;

   const Vector<Point> *getOutline() const;

   static Vector<Point> computeGeom(const Point &start, const Point &end, F32 scaleFact = 1);
   static DatabaseObject *findForceFieldEnd(const GridDatabase *db, const Point &start, const Point &normal, Point &end);

   void render() const;
   void render(const Color &color) const;
   S32 getRenderSortValue();

   TNL_DECLARE_CLASS(ForceField);
};


////////////////////////////////////////
////////////////////////////////////////

class ForceFieldProjector : public EngineeredItem
{
   typedef EngineeredItem Parent;

private:
   SafePtr<ForceField> mField;

   void initialize();

   Vector<Point> getObjectGeometry(const Point &anchor, const Point &normal) const;  

   F32 getSelectionOffsetMagnitude();
   bool mNeedToCleanUpField;

public:
   static const S32 defaultRespawnTime = 0;

   explicit ForceFieldProjector(lua_State *L = NULL);                                     // Combined Lua / C++ default constructor
   ForceFieldProjector(S32 team, const Point &anchorPoint, const Point &anchorNormal);    // Constructor for when ffp is built with engineer
   virtual ~ForceFieldProjector();                                                        // Destructor

   ForceFieldProjector *clone() const;
   
   const Vector<Point> *getCollisionPoly() const;

   void createCaptiveForceField();
   
   static Vector<Point> getForceFieldProjectorGeometry(const Point &anchor, const Point &normal);
   static Point getForceFieldStartPoint(const Point &anchor, const Point &normal, F32 scaleFact = 1);

   // Get info about the forcfield that might be projected from this projector
   void getForceFieldStartAndEndPoints(Point &start, Point &end) const;

   void onAddedToGame(Game *theGame);
   void onAddedToEditor();

   void idle(BfObject::IdleCallPath path);

   void render() const;
   void onEnabled();
   void onDisabled();

   TNL_DECLARE_CLASS(ForceFieldProjector);

   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString() const;
   const char *getPrettyNamePlural() const;
   const char *getOnDockName() const;
   const char *getOnScreenName() const;

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   void renderDock(const Color &color) const;
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;

   void onGeomChanged();
   void findForceFieldEnd();                      // Find end of forcefield in editor

	///// Lua interface
	LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(ForceFieldProjector);

	static const char *luaClassName;
	static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getPos(lua_State *L);
   S32 lua_setPos(lua_State *L);
   S32 lua_setTeam(lua_State *L);
   S32 lua_removeFromGame(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class Turret : public EngineeredItem
{
   typedef EngineeredItem Parent;

private:
   Timer mFireTimer;
   F32 mCurrentAngle;

   void initialize();

   F32 getSelectionOffsetMagnitude();

public:
   explicit Turret(lua_State *L = NULL);                                   // Combined Lua / C++ default constructor
   Turret(S32 team, const Point &anchorPoint, const Point &anchorNormal);  // Constructor for when turret is built with engineer
   virtual ~Turret();                                                      // Destructor

   Turret *clone() const;

   WeaponType mWeaponFireType;

   bool processArguments(S32 argc, const char **argv, Level *level);
   string toLevelCode() const;

   static const S32 defaultRespawnTime = 0;

   static const F32 TURRET_OFFSET;                    // Distance of the turret's render location from it's attachment location
                                                      // Also serves as radius of circle of turret's body, where the turret starts
   static const S32 TurretTurnRate = 4;               // How fast can turrets turn to aim?
   static const S32 TurretPerceptionDistance = 800;   // Area to search for potential targets...

   static const S32 AimMask = Parent::FirstFreeMask;


   Vector<Point> getObjectGeometry(const Point &anchor, const Point &normal) const;
   static Vector<Point> getTurretGeometry(const Point &anchor, const Point &normal);
   
   const Vector<Point> *getCollisionPoly() const;
   const Vector<Point> *getOutline() const;

   F32 getEditorRadius(F32 currentScale) const;

   void render() const;
   void idle(IdleCallPath path);
   void onAddedToGame(Game *theGame);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(Turret);

   /////
   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString() const;
   const char *getPrettyNamePlural() const;
   const char *getOnDockName() const;
   const char *getOnScreenName() const;

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   void onGeomChanged();

   void renderDock(const Color &color) const;
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;

   ///// Lua interface
	LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Turret);

	static const char *luaClassName;
	static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   // LuaItem methods
   S32 lua_getRad(lua_State *L);
   S32 lua_getPos(lua_State *L);
   S32 lua_getAimAngle(lua_State *L);
   S32 lua_setAimAngle(lua_State *L);
   S32 lua_setWeapon(lua_State *L);
};

////////////////////////////////////////
////////////////////////////////////////

class Mortar : public EngineeredItem
{
   typedef EngineeredItem Parent;

private:
   Timer mFireTimer;
   Vector<Point> mZone;

   void initialize();

   F32 getSelectionOffsetMagnitude();

public:
   explicit Mortar(lua_State *L = NULL);                                   // Combined Lua / C++ default constructor
   Mortar(S32 team, const Point &anchorPoint, const Point &anchorNormal);  // Constructor for when mortar is built with engineer
   virtual ~Mortar();                                                      // Destructor

   Mortar *clone() const;

   WeaponType mWeaponFireType;

   bool processArguments(S32 argc, const char **argv, Level *level);
   string toLevelCode() const;

   static const S32 defaultRespawnTime = 0;

   static const F32 MORTAR_OFFSET;                    // Distance of the turret's render location from it's attachment location
                                                      // Also serves as radius of circle of turret's body, where the turret starts
   static const S32 TurretTurnRate = 4;               // How fast can turrets turn to aim?
   static const S32 TurretPerceptionDistance = 800;   // Area to search for potential targets...

   static const S32 AimMask = Parent::FirstFreeMask;


   Vector<Point> getObjectGeometry(const Point &anchor, const Point &normal) const;
   static Vector<Point> getMortarGeometry(const Point &anchor, const Point &normal);
   
   const Vector<Point> *getCollisionPoly() const;
   const Vector<Point> *getOutline() const;

   F32 getEditorRadius(F32 currentScale) const;

   void render() const;
   void idle(IdleCallPath path);
   void onAddedToGame(Game *theGame);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(Mortar);

   /////
   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString() const;
   const char *getPrettyNamePlural() const;
   const char *getOnDockName() const;
   const char *getOnScreenName() const;

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   void onGeomChanged();

   void renderDock(const Color &color) const;
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;

   ///// Lua interface
	LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Mortar);

	static const char *luaClassName;
	static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   // LuaItem methods
   S32 lua_getRad(lua_State *L);
   S32 lua_getPos(lua_State *L);
   S32 lua_setWeapon(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class EngineerModuleDeployer
{
private:
   Point mDeployPosition, mDeployNormal;
   string mErrorMessage;

public:
   // Check potential deployment position
   bool canCreateObjectAtLocation(const Level *level, const Ship *ship, U32 objectType);

   bool deployEngineeredItem(ClientInfo *clientInfo, U32 objectType);  // Deploy!
   string getErrorMessage();

   static bool findDeployPoint(const Ship *ship, U32 objectType, Point &deployPosition, Point &deployNormal);
   static string checkResourcesAndEnergy(const Ship *ship);
};


};
#endif

