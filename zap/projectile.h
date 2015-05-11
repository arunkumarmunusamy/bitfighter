//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _PROJECTILE_H_
#define _PROJECTILE_H_

#include "BfObject.h"      // Parent
#include "moveObject.h"    // Parent

#include "Point.h"
#include "WeaponInfo.h"
#include "sparkManager.h"

namespace Zap
{


class ClientInfo;

/////////////////////////////////////
/////////////////////////////////////


// Basic bullet object
class Projectile: public BfObject
{
   typedef BfObject Parent;

private:
   static const S32 COMPRESSED_VELOCITY_MAX = 2047;

   SafePtr<BfObject> mShooter;

   void initialize(WeaponType type, const Point &pos, const Point &vel, BfObject *shooter);

protected:
   enum MaskBits {
      InitialMask   = Parent::FirstFreeMask << 0,
      ExplodedMask  = Parent::FirstFreeMask << 1,
      PositionMask  = Parent::FirstFreeMask << 2,
      FirstFreeMask = Parent::FirstFreeMask << 3
   };

   Point mVelocity;

   virtual F32 getRadius() const;

public:
   U32 mTimeRemaining;
   ProjectileType mType;
   WeaponType mWeaponType;
   bool mCollided;
   bool hitShip;
   bool mAlive;
   bool mBounced;
   U32 mLiveTimeIncreases;

   Projectile(WeaponType type, const Point &pos, const Point &vel, BfObject *shooter);  // Constructor -- used when weapon is fired  
   explicit Projectile(lua_State *L = NULL);                                            // Combined Lua / C++ default constructor -- only used in Lua at the moment
   virtual ~Projectile();                                                               // Destructor

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void handleCollision(BfObject *theObject, Point collisionPoint);

   void onAddedToGame(Game *game);

   void idle(BfObject::IdleCallPath path);
   void damageObject(DamageInfo *info);
   void explode(BfObject *hitObject, Point p);

   virtual Point getRenderVel() const;
   virtual Point getActualVel() const;

   virtual bool canAddToEditor();

   void render() const;
   void renderItem(const Point &pos) const;
   bool shouldRender() const;


   BfObject *getShooter() const;

   TNL_DECLARE_CLASS(Projectile);

   //// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Projectile);

   static const char *luaClassName;
   static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getRad(lua_State *L);      // Radius of item (returns number)
   S32 lua_getVel(lua_State *L);
   S32 lua_getWeapon(lua_State *L);   // Return which type of weapon this is
   S32 lua_setVel(lua_State *L);
};


static const F32 BurstRadius = 7;
static const F32 BurstMass = 1;

// Basic burst object, and the base clase used for both mines and spybugs
class Burst : public MoveItem
{
   typedef MoveItem Parent;

private:
   SafePtr<BfObject> mShooter;

   void initialize(const Point &pos, const Point &vel, BfObject *shooter);

public:
   Burst(const Point &pos, const Point &vel, BfObject *shooter, F32 radius = BurstRadius);  // Constructor -- used when burst is fired
   explicit Burst(lua_State *L = NULL);                                                     // Combined Lua / C++ default constructor
   virtual ~Burst();                                                                        // Destructor

   enum Constants
   {
      FirstFreeMask = MoveItem::FirstFreeMask,
   };

   static const S32 InnerBlastRadius = 100;
   static const S32 OuterBlastRadius = 250;

   S32 mTimeRemaining;
   bool mExploded;
   bool collide(BfObject *otherObj);   // Things (like bullets) can collide with grenades


   WeaponType mWeaponType;
   void renderItem(const Point &pos) const;
   bool shouldRender() const;

   void idle(IdleCallPath path);
   void damageObject(DamageInfo *damageInfo);
   void doExplosion(const Point &pos);
   void explode(const Point &pos);
   bool mIsOwnedByLocalClient;  // Set client-side to determine how to render

   virtual bool canAddToEditor();

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   bool collided(BfObject *hitObject, U32 stateIndex);

   BfObject *getShooter() const;

   TNL_DECLARE_CLASS(Burst);

   //// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Burst);

   static const char *luaClassName;
   static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   virtual S32 lua_getWeapon(lua_State *L);   // Return which type of weapon this is
};


////////////////////////////////////////
////////////////////////////////////////

class Mine : public Burst
{
   typedef Burst Parent;

private:
   static const U32 FuseDelay;               // Delay of Mine explosion if exploded by another Mine

   bool mArmed;
   Timer mFuseTimer;
   void initialize(const Point &pos);

   bool getMineVisible(const ClientGame *game) const;

public:
   static const F32 SensorRadius;            // Radius of outer circle when mine is rendered
   static const S32 ArmedMask = Burst::FirstFreeMask;

   Mine(const Point &pos, BfObject *owner);  // Constructor -- used when mine is planted
   explicit Mine(lua_State *L = NULL);       // Combined Lua / C++ default constructor -- used in Lua and editor
   virtual ~Mine();                          // Destructor

   Mine *clone() const;

   bool collide(BfObject *otherObj);
   void idle(IdleCallPath path);

   void damageObject(DamageInfo *damageInfo);
   void renderItem(const Point &pos) const;

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(Mine);

   /////
   // Editor methods
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;
   void renderDock(const Color &color) const;

   const char *getEditorHelpString() const;
   const char *getPrettyNamePlural() const;
   const char *getOnDockName() const;
   const char *getOnScreenName() const;

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   virtual bool canAddToEditor();

   string toLevelCode() const;

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Mine);

   static const char *luaClassName;
   static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

class SpyBug : public Burst
{
   typedef Burst Parent;

private:
   void initialize(const Point &pos, BfObject *owner);

public:
   SpyBug(const Point &pos, BfObject *planter); // Constructor -- used when SpyBug is deployed
   explicit SpyBug(lua_State *L = NULL);        // Combined Lua / C++ default constructor -- used in Lua and editor
   virtual ~SpyBug();                           // Destructor
   SpyBug *clone() const;

   static const S32 SPY_BUG_RANGE = 300;     // How far can a spy bug see?

   bool processArguments(S32 argc, const char **argv, Level *level);
   void onAddedToGame(Game *theGame);

   bool collide(BfObject *otherObj);
   void idle(IdleCallPath path);

   void damageObject(DamageInfo *damageInfo);
   void renderItem(const Point &pos) const;

   bool isVisibleToPlayer(S32 playerTeam, bool isTeamGame); // client side
   bool isVisibleToPlayer(ClientInfo *clientInfo, bool isTeamGame); // server side

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(SpyBug);

   /////
   // Editor methods
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;
   void renderDock(const Color &color) const;

   const char *getEditorHelpString() const;
   const char *getPrettyNamePlural() const;
   const char *getOnDockName() const;
   const char *getOnScreenName() const;

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   virtual bool canAddToEditor();

   string toLevelCode() const;

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(SpyBug);

   static const char *luaClassName;
   static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


// Basic burst object, and the base clase used for both mines and spybugs
class Seeker : public MoveItem
{
private:
   typedef MoveItem Parent;

   enum Constants
   {
      FirstFreeMask = MoveItem::FirstFreeMask,
   };

   static U32 TargetAcquisitionRadius;
   static F32 MaximumAngleChangePerSecond;
   static F32 TargetSearchAngle;

   static const S32 ReassessTargetTime;

   static const S32 InnerBlastRadius;
   static const S32 OuterBlastRadius;

   SafePtr<BfObject> mShooter;
   SafePtr<BfObject> mAcquiredTarget;
   Timer mReassessTargetTimer;

   S32 mTimeRemaining;
   bool mExploded;
   bool mBounced;

   void initialize(const Point &pos, const Point &vel, F32 angle, BfObject *shooter);
   void acquireTarget();
   void emitMovementSparks();

#ifndef ZAP_DEDICATED
   UI::FxTrail mTrail;
#endif


public:
   Seeker(const Point &pos, const Point &vel, F32 angle, BfObject *shooter);    // Constructor
   explicit Seeker(lua_State *L = NULL);                                        // Combined Lua / C++ default constructor
   virtual ~Seeker();                                                           // Destructor

   WeaponType mWeaponType;

   bool collide(BfObject *otherObj);                    // Things (like bullets) can collide with grenades
   bool collided(BfObject *otherObj, U32 stateIndex);   // Things (like bullets) can collide with grenades

   virtual bool canAddToEditor();

   void renderItem(const Point &pos) const;
   bool shouldRender() const;

   void idle(IdleCallPath path);
   void damageObject(DamageInfo *damageInfo);
   void doExplosion(const Point &pos);
   void handleCollision(BfObject *hitObject, Point collisionPoint);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   BfObject *getShooter() const;

   TNL_DECLARE_CLASS(Seeker);

   //// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Seeker);

   static const char *luaClassName;
   static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   virtual S32 lua_getWeapon(lua_State *L);   // Return which type of weapon this is
};



};
#endif

