//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef COREGAME_H_
#define COREGAME_H_

#include "gameType.h"   // Parent class for CoreGameType
#include "item.h"       // Parent class for CoreItem


namespace Zap {

// Forward Declarations
class CoreItem;
class ClientInfo;

class CoreGameType : public GameType
{
   typedef GameType Parent;

private:
   Vector<SafePtr<CoreItem> > mCores;

   Vector<string> makeParameterMenuKeys() const;

public:
   static const S32 DestroyedCoreScore = 1;

   CoreGameType();            // Constructor
   virtual ~CoreGameType();   // Destructor

   bool processArguments(S32 argc, const char **argv, Level *level);
   string toLevelCode() const;

   void idle(BfObject::IdleCallPath path, U32 deltaT);


   bool isTeamCoreBeingAttacked(S32 teamIndex) const;

   // Runs on client
   void renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const;

   void addCore(CoreItem *core, S32 team);
   void removeCore(CoreItem *core);

   // What does aparticular scoring event score?
   void updateScore(ClientInfo *player, S32 team, ScoringEvent event, S32 data = 0);
   S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);
   void score(ClientInfo *destroyer, S32 coreOwningTeam, S32 score);

   void onOvertimeStarted();


#ifndef ZAP_DEDICATED
   const Vector<string> *getGameParameterMenuKeys() const;
   void renderScoreboardOrnament(S32 teamIndex, S32 xpos, S32 ypos) const;
#endif

   GameTypeId getGameTypeId() const;
   const char *getGameTypeName() const;
   const char *getShortName() const;
   const char **getInstructionString() const;
   HelpItem getGameStartInlineHelpItem() const;
   bool canBeTeamGame() const;
   bool canBeIndividualGame() const;


   TNL_DECLARE_CLASS(CoreGameType);
};


////////////////////////////////////////
////////////////////////////////////////

static const S32 CORE_PANELS = 10;     // Note that changing this will require update of all clients, and a new CS_PROTOCOL_VERSION

struct PanelGeom 
{
   Point vert[CORE_PANELS];            // Panel 0 stretches from vert 0 to vert 1
   Point mid[CORE_PANELS];             // Midpoint of Panel 0 is mid[0]
   Point repair[CORE_PANELS];
   F32 angle;
   bool isValid;

   Point getStart(S32 i) const { return vert[i % CORE_PANELS]; }
   Point getEnd(S32 i) const   { return vert[(i + 1) % CORE_PANELS]; }
};


////////////////////////////////////////
////////////////////////////////////////

class EditorAttributeMenuUI;

class CoreItem : public Item
{

typedef Item Parent;

public:
   static const F32 PANEL_ANGLE;                         // = FloatTau / (F32) CoreItem::CORE_PANELS;
   static const F32 DamageReductionRatio;
   static const U32 CoreRadius = 100;
   static const U32 CoreDefaultStartingHealth = 40;      // In ship-damage equivalents; these will be divided amongst all panels

private:
   static const U32 CoreMinWidth = 20;
   static const U32 CoreHeartbeatStartInterval = 2000;   // In milliseconds
   static const U32 CoreHeartbeatMinInterval = 500;
   static const U32 CoreAttackedWarningDuration = 600;
   static const U32 ExplosionInterval = 600;
   static const U32 ExplosionCount = 3;

   U32 mCurrentExplosionNumber;
   PanelGeom mPanelGeom;

   bool mHasExploded;
   bool mBeingAttacked;
   F32 mStartingHealth;          // Health stored in the level file, will be divided amongst panels
   F32 mStartingPanelHealth;     // Health divided up amongst panels
   void setHealth();             // Sets startingHealth value, panels will be scaled up or down as needed

   F32 mPanelHealth[CORE_PANELS];
   Timer mHeartbeatTimer;        // Client-side timer
   Timer mExplosionTimer;        // Client-side timer
   Timer mAttackedWarningTimer;  // Server-side timer
   S32 mRotateSpeed;

protected:
   enum MaskBits {
      PanelDamagedMask = Parent::FirstFreeMask << 0,  // each bit mask have own panel updates (PanelDamagedMask << n)
      PanelDamagedAllMask = ((1 << CORE_PANELS) - 1) * PanelDamagedMask,  // all bits of PanelDamagedMask
      FirstFreeMask   = Parent::FirstFreeMask << CORE_PANELS
   };

public:
   explicit CoreItem(lua_State *L = NULL);   // Combined Lua / C++ default constructor
   virtual ~CoreItem();                      // Destructor
   CoreItem *clone() const;

   static F32 getCoreAngle(U32 time);
   void renderItem(const Point &pos) const;
   bool shouldRender() const;

   const Vector<Point> *getCollisionPoly() const;
   bool getCollisionCircle(U32 state, Point &center, F32 &radius) const;
   bool collide(BfObject *otherObject);
   void degradeAllPanels(F32 amount);


   bool isBeingAttacked();

   void setStartingHealth(F32 health);
   F32 getStartingHealth() const;
   F32 getTotalCurrentHealth() const;     // Returns total current health of all panels
   F32 getHealth() const;                 // Returns overall current health of item as a ratio between 0 and 1
   bool isPanelDamaged(S32 panelIndex);
   bool isPanelInRepairRange(const Point &origin, S32 panelIndex);

   Vector<Point> getRepairLocations(const Point &repairOrigin);
   PanelGeom getPanelGeom() const;
   static void fillPanelGeom(const Point &pos, S32 time, PanelGeom &panelGeom);


   void onAddedToGame(Game *theGame);
#ifndef ZAP_DEDICATED
   void onGeomChanged();
#endif

   void damageObject(DamageInfo *theInfo);
   bool damagePanel(S32 panelIndex, F32 damage, F32 minHealth = 0);
   bool checkIfCoreIsDestroyed() const;
   void coreDestroyed(const DamageInfo *damageInfo);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);

#ifndef ZAP_DEDICATED
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void onItemExploded(Point pos);
   void doExplosion(const Point &pos);
   void doPanelDebris(S32 panelIndex);
#endif

   void idle(BfObject::IdleCallPath path);

   bool processArguments(S32 argc, const char **argv, Level *level);
   string toLevelCode() const;

   TNL_DECLARE_CLASS(CoreItem);

   void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);

   ///// Editor methods
   const char *getEditorHelpString() const;
   const char *getPrettyNamePlural() const;
   const char *getOnDockName() const;
   const char *getOnScreenName() const;

#ifndef ZAP_DEDICATED
   bool startEditingAttrs(EditorAttributeMenuUI *attributeMenu);
   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);
#endif

   F32 getEditorRadius(F32 currentScale) const;
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) const;    
   void renderDock(const Color &color) const;

   bool canBeHostile();
   bool canBeNeutral();

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(CoreItem);

   static const char *luaClassName;
   static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getCurrentHealth(lua_State *L);    // Current health = FullHealth - damage sustained
   S32 lua_getFullHealth(lua_State *L);       // Health with no damange
   S32 lua_setFullHealth(lua_State *L);     
   S32 lua_setTeam(lua_State *L);
};



} /* namespace Zap */
#endif /* COREGAME_H_ */
