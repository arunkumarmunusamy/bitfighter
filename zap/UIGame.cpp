//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

// Use this for testing the scoreboard
//#define USE_DUMMY_PLAYER_SCORES

#include "UIGame.h"

#include "UIChat.h"
#include "UIInstructions.h"
#include "UIManager.h"
#include "UIMenus.h"

#include "barrier.h"
#include "BotNavMeshZone.h"
#include "ClientGame.h"
#include "Colors.h"
#include "Console.h"             // Our console object
#include "Cursor.h"
#include "DisplayManager.h"
#include "EngineeredItem.h"      // For EngineerModuleDeployer
#include "FontManager.h"
#include "GameManager.h"
#include "GameObjectRender.h"
#include "GameRecorderPlayback.h"
#include "gameType.h"
#include "GaugeRenderer.h"
#include "Intervals.h"
#include "Level.h"
#include "projectile.h"          // For SpyBug
#include "robot.h"
#include "ScissorsManager.h"
#include "ServerGame.h"
#include "shipItems.h"           // For EngineerBuildObjects
#include "SoundSystem.h"
#include "voiceCodec.h"

#include "stringUtils.h"
#include "RenderUtils.h"
#include "GeomUtils.h"

#include <cmath>     // Needed to compile under Linux, OSX

namespace Zap
{

// Sizes and other things to help with positioning
static const S32 SRV_MSG_FONT_SIZE = 14;
static const S32 SRV_MSG_FONT_GAP = 4;
static const S32 CHAT_FONT_SIZE = 12;
static const S32 CHAT_FONT_GAP = 3;
static const S32 CHAT_WRAP_WIDTH = 700;            // Max width of chat messages displayed in-game
static const S32 SRV_MSG_WRAP_WIDTH = 750;


// Constructor
GameUserInterface::GameUserInterface(ClientGame *game, UIManager *uiManager) :
                  Parent(game, uiManager), 
                  mVoiceRecorder(game),  //   lines expr  topdown   wrap width          font size          line gap
                  mServerMessageDisplayer(game,  6, true,  true,  SRV_MSG_WRAP_WIDTH, SRV_MSG_FONT_SIZE, SRV_MSG_FONT_GAP),
                  mChatMessageDisplayer1 (game,  5, true,  false, CHAT_WRAP_WIDTH,    CHAT_FONT_SIZE,    CHAT_FONT_GAP),
                  mChatMessageDisplayer2 (game,  5, false, false, CHAT_WRAP_WIDTH,    CHAT_FONT_SIZE,    CHAT_FONT_GAP),
                  mChatMessageDisplayer3 (game, 24, false, false, CHAT_WRAP_WIDTH,    CHAT_FONT_SIZE,    CHAT_FONT_GAP),
                  mFpsRenderer(game),
                  mLevelInfoDisplayer(game),
                  mHelpItemManager(mGameSettings)
{
   mInScoreboardMode = false;
   displayInputModeChangeAlert = false;
   mMissionOverlayActive = false;
   mCmdrsMapKeyRepeatSuppressionSystemApprovesToggleCmdrsMap = true;

   mHelperManager.initialize(game);

   mMessageDisplayMode = ShortTimeout;

   // Some debugging settings
   mDebugShowShipCoords   = false;
   mDebugShowObjectIds    = false;
   mShowDebugBots         = false;
   mDebugShowMeshZones    = false;

   mShrinkDelayTimer.setPeriod(500);

   mGotControlUpdate = false;
   
   mFiring = false;

   for(U32 i = 0; i < (U32)ShipModuleCount; i++)
   {
      mModPrimaryActivated[i] = false;
      mModSecondaryActivated[i] = false;
      mModuleDoubleTapTimer[i].setPeriod(DoubleClickTimeout);
   }
   
   mAnnouncementTimer.setPeriod(FIFTEEN_SECONDS);  
   mAnnouncement = "";

   mShowProgressBar = false;
   mProgressBarFadeTimer.setPeriod(ONE_SECOND);

   // Transition time between regular map and commander's map; in ms, higher = slower
   mCommanderZoomDelta.setPeriod(350);
   mInCommanderMap = false;

   prepareStars();
}


// Destructor  -- only runs when we're exiting to the OS
GameUserInterface::~GameUserInterface()
{
   // Do nothing
}


void GameUserInterface::onPlayerJoined()     { mHelperManager.onPlayerJoined();     }
void GameUserInterface::onPlayerQuit()       { mHelperManager.onPlayerQuit();       }
void GameUserInterface::quitEngineerHelper() { mHelperManager.quitEngineerHelper(); }  // When ship dies engineering
void GameUserInterface::exitHelper()         { mHelperManager.exitHelper();         }


void GameUserInterface::onGameOver()         
{ 
   mHelperManager.onGameOver();         
}


// This event gets run after the scoreboard display is finished
void GameUserInterface::onGameReallyAndTrulyOver()         
{ 
   mFxManager.onGameReallyAndTrulyOver();
   mHelperManager.onGameOver();         
}


void GameUserInterface::setAnnouncement(const string &message)
{
   mAnnouncement = message;
   mAnnouncementTimer.reset();
}


void GameUserInterface::onActivate()
{
   mDisableShipKeyboardInput = false;  // Make sure our ship controls are active
   mMissionOverlayActive = false;      // Turn off the mission overlay (if it was on)
   Cursor::disableCursor();            // Turn off cursor
   onMouseMoved();                     // Make sure ship pointed is towards mouse
   mCmdrsMapKeyRepeatSuppressionSystemApprovesToggleCmdrsMap = true;


   // Clear out any lingering server or chat messages
   mServerMessageDisplayer.reset();
   mChatMessageDisplayer1.reset();
   mChatMessageDisplayer2.reset();
   mChatMessageDisplayer3.reset();

   mConnectionStatsRenderer.reset();

   // Clear out any walls we were using in a previous run
   Barrier::clearRenderItems();           // TODO: Should really go in an onDeactivate method, which we don't really have
   mLevelInfoDisplayer.clearDisplayTimer();

   mLoadoutIndicator.reset();
   mShowProgressBar = true;               // Causes screen to be black before level is loaded

   mHelperManager.reset();

   for(S32 i = 0; i < ShipModuleCount; i++)
   {
      mModPrimaryActivated[i]   = false;
      mModSecondaryActivated[i] = false;
   }

   mShutdownMode = None;

   getGame()->onGameUIActivated();
}


void GameUserInterface::addStartingHelpItemsToQueue()
{
   // Queue up some initial help messages for the new users
   mHelpItemManager.reset();
   mHelpItemManager.addInlineHelpItem(WelcomeItem);            // Hello, my name is Clippy!     

   if(getGame()->getInputMode() == InputModeKeyboard)          // Show help related to basic movement and shooting
      mHelpItemManager.addInlineHelpItem(ControlsKBItem);
   else
      mHelpItemManager.addInlineHelpItem(ControlsJSItem);

   mHelpItemManager.addInlineHelpItem(ModulesAndWeaponsItem);  // Point out loadout indicators

   mHelpItemManager.addInlineHelpItem(ControlsModulesItem);    // Show how to activate modules
   mHelpItemManager.addInlineHelpItem(ChangeWeaponsItem);      // Explain how to toggle weapons
   mHelpItemManager.addInlineHelpItem(CmdrsMapItem);           // Suggest viewing cmdrs map
   mHelpItemManager.addInlineHelpItem(ChangeConfigItem);       // Changing loadouts
   mHelpItemManager.addInlineHelpItem(GameModesItem);          // Use F2 to see current mission
   mHelpItemManager.addInlineHelpItem(GameTypeAndTimer);       // Point out clock and score in LR
   mHelpItemManager.addInlineHelpItem(EnergyGaugeItem);        // Show user the energy gauge
   mHelpItemManager.addInlineHelpItem(ViewScoreboardItem);     // Show ow to get the score
   mHelpItemManager.addInlineHelpItem(TryCloakItem);           // Recommend cloaking
   mHelpItemManager.addInlineHelpItem(TryTurboItem);           // Recommend turbo

   // And finally...
   mHelpItemManager.addInlineHelpItem(F1HelpItem);             // How to get Help
   
   if(getGame()->getBotCount() == 0)
      mHelpItemManager.addInlineHelpItem(AddBotsItem);         // Add some bots?
}


void GameUserInterface::onReactivate()
{
   mDisableShipKeyboardInput = false;
   Cursor::disableCursor();    // Turn off cursor

   if(!isChatting())
      getGame()->setBusyChatting(false);

   for(S32 i = 0; i < ShipModuleCount; i++)
   {
      mModPrimaryActivated[i]   = false;
      mModSecondaryActivated[i] = false;
   }

   onMouseMoved();   // Call onMouseMoved to get ship pointed at current cursor location
   mCmdrsMapKeyRepeatSuppressionSystemApprovesToggleCmdrsMap = true;
}


// Called when level just beginning (called from GameConnection::onStartGhosting)
// We probably don't have a GameType yet, so we don't know what our level name will be
void GameUserInterface::onGameStarting()
{
   mDispWorldExtents.set(Point(0,0), 0);
   Barrier::clearRenderItems();

   addStartingHelpItemsToQueue();      // Do this here so if the helpItem manager gets turned on, items will start displaying next game

   mHelpItemManager.onGameStarting();
}


static char stringBuffer[256];

void GameUserInterface::displayErrorMessage(const char *format, ...)
{
   va_list args;

   va_start(args, format);
   vsnprintf(stringBuffer, sizeof(stringBuffer), format, args);
   va_end(args);

   displayMessage(Colors::cmdChatColor, stringBuffer);
}


void GameUserInterface::onGameTypeChanged()
{
   mLevelInfoDisplayer.onGameTypeChanged();     // Tell mLevelInfoDisplayer there is a new GameType in town
}


void GameUserInterface::displaySuccessMessage(const char *format, ...)
{
   va_list args;

   va_start(args, format);
   vsnprintf(stringBuffer, sizeof(stringBuffer), format, args);
   va_end(args);

   displayMessage(Color(0.6, 1, 0.8), stringBuffer);
}


void GameUserInterface::displayMessage(const Color &msgColor, const char *message)
{
   // Ignore empty message
   if(strcmp(message, "") == 0)
      return;

   mServerMessageDisplayer.onChatMessageReceived(msgColor, message);
}


bool GameUserInterface::isShowingMissionOverlay() const
{
   return mMissionOverlayActive;
}


void GameUserInterface::startLoadingLevel(bool engineerEnabled)
{
   mShowProgressBar = true;             // Show progress bar

   resetLevelInfoDisplayTimer();        // Start displaying the level info, now that we have it
   pregameSetup(engineerEnabled);       // Now we know all we need to initialize our loadout options
}


void GameUserInterface::doneLoadingLevel()
{
   mShowProgressBar = false;
   mProgressBarFadeTimer.reset();
   mDispWorldExtents.set(getGame()->getWorldExtents());
}


// Limit shrinkage of extent window to reduce jerky effect of some distant object disappearing from view
static F32 rectify(F32 actual, F32 disp, bool isMax, bool waiting, bool loading, U32 timeDelta, Timer &shrinkDelayTimer)
{
   const F32 ShrinkRate = 2.0f;     // Pixels per ms

   F32 delta = actual - disp;

   // When loading or really close to actual, just return the actual extent
   if(fabs(delta) < 0.1 || loading)
      return actual;

   // If the display needs to grow, we do that without delay
   if((delta < 0 && !isMax) || (delta > 0 && isMax))
   {
      shrinkDelayTimer.reset();
      return actual;
   }

   // So if we are here, the actual extents are smaller than the display, and we need to contract.

   // We have a timer that gives us a little breathing room before we start contracting.  If waiting is true, no contraction.
   if(waiting)
      return disp;
   
   // If the extents are close to the display, snap to the extents, to avoid overshooting
   if(fabs(disp - actual) <= ShrinkRate * timeDelta)
      return actual;

   // Finally, contract display extents by our ShrinkRate
   return disp + (delta > 0 ? 1 : -1) * ShrinkRate * timeDelta;
}


// Limit shrinkage of extent window to reduce jerky effect of some distant object disappearing from view
void GameUserInterface::rectifyExtents(U32 timeDelta)
{
   const Rect *worldExtentRect = getGame()->getWorldExtents();

   mShrinkDelayTimer.update(timeDelta);

   bool waiting = mShrinkDelayTimer.getCurrent() > 0;

   mDispWorldExtents.max.x = rectify(worldExtentRect->max.x, mDispWorldExtents.max.x, true,  waiting, mShowProgressBar, timeDelta, mShrinkDelayTimer);
   mDispWorldExtents.max.y = rectify(worldExtentRect->max.y, mDispWorldExtents.max.y, true,  waiting, mShowProgressBar, timeDelta, mShrinkDelayTimer);
   mDispWorldExtents.min.x = rectify(worldExtentRect->min.x, mDispWorldExtents.min.x, false, waiting, mShowProgressBar, timeDelta, mShrinkDelayTimer);
   mDispWorldExtents.min.y = rectify(worldExtentRect->min.y, mDispWorldExtents.min.y, false, waiting, mShowProgressBar, timeDelta, mShrinkDelayTimer);
}


void GameUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   // Update some timers
   mShutdownTimer.update(timeDelta);
   mWrongModeMsgDisplay.update(timeDelta);
   mProgressBarFadeTimer.update(timeDelta);
   mCommanderZoomDelta.update(timeDelta);
   mLevelInfoDisplayer.idle(timeDelta);

   if(shouldRenderLevelInfo())
      mInputModeChangeAlertDisplayTimer.reset(0);     // Supress mode change alert if this message is displayed...
   else
      mInputModeChangeAlertDisplayTimer.update(timeDelta);

   if(mAnnouncementTimer.update(timeDelta))
      mAnnouncement = "";

   for(U32 i = 0; i < (U32)ShipModuleCount; i++)
      mModuleDoubleTapTimer[i].update(timeDelta);

   // Messages
   mServerMessageDisplayer.idle(timeDelta);
   mChatMessageDisplayer1.idle(timeDelta);
   mChatMessageDisplayer2.idle(timeDelta);
   mChatMessageDisplayer3.idle(timeDelta);

   mFpsRenderer.idle(timeDelta);
   mConnectionStatsRenderer.idle(timeDelta, getGame()->getConnectionToServer());
   
   mHelperManager.idle(timeDelta);
   mVoiceRecorder.idle(timeDelta);
   mLevelListDisplayer.idle(timeDelta);

   mLoadoutIndicator.idle(timeDelta);

   // Processes sparks and teleporter effects -- 
   //    Do this even while suspended to make objects look normal while in /idling
   //    But not while playing back game recordings, idled in idleFxManager with custom timeDelta
   if(dynamic_cast<GameRecorderPlayback *>(getGame()->getConnectionToServer()) == NULL)
      mFxManager.idle(timeDelta);

   if(shouldCountdownHelpItemTimer())
      mHelpItemManager.idle(timeDelta, getGame());

   // Keep ship pointed towards mouse cmdrs map zoom transition
   if(mCommanderZoomDelta.getCurrent() > 0)               
      onMouseMoved();

   if(renderWithCommanderMap())
      rectifyExtents(timeDelta);
}


// Returns true if we can show an inline help item
bool GameUserInterface::shouldCountdownHelpItemTimer() const
{
   return getGame()->getClientInfo()->getShowLevelUpMessage() == NONE &&   // Levelup message not being shown
          !getGame()->isSpawnDelayed() &&                                  // No spawn-delay stuff going on
          getUIManager()->getCurrentUI() == this &&                        // No other UI being drawn on top
          !shouldRenderLevelInfo() &&                                      // F2 levelinfo is not displayed...
          !scoreboardIsVisible() &&                                        // Hide help when scoreboard is visible
          !mHelperManager.isHelperActive();                                // Disable help helpers are active
}


void GameUserInterface::resetInputModeChangeAlertDisplayTimer(U32 timeInMs)
{
   mInputModeChangeAlertDisplayTimer.reset(timeInMs);
}


#ifdef TNL_OS_WIN32
   extern void checkMousePos(S32 maxdx, S32 maxdy);
#endif


void GameUserInterface::toggleShowingShipCoords() { mDebugShowShipCoords = !mDebugShowShipCoords; }
void GameUserInterface::toggleShowingObjectIds()  { mDebugShowObjectIds  = !mDebugShowObjectIds;  }
void GameUserInterface::toggleShowingMeshZones()  { mDebugShowMeshZones  = !mDebugShowMeshZones;  }
void GameUserInterface::toggleShowDebugBots()     { mShowDebugBots       = !mShowDebugBots;       }


bool GameUserInterface::isShowingDebugShipCoords() const { return mDebugShowShipCoords; }


// Some FxManager passthrough functions
void GameUserInterface::clearSparks()
{
   mFxManager.clearSparks();
}


// Only runs when playing back a saved game... why?
// Allows FxManager to pause or run in slow motion with custom timeDelta
void GameUserInterface::idleFxManager(U32 timeDelta)
{
   mFxManager.idle(timeDelta);
}


F32 GameUserInterface::getCommanderZoomFraction() const
{
   return mInCommanderMap ? 1 - mCommanderZoomDelta.getFraction() : mCommanderZoomDelta.getFraction();
}


// Make sure we are not in commander's map when connection to game server is established
void GameUserInterface::resetCommandersMap()
{
   mInCommanderMap = false;
   mCommanderZoomDelta.clear();
}


void GameUserInterface::emitBlast(const Point &pos, U32 size)
{
   mFxManager.emitBlast(pos, size);
}


void GameUserInterface::emitBurst(const Point &pos, const Point &scale, const Color &color1, const Color &color2)
{
   mFxManager.emitBurst(pos, scale, color1, color2);
}


void GameUserInterface::emitDebrisChunk(const Vector<Point> &points, const Color &color, const Point &pos, const Point &vel, S32 ttl, F32 angle, F32 rotation)
{
   mFxManager.emitDebrisChunk(points, color, pos, vel, ttl, angle, rotation);
}


void GameUserInterface::emitTextEffect(const string &text, const Color &color, const Point &pos, bool relative)
{
   mFxManager.emitTextEffect(text, color, pos, relative);
}


void GameUserInterface::emitDelayedTextEffect(U32 delay, const string &text, const Color &color, const Point &pos, bool relative)
{
   mFxManager.emitDelayedTextEffect(delay, text, color, pos, relative);
}

void GameUserInterface::emitSpark(const Point &pos, const Point &vel, const Color &color, S32 ttl, UI::SparkType sparkType)
{
   mFxManager.emitSpark(pos, vel, color, ttl, sparkType);
}


void GameUserInterface::emitExplosion(const Point &pos, F32 size, const Color *colorArray, U32 numColors)
{
   mFxManager.emitExplosion(pos, size, colorArray, numColors);
}


void GameUserInterface::emitTeleportInEffect(const Point &pos, U32 type)
{
   mFxManager.emitTeleportInEffect(pos, type);
}


// Draw main game screen (client only)
void GameUserInterface::render() const
{
   if(!getGame()->isConnectedToServer())
   {
      mGL->glColor(Colors::white);
      static const SymbolString connecting("Connecting to server...", NULL, ErrorMsgContext, 30, false, AlignmentCenter);
      connecting.render(Point(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, 290));

      mGL->glColor(Colors::green);
      if(getGame()->getConnectionToServer())
      {
         SymbolString stat(GameConnection::getConnectionStateString(getGame()->getConnectionToServer()->getConnectionState()), 
                           NULL, ErrorMsgContext, 16, false, AlignmentCenter);
         stat.render(Point(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, 326));
      }

      mGL->glColor(Colors::white);
      static const SymbolString pressEsc("Press [[ESC]] to abort", NULL, ErrorMsgContext, 20, false, AlignmentCenter);
      pressEsc.render(Point(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, 366));

      return;
   }

   if(renderWithCommanderMap())
      renderGameCommander();
   else
      renderGameNormal();

   S32 level = NONE;
   //if(getGame()->getLocalRemoteClientInfo())    // Can happen when starting new level before all packets have arrived from server
      level = getGame()->getClientInfo()->getShowLevelUpMessage();

   if(level != NONE)
      renderLevelUpMessage(level);
   else if(getGame()->isSpawnDelayed())
      renderSuspendedMessage();
   
   // Fade inlineHelpItem in and out as chat widget appears or F2 levelInfo appears.
   // Don't completely hide help item when chatting -- it's jarring.  
   F32 helpItemAlpha = getBackgroundTextDimFactor(false);
   mHelpItemManager.renderMessages(getGame(), DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2.0f + 40, helpItemAlpha);

   if(dynamic_cast<GameRecorderPlayback *>(getGame()->getConnectionToServer()) == NULL)
      renderReticle();                    // Draw crosshairs if using mouse
   renderWrongModeIndicator();            // Try to avert confusion after player has changed btwn joystick and keyboard modes
   renderChatMsgs();                      // Render incoming chat and server msgs
   mLoadoutIndicator.render(getGame());   // Draw indicators for the various loadout items

   renderLevelListDisplayer();            // List of levels loaded while hosting
   renderProgressBar();                   // Status bar that shows progress of loading this level
   mVoiceRecorder.render();               // Indicator that someone is sending a voice msg

   mHelperManager.render();
   renderLostConnectionMessage();      // Renders message overlay if we're losing our connection to the server

   mFpsRenderer.render(DisplayManager::getScreenInfo()->getGameCanvasWidth());     // Display running average FPS
   mConnectionStatsRenderer.render(getGame()->getConnectionToServer());    

   GameType *gameType = getGame()->getGameType();

   if(gameType)
      gameType->renderInterfaceOverlay(DisplayManager::getScreenInfo()->getGameCanvasWidth(), 
                                       DisplayManager::getScreenInfo()->getGameCanvasHeight());
   renderLevelInfo();
   
   renderShutdownMessage();

   renderConsole();  // Rendered last, so it's always on top

#if 0
// Some code for outputting the position of the ship for finding good spawns
GameConnection *con = getGame()->getConnectionToServer();

if(con)
{
   BfObject *co = con->getControlObject();
   if(co)
   {
      Point pos = co->getActualPos() * F32(1 / 300.0f);
      RenderUtils::drawStringf(10, 550, 30, "%0.2g, %0.2g", pos.x, pos.y);
   }
}

if(mGotControlUpdate)
   RenderUtils::drawString(710, 10, 30, "CU");
#endif
}


void GameUserInterface::addInlineHelpItem(HelpItem item)
{
   mHelpItemManager.addInlineHelpItem(item);
}


void GameUserInterface::addInlineHelpItem(U8 objectType, S32 objectTeam, S32 playerTeam)
{
   mHelpItemManager.addInlineHelpItem(objectType, objectTeam, playerTeam);
}


void GameUserInterface::removeInlineHelpItem(HelpItem item, bool markAsSeen)
{
   mHelpItemManager.removeInlineHelpItem(item, markAsSeen);
}


F32 GameUserInterface::getObjectiveArrowHighlightAlpha() const
{
   return mHelpItemManager.getObjectiveArrowHighlightAlpha();
}


void GameUserInterface::setShowingInGameHelp(bool showing)
{
   if(showing != mHelpItemManager.isEnabled())
      mHelpItemManager.setEnabled(showing);       // Tell the HelpItemManager that its enabled status has changed
}


bool GameUserInterface::isShowingInGameHelp() const
{
   return mHelpItemManager.isEnabled();
}


void GameUserInterface::resetInGameHelpMessages()
{
   mHelpItemManager.resetInGameHelpMessages();
}


// Returns true if player is composing a chat message
bool GameUserInterface::isChatting() const
{
   return mHelperManager.isHelperActive(HelperMenu::ChatHelperType);
}


void GameUserInterface::renderSuspendedMessage() const
{
   if(getGame()->inReturnToGameCountdown())
   {
      static string waitMsg[] = { "", "WILL RESPAWN", "IN BLAH BLAH SECONDS", "" };
      waitMsg[2] = "IN " + ftos(ceil(F32(getGame()->getReturnToGameDelay()) * MS_TO_SECONDS)) + " SECONDS";
      renderMsgBox(waitMsg,  ARRAYSIZE(waitMsg));
   }
   else
   {
      static string readyMsg[] = { "", "PRESS ANY", "KEY TO", "RESPAWN", "" };
      renderMsgBox(readyMsg, ARRAYSIZE(readyMsg));
   }
}


void GameUserInterface::renderLevelUpMessage(S32 newLevel) const
{
   static string msg[] = { "", 
                           "CONGRATULATIONS!",
                           "YOU HAVE BEEN PROMOTED TO",
                           "LEVEL XXX",                     // <== This line will be updated below
                           "PRESS ANY KEY TO CONTINUE"
                           "" };

   msg[3] = "LEVEL " + itos(newLevel);
   renderMsgBox(msg,  ARRAYSIZE(msg));
}


// This is a helper for renderSuspendedMessage and renderLevelUpMessage.  It assumes that none of the messages
// will have [[key_bindings]] in them.  If this assumption changes, will need to replace the NULL below in the
// SymbolString() construction.
void GameUserInterface::renderMsgBox(const string *message, S32 msgLines) const
{
   Vector<SymbolShapePtr> messages(msgLines);

   for(S32 i = 0; i < msgLines; i++)
      messages.push_back(SymbolShapePtr(new SymbolString(message[i], NULL, ErrorMsgContext, 30, true)));

   // Use empty shared pointer instead of NULL
   renderMessageBox(boost::shared_ptr<SymbolShape>(), boost::shared_ptr<SymbolShape>(),
         messages.address(), messages.size(), -30, 2);
}


void GameUserInterface::renderLevelListDisplayer() const
{
   mLevelListDisplayer.render();
}


void GameUserInterface::renderLostConnectionMessage() const
{
   GameConnection *connection = getGame()->getConnectionToServer();

   if(connection && connection->lostContact())
   {
      //static string msg = "We have lost contact with the server; You can't play "
      //                    "until the connection has been re-established.\n\n"
      //                    "Trying to reconnect... [[SPINNER]]";
      //renderMessageBox("SERVER CONNECTION PROBLEMS", "", msg, -30);

      // Above: the old way of displaying connection problem

      // You may test this rendering by using /lag 0 100

      renderCenteredFancyBox(130, 54, 130, 10, Colors::red30, 0.75f, Colors::white);

      mGL->glColor(Colors::white);
      RenderUtils::drawStringc(430, 170, 30, "CONNECTION INTERRUPTED");

      const S32 x1 = 140;
      const S32 y1 = 142;

      mGL->glColor(Colors::black);
      RenderUtils::drawRect(x1 +  1, y1 + 20, x1 + 8, y1 + 30, GLOPT::TriangleFan);
      RenderUtils::drawRect(x1 + 11, y1 + 15, x1 + 18, y1 + 30, GLOPT::TriangleFan);
      RenderUtils::drawRect(x1 + 21, y1 + 10, x1 + 28, y1 + 30, GLOPT::TriangleFan);
      RenderUtils::drawRect(x1 + 31, y1 + 05, x1 + 38, y1 + 30, GLOPT::TriangleFan);
      RenderUtils::drawRect(x1 + 41, y1 + 00, x1 + 48, y1 + 30, GLOPT::TriangleFan);
      mGL->glColor(Colors::gray40);
      RenderUtils::drawRect(x1 +  1, y1 + 20, x1 + 8, y1 + 30, GLOPT::LineLoop);
      RenderUtils::drawRect(x1 + 11, y1 + 15, x1 + 18, y1 + 30, GLOPT::LineLoop);
      RenderUtils::drawRect(x1 + 21, y1 + 10, x1 + 28, y1 + 30, GLOPT::LineLoop);
      RenderUtils::drawRect(x1 + 31, y1 + 05, x1 + 38, y1 + 30, GLOPT::LineLoop);
      RenderUtils::drawRect(x1 + 41, y1 + 00, x1 + 48, y1 + 30, GLOPT::LineLoop);


      if((Platform::getRealMilliseconds() & 0x300) != 0) // Draw flashing red "X" on empty connection bars
      {
         static const F32 vertices[] = {x1 + 5, y1 - 5, x1 + 45, y1 + 35,  x1 + 5, y1 + 35, x1 + 45, y1 - 5 };
         mGL->glColor(Colors::red);
         mGL->glLineWidth(RenderUtils::DEFAULT_LINE_WIDTH * 2.f);
         mGL->renderVertexArray(vertices, 4, GLOPT::Lines);
         mGL->glLineWidth(RenderUtils::DEFAULT_LINE_WIDTH);
      }
   }
}


void GameUserInterface::renderShutdownMessage() const
{
   if(mShutdownMode == None)
      return;

   else if(mShutdownMode == ShuttingDown)
   {
      char timemsg[255];
      dSprintf(timemsg, sizeof(timemsg), "Server is shutting down in %d seconds.", (S32) (mShutdownTimer.getCurrent() / 1000));

      if(mShutdownInitiator)     // Local client intitiated the shutdown
      {
         string msg = string(timemsg) + "\n\nShutdown sequence intitated by you.\n\n" + mShutdownReason.getString();
         renderMessageBox("SERVER SHUTDOWN INITIATED", "Press [[Esc]] to cancel shutdown", msg, 7);
      }
      else                       // Remote user intiated the shutdown
      {
         char whomsg[255];
         dSprintf(whomsg, sizeof(whomsg), "Shutdown sequence initiated by %s.", mShutdownName.getString());

         string msg = string(timemsg) + "\n\n" + 
                      whomsg + "\n\n" + 
                      mShutdownReason.getString();
         renderMessageBox("SHUTDOWN INITIATED", "Press [[Esc]] to dismiss", msg, 7);
      }
   }
   else if(mShutdownMode == Canceled)
   {
      // Keep same number of messages as above, so if message changes, it will be a smooth transition
      string msg = "Server shutdown sequence canceled.\n\n"
                   "Play on!";

      renderMessageBox("SHUTDOWN CANCELED", "Press [[Esc]] to dismiss", msg, 7);
   }
}


void GameUserInterface::prepareStars()
{
   static const Color starYellow(1.0f, 1.0f, 0.7f);
   static const Color starBlue(0.7f, 0.7f, 1.0f);
   static const Color starRed(1.0f, 0.7f, 0.7f);
   static const Color starGreen(0.7f, 1.0f, 0.7f);
   static const Color starOrange(1.0f, 0.7f, 0.4f);

   // Default white-blue
   static const Color starColor(0.8f, 0.8f, 1.0f);

   // Create some random stars
   for(S32 i = 0; i < NumStars; i++)
   {
      // Positions
      mStars[i].set(TNL::Random::readF(), TNL::Random::readF());    // Between 0 and 1

      // Colors
      S32 starSeed = TNL::Random::readI(0, 100);

      if(starSeed < 2)
         mStarColors[i] = starGreen;
      else if(starSeed < 4)
         mStarColors[i] = starBlue;
      else if(starSeed < 6)
         mStarColors[i] = starRed;
      else if(starSeed < 8)
         mStarColors[i] = starOrange;
      else if(starSeed < 11)
         mStarColors[i] = starYellow;
      else
         mStarColors[i] = starColor;
   }

   // //Create some random hexagons
   //for(U32 i = 0; i < NumStars; i++)
   //{
   //   F32 x = TNL::Random::readF();
   //   F32 y = TNL::Random::readF();
   //   F32 ang = TNL::Random::readF() * Float2Pi;
   //   F32 size = TNL::Random::readF() * .1;


   //   for(S32 j = 0; j < 6; j++)
   //   {
   //      mStars[i * 6 + j].x = x + sin(ang + Float2Pi / 6 * j) * size;      // Between 0 and 1
   //      mStars[i * 6 + j].y = y + cos(ang + Float2Pi / 6 * j) * size;
   //   }
   //}
}


void GameUserInterface::shutdownInitiated(U16 time, const StringTableEntry &who, const StringPtr &why, bool initiator)
{
   mShutdownMode = ShuttingDown;
   mShutdownName = who;
   mShutdownReason = why;
   mShutdownInitiator = initiator;
   mShutdownTimer.reset(time * 1000);
}


void GameUserInterface::cancelShutdown()
{
   mShutdownMode = Canceled;
}


void GameUserInterface::showLevelLoadDisplay(bool show, bool fade)
{
   mLevelListDisplayer.showLevelLoadDisplay(show, fade);
}


void GameUserInterface::serverLoadedLevel(const string &levelName)
{
   mLevelListDisplayer.addLevelName(levelName);
}


// Draws level-load progress bar across the bottom of the screen
void GameUserInterface::renderProgressBar() const
{
   GameType *gt = getGame()->getGameType();
   if((mShowProgressBar || mProgressBarFadeTimer.getCurrent() > 0) && gt && gt->mObjectsExpected > 0)
   {
      mGL->glColor(Colors::green, mShowProgressBar ? 1 : mProgressBarFadeTimer.getFraction());

      // Outline
      const F32 left = 200;
      const F32 width = DisplayManager::getScreenInfo()->getGameCanvasWidth() - 2 * left;
      const F32 height = 10;

      // For some reason, there are occasions where the status bar doesn't progress all the way over during the load process.
      // The problem is that, for some reason, some objects do not add themselves to the loaded object counter, and this creates
      // a disconcerting effect, as if the level did not fully load.  Rather than waste any more time on this problem, we'll just
      // fill in the status bar while it's fading, to make it look like the level fully loaded.  Since the only thing that this
      // whole mechanism is used for is to display something to the user, this should work fine.
      F32 barWidth = mShowProgressBar ? S32((F32) width * (F32) gt->getObjectsLoaded() / (F32) gt->mObjectsExpected) : width;

      for(S32 i = 1; i >= 0; i--)
      {
         F32 w = i ? width : barWidth;

         F32 vertices[] = {
               left,     F32(DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin),
               left + w, F32(DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin),
               left + w, F32(DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - height),
               left,     F32(DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - height)
         };
         mGL->renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, i ? GLOPT::LineLoop : GLOPT::TriangleFan);
      }
   }
}


// Draw the reticle (i.e. the mouse cursor) if we are using keyboard/mouse
void GameUserInterface::renderReticle() const
{
   bool shouldRender = getGame()->getInputMode() == InputModeKeyboard &&   // Reticle in keyboard mode only
                       getUIManager()->isCurrentUI<GameUserInterface>();   // And not when a menu is active
   if(!shouldRender)
      return;

   Point offsetMouse = mMousePoint + Point(DisplayManager::getScreenInfo()->getGameCanvasWidth()  * 0.5f, 
                                           DisplayManager::getScreenInfo()->getGameCanvasHeight() * 0.5f);

   F32 vertices[] = {
      // Center cross-hairs
      offsetMouse.x - 15, offsetMouse.y,
      offsetMouse.x + 15, offsetMouse.y,
      offsetMouse.x,      offsetMouse.y - 15,
      offsetMouse.x,      offsetMouse.y + 15,

      // Large axes lines
      0, offsetMouse.y,
      offsetMouse.x - 30, offsetMouse.y,

      offsetMouse.x + 30, offsetMouse.y,
      (F32)DisplayManager::getScreenInfo()->getGameCanvasWidth(), offsetMouse.y,

      offsetMouse.x, 0,
      offsetMouse.x, offsetMouse.y - 30,

      offsetMouse.x, offsetMouse.y + 30,
      offsetMouse.x, (F32)DisplayManager::getScreenInfo()->getGameCanvasHeight(),
   };

#define RETICLE_COLOR Colors::green
#define COLOR_RGB RETICLE_COLOR.r, RETICLE_COLOR.g, RETICLE_COLOR.b      

   static F32 colors[] = {
   //    R,G,B   aplha
      COLOR_RGB, 0.7f,
      COLOR_RGB, 0.7f,
      COLOR_RGB, 0.7f,
      COLOR_RGB, 0.7f,

      COLOR_RGB, 0.0f,
      COLOR_RGB, 0.7f,

      COLOR_RGB, 0.7f,
      COLOR_RGB, 0.0f,

      COLOR_RGB, 0.0f,
      COLOR_RGB, 0.7f,

      COLOR_RGB, 0.7f,
      COLOR_RGB, 0.0f,
   };

#undef COLOR_RGB
#undef RETICLE_COLOR

   mGL->renderColorVertexArray(vertices, colors, ARRAYSIZE(vertices) / 2, GLOPT::Lines);
}


void GameUserInterface::renderWrongModeIndicator() const
{
   if(mWrongModeMsgDisplay.getCurrent())
   {
      // Fade for last half second
      F32 alpha = mWrongModeMsgDisplay.getCurrent() < 500 ? mWrongModeMsgDisplay.getCurrent() / 500.0f : 1.0f;

      mGL->glColor(Colors::red, alpha);
      FontManager::pushFontContext(HelperMenuContext);
      RenderUtils::drawCenteredString(225, 20, "You are in joystick mode.");
      RenderUtils::drawCenteredString(250, 20, "You can change to Keyboard input with the Options menu.");
      FontManager::popFontContext();
   }
}


void GameUserInterface::onMouseDragged()
{
   TNLAssert(false, "Is this ever called?");  // Probably not!
   onMouseMoved();
}


void GameUserInterface::onMouseMoved()
{
   Parent::onMouseMoved();

   mMousePoint.set(DisplayManager::getScreenInfo()->getMousePos()->x - DisplayManager::getScreenInfo()->getGameCanvasWidth()  / 2,
                   DisplayManager::getScreenInfo()->getMousePos()->y - DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2);

   if(mInCommanderMap)     // Ship not in center of the screen in cmdrs map.  Where is it?
   {
      Ship *ship = getGame()->getLocalPlayerShip();

      if(!ship)
         return;

      Point o = ship->getRenderPos();  // To avoid taking address of temporary
      Point p = worldToScreenPoint(&o, DisplayManager::getScreenInfo()->getGameCanvasWidth(), 
                                       DisplayManager::getScreenInfo()->getGameCanvasHeight());

      mCurrentMove.angle = atan2(mMousePoint.y + DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2 - p.y, 
                                 mMousePoint.x + DisplayManager::getScreenInfo()->getGameCanvasWidth()  / 2 - p.x);
   }

   else     // Ship is at center of the screen
      mCurrentMove.angle = atan2(mMousePoint.y, mMousePoint.x);
}


// Called from renderObjectiveArrow() & ship's onMouseMoved() when in commander's map
Point GameUserInterface::worldToScreenPoint(const Point *point,  S32 canvasWidth, S32 canvasHeight) const
{
   Ship *ship = getGame()->getLocalPlayerShip();

   if(!ship)
      return Point(0,0);

   Point position = ship->getRenderPos();    // Ship's location (which will be coords of screen's center)
   
   if(renderWithCommanderMap())              
   {
      F32 zoomFrac = getCommanderZoomFraction();
      const Rect *worldExtentRect = getGame()->getWorldExtents();

      Point worldExtents = worldExtentRect->getExtents();
      worldExtents.x *= canvasWidth  / F32(canvasWidth  - (UserInterface::horizMargin * 2));
      worldExtents.y *= canvasHeight / F32(canvasHeight - (UserInterface::vertMargin  * 2));


      F32 aspectRatio = worldExtents.x / worldExtents.y;
      F32 screenAspectRatio = F32(canvasWidth) / F32(canvasHeight);

      if(aspectRatio > screenAspectRatio)
         worldExtents.y *= aspectRatio / screenAspectRatio;
      else
         worldExtents.x *= screenAspectRatio / aspectRatio;


      Point offset = (worldExtentRect->getCenter() - position) * zoomFrac + position;
      Point visSize = getGame()->computePlayerVisArea(ship) * 2;
      Point modVisSize = (worldExtents - visSize) * zoomFrac + visSize;

      Point visScale(canvasWidth / modVisSize.x, canvasHeight / modVisSize.y );

      Point ret = (*point - offset) * visScale + Point((canvasWidth / 2), (canvasHeight / 2));
      return ret;
   }
   else                       // Normal map view
   {
      Point visExt = getGame()->computePlayerVisArea(ship);
      Point scaleFactor((canvasWidth / 2) / visExt.x, (canvasHeight / 2) / visExt.y);

      Point ret = (*point - position) * scaleFactor + Point((canvasWidth / 2), (canvasHeight / 2));
      return ret;
   }
}


// Returns true if we are either in the cmdrs map, or are transitioning
bool GameUserInterface::renderWithCommanderMap() const
{
   return mInCommanderMap || mCommanderZoomDelta.getCurrent() > 0;
}


// Is engineer enabled on this level?  Only set at beginning of level, not changed during game
void GameUserInterface::pregameSetup(bool engineerEnabled)
{
   mHelperManager.pregameSetup(engineerEnabled);
}


void GameUserInterface::setSelectedEngineeredObject(U32 objectType)
{
   mHelperManager.setSelectedEngineeredObject(objectType);
}


void GameUserInterface::activateHelper(HelperMenu::HelperMenuType helperType, bool activatedWithChatCmd)
{
   mHelperManager.activateHelper(helperType, activatedWithChatCmd);
   playBoop();
}


// Used only for testing
bool GameUserInterface::isHelperActive(HelperMenu::HelperMenuType helperType) const
{
   return mHelperManager.isHelperActive(helperType);
}


// Used only for testing
const HelperMenu *GameUserInterface::getActiveHelper() const
{
   return mHelperManager.getActiveHelper();
}


void GameUserInterface::renderEngineeredItemDeploymentMarker(Ship *ship) const
{
   mHelperManager.renderEngineeredItemDeploymentMarker(ship);
}


// Runs on client
void GameUserInterface::dropItem()
{
   if(!getGame()->getConnectionToServer())
      return;

   Ship *ship = getGame()->getLocalPlayerShip();

   GameType *gt = getGame()->getGameType();
   if(!ship || !gt)
      return;

   if(!gt->isCarryingItems(ship))
   {
      displayErrorMessage("You don't have any items to drop!");
      return;
   }

   gt->c2sDropItem();
}


// Select next weapon
void GameUserInterface::chooseNextWeapon()
{
   GameType *gameType = getGame()->getGameType();
   if(gameType)
      gameType->c2sChooseNextWeapon();
}


void GameUserInterface::choosePrevWeapon()
{
   GameType *gameType = getGame()->getGameType();
   if(gameType)
      gameType->c2sChoosePrevWeapon();
}


// Select a weapon by its index
void GameUserInterface::selectWeapon(U32 indx)
{
   GameType *gameType = getGame()->getGameType();
   if(gameType)
      gameType->c2sSelectWeapon(indx);

   mHelpItemManager.removeInlineHelpItem(ChangeWeaponsItem, true);      // User has demonstrated this skill
}


void GameUserInterface::activateModule(S32 index)
{
   // Still active, just return
   if(!getGame() || !getGame()->getLocalPlayerShip() || mModPrimaryActivated[index])
      return;

   // Activate module primary component
   mModPrimaryActivated[index] = true;
   setModulePrimary(getGame()->getLocalPlayerShip()->getModule(index), true);

   // If the module secondary double-tap timer hasn't run out, activate the secondary component
   if(mModuleDoubleTapTimer[index].getCurrent() != 0)
      mModSecondaryActivated[index] = true;

   // Now reset the double-tap timer since we've just activate this module
   mModuleDoubleTapTimer[index].reset();

   // Player figured out how to activate their modules... skip related help
   mHelpItemManager.removeInlineHelpItem(ControlsModulesItem, true);           

   if(getGame()->getLocalPlayerShip()->getModule(index) == ModuleCloak)
      mHelpItemManager.removeInlineHelpItem(TryCloakItem, true);     // Already tried it!
   else if(getGame()->getLocalPlayerShip()->getModule(index) == ModuleBoost)
      mHelpItemManager.removeInlineHelpItem(TryTurboItem, true);     // Already tried it!
}


void GameUserInterface::toggleLevelRating()
{
   if(!getGame()->canRateLevel())      // Will display any appropriate error messages
      return;

   PersonalRating newRating = getGame()->toggleLevelRating();  // Change rating and get new value

   string msg = "Your rating: " + getPersonalRatingString(newRating);
   displaySuccessMessage(msg.c_str());

   mHelpItemManager.removeInlineHelpItem(RateThisLevel, true);             // Demonstrated ability to rate a level!
}


// Static method
string GameUserInterface::getPersonalRatingString(PersonalRating rating)
{
   if(rating == RatingGood)      return "+1";
   if(rating == RatingNeutral)   return "0";
   if(rating == RatingBad)       return "-1";

   return getTotalRatingString((S16)rating);    // Handles UnknownRating, Unrated
}


// Static method
string GameUserInterface::getTotalRatingString(S16 rating)
{
   if(rating == UnknownRating)      return "?";
   if(rating == Unrated)            return "Unrated";

   return (rating > 0 ? "+" : "") + itos(rating);
}


// A new loadout has arrived
void GameUserInterface::newLoadoutHasArrived(const LoadoutTracker &loadout)
{
   mLoadoutIndicator.newLoadoutHasArrived(loadout);
}


void GameUserInterface::setActiveWeapon(U32 weaponIndex)
{
   mLoadoutIndicator.setActiveWeapon(weaponIndex);
}


void GameUserInterface::updateLeadingPlayerAndScore()
{
   mTimeLeftRenderer.updateLeadingPlayerAndScore(getGame());
}


// Used?
void GameUserInterface::setModulePrimary(ShipModule module, bool isActive)
{
   mLoadoutIndicator.setModulePrimary(module, isActive);
}


void GameUserInterface::setModuleSecondary(ShipModule module, bool isActive)
{
   mLoadoutIndicator.setModuleSecondary(module, isActive);
}


// Returns the width of the current loadout, as rendered
S32 GameUserInterface::getLoadoutIndicatorWidth() const
{
   return mLoadoutIndicator.getWidth();
}


bool GameUserInterface::scoreboardIsVisible() const
{
   // GameType can be NULL when first starting up
   return mInScoreboardMode || getGame()->isGameOver();
}


Point GameUserInterface::getTimeLeftIndicatorWidthAndHeight() const
{
   return mTimeLeftRenderer.render(getGame()->getGameType(), scoreboardIsVisible(), getGame()->areTeamsLocked(), false);
}


// Key pressed --> take action!
// Handles all keypress events, including mouse clicks and controller button presses
bool GameUserInterface::onKeyDown(InputCode inputCode)
{
   // Kind of hacky, but this will unsuspend and swallow the keystroke, which is what we want
   if(!mHelperManager.isHelperActive())
   {
      if(getGame()->getClientInfo()->getShowLevelUpMessage() != NONE)
      {
         getGame()->undelaySpawn();
         if(inputCode != KEY_ESCAPE)  // Don't swollow escape
            return true;
      }
      else if(getGame()->isSpawnDelayed())
      {
         // Allow scoreboard and the various chats while idle
         if(!checkInputCode(BINDING_LOBBYCHAT, inputCode) &&
            !checkInputCode(BINDING_GLOBCHAT,  inputCode)  &&
            !checkInputCode(BINDING_TEAMCHAT,  inputCode)  &&
            !checkInputCode(BINDING_CMDCHAT,   inputCode)  &&
            !checkInputCode(BINDING_SCRBRD,    inputCode))
         {
            getGame()->undelaySpawn();
            if(inputCode != KEY_ESCAPE)  // Don't swollow escape: Lagged out and can't un-idle to bring up the menu?
               return true;
         }
      }
   }

   if(checkInputCode(BINDING_LOBBYCHAT, inputCode))
      getGame()->setBusyChatting(true);

   if(Parent::onKeyDown(inputCode))    // Let parent try handling the key
      return true;

   if(GameManager::gameConsole->onKeyDown(inputCode))   // Pass the key on to the console for processing
      return true;

   if(checkInputCode(BINDING_HELP, inputCode))   // Turn on help screen
   {
      playBoop();
      getGame()->setBusyChatting(true);

      // If we have a helper, let that determine what happens when the help key is pressed.  Otherwise, show help normally.
      if(mHelperManager.isHelperActive())
         mHelperManager.activateHelp(getUIManager());
      else
         getUIManager()->activate<InstructionsUserInterface>();

      mHelpItemManager.removeInlineHelpItem(F1HelpItem, true);    // User knows how to access help

      return true;
   }

   // Ctrl-/ toggles console window for the moment
   // Only open when there are no active helpers
   if(!mHelperManager.isHelperActive() && inputCode == KEY_SLASH && InputCodeManager::checkModifier(KEY_CTRL))
   {
      if(GameManager::gameConsole->isOk())        // Console is only not Ok if something bad has happened somewhere
         GameManager::gameConsole->toggleVisibility();

      return true;
   }

   if(checkInputCode(BINDING_MISSION, inputCode))    // F2
   {
      onMissionKeyPressed();

      return true;
   }

   if(inputCode == KEY_M && InputCodeManager::checkModifier(KEY_CTRL))        // Ctrl+M, for now, to cycle through message dispaly modes
   {
      toggleChatDisplayMode();
      return true;
   }

   // Disallow chat when a level is loading.  This is a workaround for disappearing chats during
   // level transitions.  The true fix is probably to move chats from the GameType and into the GameConnection
   if(!mShowProgressBar && mHelperManager.isHelperActive() && mHelperManager.processInputCode(inputCode))   // Will return true if key was processed
   {
      // Experimental, to keep ship from moving after entering a quick chat that has the same shortcut as a movement key
      InputCodeManager::setState(inputCode, false);
      return true;
   }

   // If we're not in a helper, and we apply the engineer module, then we can handle that locally by displaying a menu or message
   if(!mHelperManager.isHelperActive())
   {
      Ship *ship = getGame()->getLocalPlayerShip();
         
      if(ship)
      {
         if((checkInputCode(BINDING_MOD1, inputCode) && ship->getModule(0) == ModuleEngineer) ||
            (checkInputCode(BINDING_MOD2, inputCode) && ship->getModule(1) == ModuleEngineer))
         {
            string msg = EngineerModuleDeployer::checkResourcesAndEnergy(ship);  // Returns "" if ok, error message otherwise

            if(msg != "")
               displayErrorMessage(msg.c_str());
            else
               activateHelper(HelperMenu::EngineerHelperType);

            return true;
         }
      }
   }

#ifdef TNL_DEBUG
   // These commands only available in debug builds
   if(inputCode == KEY_H && InputCodeManager::checkModifier(KEY_SHIFT))     // Shift+H to show next real HelpItem
      mHelpItemManager.debugAdvanceHelpItem();

   if(inputCode == KEY_H && InputCodeManager::checkModifier(KEY_CTRL))     // Ctrl+H to show next dummy HelpItem
      mHelpItemManager.debugShowNextSampleHelpItem();

#endif

   if(!GameManager::gameConsole->isVisible())
   {
      if(!isChatting())
         return processPlayModeKey(inputCode);
   }

   return false;
}


// User has pressed F2
void GameUserInterface::onMissionKeyPressed()
{
   if(!mMissionOverlayActive)
   {
      mMissionOverlayActive = true;

      if(!mLevelInfoDisplayer.isDisplayTimerActive())
         mLevelInfoDisplayer.onActivated();

      mLevelInfoDisplayer.clearDisplayTimer();                    // Clear timer so releasing F2 will hide the display
      mHelpItemManager.removeInlineHelpItem(GameModesItem, true); // User seems to know about F2, unqueue help message
   }
}


void GameUserInterface::onMissionKeyReleased()
{
   mMissionOverlayActive = false;
   mLevelInfoDisplayer.onDeactivated();
}


void GameUserInterface::onTextInput(char ascii)
{
   if(GameManager::gameConsole->isVisible())
      GameManager::gameConsole->onKeyDown(ascii);

   mHelperManager.onTextInput(ascii);
}


// Helper function...
static void saveLoadoutPreset(ClientGame *game, const LoadoutTracker *loadout, S32 slot)
{
   game->getSettings()->setLoadoutPreset(loadout, slot);
   game->displaySuccessMessage(("Current loadout saved as preset " + itos(slot + 1)).c_str());
}


static void loadLoadoutPreset(ClientGame *game, S32 slot)
{
   LoadoutTracker loadout = game->getSettings()->getLoadoutPreset(slot);

   if(!loadout.isValid())
   {
      string msg = "Preset " + itos(slot + 1) + " is undefined -- to define it, try Ctrl+" + itos(slot + 1);
      game->displayErrorMessage(msg.c_str());
      return;
   }

   //GameType *gameType = game->getGameType();
   //if(!gameType)
   //   return;

   game->requestLoadoutPreset(slot);
}


bool checkInputCode(InputCode codeUserEntered, InputCode codeToActivateCommand)
{
   return codeUserEntered == codeToActivateCommand;
}


// Helper function -- checks input keys and sees if we should start chatting.  Returns true if entered chat mode, false if not.
bool GameUserInterface::checkEnterChatInputCode(InputCode inputCode)
{
   if(checkInputCode(BINDING_TEAMCHAT, inputCode))          // Start entering a team chat msg
      mHelperManager.activateHelper(ChatHelper::TeamChat);
   else if(checkInputCode(BINDING_GLOBCHAT, inputCode))     // Start entering a global chat msg
      mHelperManager.activateHelper(ChatHelper::GlobalChat);
   else if(checkInputCode(BINDING_CMDCHAT, inputCode))      // Start entering a command
      mHelperManager.activateHelper(ChatHelper::CmdChat);
   else
      return false;

   return true;
}


// Can only get here if we're not in chat mode
bool GameUserInterface::processPlayModeKey(InputCode inputCode)
{
   // The following keys are allowed in both play mode and in loadout or
   // engineering menu modes if not used in the loadout menu above
   // They are currently hardcoded, both here and in the instructions
   if(inputCode == KEY_CLOSEBRACKET && InputCodeManager::checkModifier(KEY_ALT))          // Alt+] advances bots by one step if frozen
      EventManager::get()->addSteps(1);
   else if(inputCode == KEY_CLOSEBRACKET && InputCodeManager::checkModifier(KEY_CTRL))    // Ctrl+] advances bots by 10 steps if frozen
      EventManager::get()->addSteps(10);

   else if(checkInputCode(BINDING_LOAD_PRESET_1, inputCode))  // Loading loadout presets
      loadLoadoutPreset(getGame(), 0);
   else if(checkInputCode(BINDING_LOAD_PRESET_2, inputCode))
      loadLoadoutPreset(getGame(), 1);
   else if(checkInputCode(BINDING_LOAD_PRESET_3, inputCode))
      loadLoadoutPreset(getGame(), 2);

   else if(checkInputCode(BINDING_SAVE_PRESET_1, inputCode))  // Saving loadout presets
      saveLoadoutPreset(getGame(), mLoadoutIndicator.getLoadout(), 0);
   else if(checkInputCode(BINDING_SAVE_PRESET_2, inputCode))
      saveLoadoutPreset(getGame(), mLoadoutIndicator.getLoadout(), 1);
   else if(checkInputCode(BINDING_SAVE_PRESET_3, inputCode))
      saveLoadoutPreset(getGame(), mLoadoutIndicator.getLoadout(), 2);

   else if(checkInputCode(BINDING_MOD1, inputCode))
      activateModule(0);
   else if(checkInputCode(BINDING_MOD2, inputCode))
      activateModule(1);
   else if(checkInputCode(BINDING_FIRE, inputCode))
   {
      mFiring = true;
      mHelpItemManager.removeInlineHelpItem(ControlsKBItem, true, 0xFF - 1);     // Player has demonstrated knowledge of how to fire
   }
   else if(checkInputCode(BINDING_SELWEAP1, inputCode))
      selectWeapon(0);
   else if(checkInputCode(BINDING_SELWEAP2, inputCode))
      selectWeapon(1);
   else if(checkInputCode(BINDING_SELWEAP3, inputCode))
      selectWeapon(2);
   else if(checkInputCode(BINDING_FPS, inputCode))
   {
      if(InputCodeManager::checkModifier(KEY_CTRL))
         mConnectionStatsRenderer.toggleVisibility();
      else
         mFpsRenderer.toggleVisibility();
   }
   else if(checkInputCode(BINDING_ADVWEAP, inputCode))
      chooseNextWeapon();

   // By default, Handle mouse wheel. Users can change it in "Define Keys" option
   else if(checkInputCode(BINDING_ADVWEAP2, inputCode))
      chooseNextWeapon();
   else if(checkInputCode(BINDING_PREVWEAP, inputCode))
      choosePrevWeapon();
   else if(checkInputCode(BINDING_TOGGLE_RATING, inputCode))
      toggleLevelRating();

   else if(inputCode == KEY_ESCAPE || inputCode == BUTTON_BACK)
   {
      if(mShutdownMode == ShuttingDown)
      {
         if(mShutdownInitiator)
         {
            getGame()->getConnectionToServer()->c2sRequestCancelShutdown();
            mShutdownMode = Canceled;
         }
         else
            mShutdownMode = None;

         return true;
      }
      else if(mShutdownMode == Canceled)
      {
         mShutdownMode = None;
         return true;
      }

      playBoop();

      if(!getGame()->isConnectedToServer())     // Perhaps we're still joining?
      {
         getGame()->closeConnectionToGameServer();
         getUIManager()->reactivate(getUIManager()->getUI<MainMenuUserInterface>());      // Back to main menu
      }
      else
      {
         getGame()->setBusyChatting(true);
         getUIManager()->activate<GameMenuUserInterface>();
      }
   }
   else if(checkInputCode(BINDING_CMDRMAP, inputCode))
   {
      if(!mCmdrsMapKeyRepeatSuppressionSystemApprovesToggleCmdrsMap)
         return true;

      toggleCommanderMap();

      // Suppress key repeat by disabling cmdrs map until keyUp event is received
      mCmdrsMapKeyRepeatSuppressionSystemApprovesToggleCmdrsMap = false;
      
      // Now that we've demonstrated use of cmdrs map, no need to tell player about it
      mHelpItemManager.removeInlineHelpItem(CmdrsMapItem, true);  
   }

   else if(checkInputCode(BINDING_SCRBRD, inputCode))
   {     // (braces needed)
      if(!mInScoreboardMode)    // We're activating the scoreboard
      {
         mInScoreboardMode = true;
         GameType *gameType = getGame()->getGameType();
         if(gameType)
            gameType->c2sRequestScoreboardUpdates(true);

         mHelpItemManager.removeInlineHelpItem(ViewScoreboardItem, true);  // User found the tab key!
      }
   }
   else if(checkInputCode(BINDING_TOGVOICE, inputCode))
   {     // (braces needed)
      if(!mVoiceRecorder.mRecordingAudio)  // Turning recorder on
         mVoiceRecorder.start();
   }

   // The following keys are only allowed when there are no helpers or when the top helper permits
   else if(mHelperManager.isChatAllowed())    
   {
      if(checkEnterChatInputCode(inputCode))
         return true;

      // These keys are only available when there is no helper active
      if(!mHelperManager.isHelperActive())
      {
         if(checkInputCode(BINDING_QUICKCHAT, inputCode))
            activateHelper(HelperMenu::QuickChatHelperType);
         else if(checkInputCode(BINDING_LOADOUT, inputCode))
            activateHelper(HelperMenu::LoadoutHelperType);
         else if(checkInputCode(BINDING_DROPITEM, inputCode))
            dropItem();
         // Check if the user is trying to use keyboard to move when in joystick mode
         else if(getGame()->getInputMode() == InputModeJoystick)
            checkForKeyboardMovementKeysInJoystickMode(inputCode);
      }
   }
   else
      return false;

   return true;
}


// Toggles commander's map activation status
void GameUserInterface::toggleCommanderMap()
{
   mInCommanderMap = !mInCommanderMap;
   mCommanderZoomDelta.invert();

   if(mInCommanderMap)
      playSoundEffect(SFXUICommUp);
   else
      playSoundEffect(SFXUICommDown);

   getGame()->setUsingCommandersMap(mInCommanderMap);
}


SFXHandle GameUserInterface::playSoundEffect(U32 profileIndex, F32 gain) const
{
   return getUIManager()->playSoundEffect(profileIndex, gain);
}


// Show a message if the user starts trying to play with keyboard in joystick mode
void GameUserInterface::checkForKeyboardMovementKeysInJoystickMode(InputCode inputCode)
{
   if(checkInputCode(BINDING_UP,    inputCode) ||
      checkInputCode(BINDING_DOWN,  inputCode) ||
      checkInputCode(BINDING_LEFT,  inputCode) ||
      checkInputCode(BINDING_RIGHT, inputCode))
         mWrongModeMsgDisplay.reset(THREE_SECONDS);
}


// This is a bit complicated to explain... basically, when chatRelated is true, it won't apply a dimming factor
// when entering a chat message.  When false, it will.
F32 GameUserInterface::getBackgroundTextDimFactor(bool chatRelated) const
{
   F32 helperManagerFactor = chatRelated ? 
         mHelperManager.getDimFactor() : 
         MAX(mHelperManager.getFraction(), UI::DIM_LEVEL);

   // Hide help message when scoreboard is visible
   if(mInScoreboardMode)
      helperManagerFactor = 0;

   return MIN(helperManagerFactor, mLevelInfoDisplayer.getFraction());
}


// Display proper chat queue based on mMessageDisplayMode.  These displayers are configured in the constructor. 
void GameUserInterface::renderChatMsgs() const
{
   bool chatDisabled = !mHelperManager.isChatAllowed();
   bool announcementActive = (mAnnouncementTimer.getCurrent() != 0);

   F32 alpha = 1; // getBackgroundTextDimFactor(true);

   if(mMessageDisplayMode == ShortTimeout)
      mChatMessageDisplayer1.render(IN_GAME_CHAT_DISPLAY_POS, chatDisabled, announcementActive, alpha);
   else if(mMessageDisplayMode == ShortFixed)
      mChatMessageDisplayer2.render(IN_GAME_CHAT_DISPLAY_POS, chatDisabled, announcementActive, alpha);
   else
      mChatMessageDisplayer3.render(IN_GAME_CHAT_DISPLAY_POS, chatDisabled, announcementActive, alpha);

   mServerMessageDisplayer.render(messageMargin, chatDisabled, false, alpha);

   if(announcementActive)
      renderAnnouncement(IN_GAME_CHAT_DISPLAY_POS);
}


void GameUserInterface::renderAnnouncement(S32 pos) const
{
   mGL->glColor(Colors::red);
   mGL->glLineWidth(RenderUtils::LINE_WIDTH_4);

   S32 x = RenderUtils::drawStringAndGetWidth(UserInterface::horizMargin, pos, 16, "*** ");
   x += RenderUtils::drawStringAndGetWidth(UserInterface::horizMargin + x, pos, 16, mAnnouncement.c_str());
   RenderUtils::drawString(UserInterface::horizMargin + x, pos, 16, " ***");

   mGL->glLineWidth(RenderUtils::DEFAULT_LINE_WIDTH);
}


void GameUserInterface::onKeyUp(InputCode inputCode)
{
   // These keys works in any mode!  And why not??

   if(checkInputCode(BINDING_MISSION, inputCode))    // F2
      onMissionKeyReleased();

   else if(checkInputCode(BINDING_MOD1, inputCode))
   {
      mModPrimaryActivated[0] = false;
      mModSecondaryActivated[0] = false;

      if(getGame()->getLocalPlayerShip())    // Sometimes false if in hit any key to continue mode
         setModulePrimary(getGame()->getLocalPlayerShip()->getModule(0), false);
   }
   else if(checkInputCode(BINDING_MOD2, inputCode))
   {
      mModPrimaryActivated[1] = false;
      mModSecondaryActivated[1] = false;

      if(getGame()->getLocalPlayerShip())    // Sometimes false if in hit any key to continue mode
         setModulePrimary(getGame()->getLocalPlayerShip()->getModule(1), false);
   }
   else if(checkInputCode(BINDING_FIRE, inputCode))
      mFiring = false;
   else if(checkInputCode(BINDING_SCRBRD, inputCode))
   {     // (braces required)
      if(mInScoreboardMode)     // We're turning scoreboard off
      {
         mInScoreboardMode = false;
         GameType *gameType = getGame()->getGameType();
         if(gameType)
            gameType->c2sRequestScoreboardUpdates(false);
      }
   }
   else if(checkInputCode(BINDING_TOGVOICE, inputCode))
   {     // (braces required)
      if(mVoiceRecorder.mRecordingAudio)  // Turning recorder off
         mVoiceRecorder.stop();
   }
   else if(checkInputCode(BINDING_CMDRMAP, inputCode))
      mCmdrsMapKeyRepeatSuppressionSystemApprovesToggleCmdrsMap = true;
}


void GameUserInterface::receivedControlUpdate(bool recvd)
{
   mGotControlUpdate = recvd;
}


bool GameUserInterface::isInScoreboardMode()
{
   return mInScoreboardMode;
}


static void joystickUpdateMove(ClientGame *game, GameSettings *settings, Move *theMove)
{
   // One of each of left/right axis and up/down axis should be 0 by this point
   // but let's guarantee it..   why?
   theMove->x = game->mJoystickInputs[JoystickMoveAxesRight] - 
                game->mJoystickInputs[JoystickMoveAxesLeft];
   theMove->x = MAX(theMove->x, -1);
   theMove->x = MIN(theMove->x, 1);
   theMove->y =  game->mJoystickInputs[JoystickMoveAxesDown] - 
                 game->mJoystickInputs[JoystickMoveAxesUp];
   theMove->y = MAX(theMove->y, -1);
   theMove->y = MIN(theMove->y, 1);

   //logprintf(
   //      "Joystick axis values. Move: Left: %f, Right: %f, Up: %f, Down: %f\nShoot: Left: %f, Right: %f, Up: %f, Down: %f ",
   //      mJoystickInputs[MoveAxesLeft],  mJoystickInputs[MoveAxesRight],
   //      mJoystickInputs[MoveAxesUp],    mJoystickInputs[MoveAxesDown],
   //      mJoystickInputs[ShootAxesLeft], mJoystickInputs[ShootAxesRight],
   //      mJoystickInputs[ShootAxesUp],   mJoystickInputs[ShootAxesDown]
   //      );

   //logprintf(
   //         "Move values. Move: Left: %f, Right: %f, Up: %f, Down: %f",
   //         theMove->left, theMove->right,
   //         theMove->up, theMove->down
   //         );


   //logprintf("XY from shoot axes. x: %f, y: %f", x, y);


   Point p(game->mJoystickInputs[JoystickShootAxesRight] - 
           game->mJoystickInputs[JoystickShootAxesLeft], 
                             game->mJoystickInputs[JoystickShootAxesDown]  - 
                             game->mJoystickInputs[JoystickShootAxesUp]);

   F32 fact =  p.len();

   if(fact > 0.66f)        // It requires a large movement to actually fire...
   {
      theMove->angle = atan2(p.y, p.x);
      theMove->fire = true;
   }
   else if(fact > 0.25)    // ...but you can change aim with a smaller one
   {
      theMove->angle = atan2(p.y, p.x);
      theMove->fire = false;
   }
   else
      theMove->fire = false;
}


// Return current move (actual move processing in ship.cpp)
// Will also transform move into "relative" mode if needed
// At the end, all input supplied here will be overwritten if
// we are using a game controller.  What a mess!
Move *GameUserInterface::getCurrentMove()
{
   Move *move = &mCurrentMove;

   if(!mDisableShipKeyboardInput && getUIManager()->isCurrentUI<GameUserInterface>() && !GameManager::gameConsole->isVisible())
   {
      // Some helpers (like TeamShuffle) like to disable movement when they are active
      if(mHelperManager.isMovementDisabled())
      {
         mCurrentMove.x = 0;
         mCurrentMove.y = 0;
      }
      else
      {
         mCurrentMove.x = F32((InputCodeManager::getState(getInputCode(mGameSettings, BINDING_RIGHT)) ? 1 : 0) -
                              (InputCodeManager::getState(getInputCode(mGameSettings, BINDING_LEFT))  ? 1 : 0));

         mCurrentMove.y = F32((InputCodeManager::getState(getInputCode(mGameSettings, BINDING_DOWN))  ? 1 : 0) -
                              (InputCodeManager::getState(getInputCode(mGameSettings, BINDING_UP))    ? 1 : 0));
      }

      // If player is moving, do not show move instructions
      if(mCurrentMove.y > 0 || mCurrentMove.x > 0)
         mHelpItemManager.removeInlineHelpItem(ControlsKBItem, true, 1);


      mCurrentMove.fire = mFiring;

      for(U32 i = 0; i < (U32)ShipModuleCount; i++)
      {
         mCurrentMove.modulePrimary[i]   = mModPrimaryActivated[i];
         mCurrentMove.moduleSecondary[i] = mModSecondaryActivated[i];
      }
   }
   else
   {
      mCurrentMove.x = 0;
      mCurrentMove.y = 0;

      mCurrentMove.fire = mFiring;     // should be false?

      for(U32 i = 0; i < (U32)ShipModuleCount; i++)
      {
         mCurrentMove.modulePrimary[i] = false;
         mCurrentMove.moduleSecondary[i] = false;
      }
   }

   // Using relative controls -- all turning is done relative to the direction of the ship, so
   // we need to udate the move a little
   if(mGameSettings->getSetting<RelAbs>(IniKey::ControlMode) == Relative)
   {
      mTransformedMove = mCurrentMove;    // Copy move

      Point moveDir(mCurrentMove.x, -mCurrentMove.y);

      Point angleDir(cos(mCurrentMove.angle), sin(mCurrentMove.angle));

      Point rightAngleDir(-angleDir.y, angleDir.x);
      Point newMoveDir = angleDir * moveDir.y + rightAngleDir * moveDir.x;

      mTransformedMove.x = newMoveDir.x;
      mTransformedMove.y = newMoveDir.y;

      // Sanity checks
      mTransformedMove.x = min( 1.0f, mTransformedMove.x);
      mTransformedMove.y = min( 1.0f, mTransformedMove.y);
      mTransformedMove.x = max(-1.0f, mTransformedMove.x);
      mTransformedMove.y = max(-1.0f, mTransformedMove.y);

      move = &mTransformedMove;
   }

   // But wait! There's more!
   // Overwrite theMove if we're using joystick (also does some other essential joystick stuff)
   // We'll also run this while in the menus so if we enter keyboard mode accidentally, it won't
   // kill the joystick.  The design of combining joystick input and move updating really sucks.
   if(getGame()->getInputMode() == InputModeJoystick || getUIManager()->isCurrentUI<OptionsMenuUserInterface>())
      joystickUpdateMove(getGame(), mGameSettings, move);

   return move;
}


void GameUserInterface::resetLevelInfoDisplayTimer()
{
   if(!mLevelInfoDisplayer.isActive())
      mLevelInfoDisplayer.onActivated();

   mLevelInfoDisplayer.resetDisplayTimer();
}


// Constructor
GameUserInterface::VoiceRecorder::VoiceRecorder(ClientGame *game)
{
   mRecordingAudio = false;
   mMaxAudioSample = 0;
   mMaxForGain = 0;
   mVoiceEncoder = new SpeexVoiceEncoder;

   mGame = game;

   mWantToStopRecordingAudio = false;
}


GameUserInterface::VoiceRecorder::~VoiceRecorder()
{
   stopNow();
}


void GameUserInterface::VoiceRecorder::idle(U32 timeDelta)
{
   if(mRecordingAudio)
   {
      if(mVoiceAudioTimer.update(timeDelta))
      {
         mVoiceAudioTimer.reset(VoiceAudioSampleTime);
         process();
      }
   }
}


void GameUserInterface::VoiceRecorder::render() const
{
   if(mRecordingAudio)
   {
      F32 amt = mMaxAudioSample / F32(0x7FFF);
      U32 totalLineCount = 50;

      // Render low/high volume lines
      mGL->glColor(Colors::white);
      F32 vertices[] = {
            10.0f,                        130.0f,
            10.0f,                        145.0f,
            F32(10 + totalLineCount * 2), 130.0f,
            F32(10 + totalLineCount * 2), 145.0f
      };
      mGL->renderVertexArray(vertices, ARRAYSIZE(vertices)/2, GLOPT::Lines);

      F32 halfway = totalLineCount * 0.5f;
      F32 full = amt * totalLineCount;

      // Total items possible is totalLineCount (50)
      static F32 colorArray[400];   // 2 * 4 color components per item
      static F32 vertexArray[200];  // 2 * 2 vertex components per item

      // Render recording volume
      for(U32 i = 1; i < full; i++)  // start at 1 to not show
      {
         if(i < halfway)
         {
            colorArray[8*(i-1)]     = i / halfway;
            colorArray[(8*(i-1))+1] = 1;
            colorArray[(8*(i-1))+2] = 0;
            colorArray[(8*(i-1))+3] = 1;
            colorArray[(8*(i-1))+4] = i / halfway;
            colorArray[(8*(i-1))+5] = 1;
            colorArray[(8*(i-1))+6] = 0;
            colorArray[(8*(i-1))+7] = 1;
         }
         else
         {
            colorArray[8*(i-1)]     = 1;
            colorArray[(8*(i-1))+1] = 1 - (i - halfway) / halfway;
            colorArray[(8*(i-1))+2] = 0;
            colorArray[(8*(i-1))+3] = 1;
            colorArray[(8*(i-1))+4] = 1;
            colorArray[(8*(i-1))+5] = 1 - (i - halfway) / halfway;
            colorArray[(8*(i-1))+6] = 0;
            colorArray[(8*(i-1))+7] = 1;
         }

         vertexArray[4*(i-1)]     = F32(10 + i * 2);
         vertexArray[(4*(i-1))+1] = F32(130);
         vertexArray[(4*(i-1))+2] = F32(10 + i * 2);
         vertexArray[(4*(i-1))+3] = F32(145);
      }

      mGL->renderColorVertexArray(vertexArray, colorArray, S32(full*2), GLOPT::Lines);
   }
}


void GameUserInterface::VoiceRecorder::start()
{
   if(!(mGame->getConnectionToServer() && mGame->getConnectionToServer()->mVoiceChatEnabled))
   {
      mGame->displayErrorMessage("!!! Voice chat not allowed on this server");
      return;
   }

   mWantToStopRecordingAudio = 0; // linux repeadedly sends key-up / key-down when only holding key down (that was in GLUT, may )
   if(!mRecordingAudio)
   {
      mRecordingAudio = SoundSystem::startRecording();
      if(!mRecordingAudio)
         return;

      mUnusedAudio = new ByteBuffer(0);
      mRecordingAudio = true;
      mMaxAudioSample = 0;
      mVoiceAudioTimer.reset(FirstVoiceAudioSampleTime);

      // trim the start of the capture buffer:
      SoundSystem::captureSamples(mUnusedAudio);
      mUnusedAudio->resize(0);
   }
}

void GameUserInterface::VoiceRecorder::stopNow()
{
   if(mRecordingAudio)
   {
      process();

      mRecordingAudio = false;
      SoundSystem::stopRecording();
      mVoiceSfx = NULL;
      mUnusedAudio = NULL;
   }
}
void GameUserInterface::VoiceRecorder::stop()
{
   if(mWantToStopRecordingAudio == 0)
      mWantToStopRecordingAudio = 2;
}


void GameUserInterface::VoiceRecorder::process()
{
   if(!(mGame->getConnectionToServer() && mGame->getConnectionToServer()->mVoiceChatEnabled))
      stop();

   if(mWantToStopRecordingAudio != 0)
   {
      mWantToStopRecordingAudio--;
      if(mWantToStopRecordingAudio == 0)
      {
         stopNow();
         return;
      }
   }
   U32 preSampleCount = mUnusedAudio->getBufferSize() / 2;
   SoundSystem::captureSamples(mUnusedAudio);

   U32 sampleCount = mUnusedAudio->getBufferSize() / 2;
   if(sampleCount == preSampleCount)
      return;

   S16 *samplePtr = (S16 *) mUnusedAudio->getBuffer();
   mMaxAudioSample = 0;

   for(U32 i = preSampleCount; i < sampleCount; i++)
   {
      if(samplePtr[i] > mMaxAudioSample)
         mMaxAudioSample = samplePtr[i];
      else if(-samplePtr[i] > mMaxAudioSample)
         mMaxAudioSample = -samplePtr[i];
   }
   mMaxForGain = U32(mMaxForGain * 0.95f);
   S32 boostedMax = mMaxAudioSample + 2048;

   if(boostedMax > mMaxForGain)
      mMaxForGain = boostedMax;

   if(mMaxForGain > MaxDetectionThreshold)
   {
      // Apply some gain to the buffer:
      F32 gain = 0x7FFF / F32(mMaxForGain);
      for(U32 i = preSampleCount; i < sampleCount; i++)
      {
         F32 sample = gain * samplePtr[i];
         if(sample > 0x7FFF)
            samplePtr[i] = 0x7FFF;
         else if(sample < -0x7FFF)
            samplePtr[i] = -0x7FFF;
         else
            samplePtr[i] = S16(sample);
      }
      mMaxAudioSample = U32(mMaxAudioSample * gain);
   }

   ByteBufferPtr sendBuffer = mVoiceEncoder->compressBuffer(mUnusedAudio);

   if(sendBuffer.isValid())
   {
      GameType *gameType = mGame->getGameType();

      if(gameType && sendBuffer->getBufferSize() < 1024)      // Don't try to send too big
         gameType->c2sVoiceChat(mGame->getSettings()->getSetting<YesNo>(IniKey::VoiceEcho), sendBuffer);
   }
}


#ifdef USE_DUMMY_PLAYER_SCORES

S32 getDummyTeamCount() { return 2; }     // Teams
S32 getDummyMaxPlayers() { return 5; }    // Players per team

// Create a set of fake player scores for testing the scoreboard -- fill scores
void getDummyPlayerScores(ClientGame *game, Vector<ClientInfo *> &scores)
{
   ClientInfo *clientInfo;

   S32 teams = getDummyTeamCount();

   for(S32 i = 0; i < getDummyMaxPlayers(); i++)
   {
      string name = "PlayerName-" + itos(i);

      clientInfo = new RemoteClientInfo(game, name, false, 0, ((i+1) % 4) > 0, i, i % 3, ClientInfo::ClientRole(i % 4), false, false);

      clientInfo->setScore(i * 3);
      clientInfo->setAuthenticated((i % 2), 0, (i % 3) > 0);
      clientInfo->setPing(100 * i + 10);
      clientInfo->setTeamIndex(i % teams);

      scores.push_back(clientInfo);
   }
}
#endif


static const char *botSymbol = "B";
static const char *levelChangerSymbol = "+";
static const char *adminSymbol = "@";

static void renderScoreboardLegend(S32 humans, U32 scoreboardTop, U32 totalHeight)
{
   const S32 LegendSize = 12;     
   const S32 LegendGap  =  3;    // Space between scoreboard and legend
   const S32 legendPos  = scoreboardTop + totalHeight + LegendGap + LegendSize;

   // Create a standard legend; only need to swap out the Humans count, which is the first chunk -- this should work even if
   // there are multiple players running in the same session -- the humans count should be the same regardless!
   static Vector<SymbolShapePtr> symbols;
   static S32 lastHumans = S32_MIN;
   if(symbols.size() == 0)
   {
      string legend = " | " + string(adminSymbol) + " = Admin | " + 
                      levelChangerSymbol + " = Can Change Levels | " + botSymbol + " = Bot |";

      symbols.push_back(SymbolShapePtr());    // Placeholder, will be replaced with humans count below
      symbols.push_back(SymbolShapePtr(new SymbolText(legend, LegendSize, ScoreboardContext, &Colors::standardPlayerNameColor)));
      symbols.push_back(SymbolShapePtr(new SymbolText(" Idle Player", LegendSize, ScoreboardContext, &Colors::idlePlayerNameColor)));
      symbols.push_back(SymbolShapePtr(new SymbolText(" | ", LegendSize, ScoreboardContext, &Colors::standardPlayerNameColor)));
      symbols.push_back(SymbolShapePtr(new SymbolText("Player on Rampage", LegendSize, ScoreboardContext, &Colors::streakPlayerNameColor)));
   }

   // Rebuild the humans symbol, if the number of humans has changed
   if(humans != lastHumans)
   {
      const string humanStr = itos(humans) + " Human" + (humans != 1 ? "s" : "");
      symbols[0] = SymbolShapePtr(new SymbolText(humanStr, LegendSize, ScoreboardContext, &Colors::standardPlayerNameColor));
      lastHumans = humans;
   }

   UI::SymbolString symbolString(symbols);
   symbolString.render(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, legendPos, AlignmentCenter);
}


// Horiz offsets from the right for rendering score components
static const S32 ScoreOff = 160;    // Solo game only
static const S32 KdOff   = 85;
static const S32 PingOff = 60;
static const U32 Gap = 3;        // Small gap for use between various UI elements
static const S32 ColHeaderTextSize = 10;


void GameUserInterface::renderPlayerSymbolAndSetColor(ClientInfo *player, S32 x, S32 y, S32 size)
{
   // Figure out how much room we need to leave for our player symbol (@, +, etc.)
   x -= RenderUtils::getStringWidth(size, adminSymbol) + Gap;  // Use admin symbol as it's the widest

   // Draw the player's experience level before we set the color
   FontManager::pushFontContext(OldSkoolContext);
   static const S32 levelSize = 7;
   mGL->glColor(Colors::green);
   RenderUtils::drawStringf(x - 8, y + 7 , levelSize, "%d", ClientGame::getExpLevel(player->getGamesPlayed()));
   FontManager::popFontContext();

   // Figure out what color to use to render player name, and set it
   if(player->isSpawnDelayed())
      mGL->glColor(Colors::idlePlayerNameColor);
   else if(player->getKillStreak() >= UserInterface::StreakingThreshold)
      mGL->glColor(Colors::streakPlayerNameColor);
   else
      mGL->glColor(Colors::standardPlayerNameColor);

   // Mark of the bot
   if(player->isRobot())
      RenderUtils::drawString(x, y, size, botSymbol);

   // Admin mark
   else if(player->isAdmin())
      RenderUtils::drawString(x, y, size, adminSymbol);

   // Level changer mark
   else if(player->isLevelChanger())
      RenderUtils::drawString(x, y, size, levelChangerSymbol);
}


enum ColIndex {
   KdIndex,
   PingIndex,
   ScoreIndex,
   ColIndexCount
};


S32 getMaxPlayersOnAnyTeam(ClientGame *clientGame, S32 teams, bool isTeamGame)
{
   S32 maxTeamPlayers = 0;

   // Check to make sure at least one team has at least one player...
   for(S32 i = 0; i < teams; i++)
   {
      Team *team = (Team *)clientGame->getTeam(i);
      S32 teamPlayers = team->getPlayerBotCount();

      if(!isTeamGame)
         maxTeamPlayers += teamPlayers;

      else if(teamPlayers > maxTeamPlayers)
         maxTeamPlayers = teamPlayers;
   }

   return maxTeamPlayers;
}


void GameUserInterface::renderScoreboard() const
{
   ClientGame *clientGame = getGame();
   GameType *gameType = clientGame->getGameType();

   const bool isTeamGame = gameType->isTeamGame();

#ifdef USE_DUMMY_PLAYER_SCORES
   S32 teams = isTeamGame ? getDummyTeamCount() : 1;
   S32 maxTeamPlayers = getDummyMaxPlayers();
#else
   clientGame->countTeamPlayers();
   const S32 teams = isTeamGame ? clientGame->getTeamCount() : 1;
   S32 maxTeamPlayers = getMaxPlayersOnAnyTeam(clientGame, teams, isTeamGame);
#endif

   if(maxTeamPlayers == 0)
      return;

   static const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();
   static const S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();

   const S32 teamHeaderHeight = isTeamGame ? 40 : 2;

   const S32 numTeamRows = (teams + 1) >> 1;

   const S32 desiredHeight = (canvasHeight - vertMargin * 2) / numTeamRows;
   const S32 lineHeight    = MIN(30, (desiredHeight - teamHeaderHeight) / maxTeamPlayers);

   const S32 sectionHeight = teamHeaderHeight + (lineHeight * maxTeamPlayers) + (2 * Gap) + 10;
   const S32 totalHeight   = sectionHeight * numTeamRows - 10  + (isTeamGame ? 0 : 4);    // 4 provides a gap btwn bottom name and legend

   const S32 scoreboardTop = (canvasHeight - totalHeight) / 2;    // Center vertically

   const S32 winStatus = clientGame->getTeamBasedGameWinner().first;
   bool hasWinner = winStatus == HasWinner;
   bool isWinningTeam;

   // Outer scoreboard box
   RenderUtils::drawFilledFancyBox(horizMargin - Gap, scoreboardTop - (2 * Gap),
                     (canvasWidth - horizMargin) + Gap, scoreboardTop + totalHeight + 23,
                     13, Colors::black, 0.85f, Colors::blue);

   FontManager::pushFontContext(ScoreboardContext);
   
   for(S32 i = 0; i < teams; i++)
   {
      if(clientGame->isGameOver() && hasWinner && i == clientGame->getTeamBasedGameWinner().second)
         isWinningTeam = true;
      else
         isWinningTeam = false;
      
      renderTeamScoreboard(i, teams, isTeamGame, isWinningTeam, scoreboardTop, sectionHeight, teamHeaderHeight, lineHeight);
   }

   renderScoreboardLegend(clientGame->getPlayerCount(), scoreboardTop, totalHeight);

   FontManager::popFontContext();
}


void GameUserInterface::renderTeamScoreboard(S32 index, S32 teams, bool isTeamGame, bool isWinningTeam,
                                             S32 scoreboardTop, S32 sectionHeight, S32 teamHeaderHeight, S32 lineHeight) const
{
   static const S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();

   static const S32 drawableWidth = canvasWidth - horizMargin * 2;

   const S32 columnCount = min(teams, 2);
   const S32 teamWidth = drawableWidth / columnCount;

   const S32 xl = horizMargin + Gap + (index & 1) * teamWidth;    // Left edge of team render area
   const S32 xr = (xl + teamWidth) - (2 * Gap);                   // Right edge of team render area
   const S32 yt = scoreboardTop + (index >> 1) * sectionHeight;   // Top edge of team render area

   // Team header
   if (isTeamGame)
      renderTeamName(index, isWinningTeam, xl, xr, yt);

   // Now for player scores.  First build a list.  Then sort it.  Then display it.
   Vector<ClientInfo *> playerScores;

#ifdef USE_DUMMY_PLAYER_SCORES      // For testing purposes only!
   getDummyPlayerScores(getGame(), playerScores);
#else
   getGame()->getGameType()->getSortedPlayerScores(index, playerScores);     // Fills playerScores for team index
#endif

   S32 curRowY = yt + teamHeaderHeight + 1;                          // Advance y coord to below team display, if there is one

   const S32 x = xl + 40;                                            // + 40 to align with team name in team game
   const S32 colHeaderYPos = isTeamGame ? curRowY + 3 : curRowY + 8; // Calc this before we change curRowY

   // Leave a gap for the colHeader... not sure yet of the exact xpos... will figure that out and render in this slot later
   if(playerScores.size() > 0)
   {
      const S32 colHeaderHeight = isTeamGame ? ColHeaderTextSize - 3: ColHeaderTextSize + 2;
      curRowY += colHeaderHeight;
   }

   S32 colIndexWidths[ColIndexCount];     
   S32 maxColIndexWidths[ColIndexCount] = {0};     // Inits every element of array to 0

   for(S32 i = 0; i < playerScores.size(); i++)
   {
      renderScoreboardLine(playerScores, isTeamGame, i, x, curRowY, lineHeight, xr, colIndexWidths);
      curRowY += lineHeight;

      for(S32 j = 0; j < ColIndexCount; j++)
         maxColIndexWidths[j] = max(colIndexWidths[j], maxColIndexWidths[j]);
   }

   // Go back and render the column headers, now that we know the widths.  These will be different for team and solo games.
   if(playerScores.size() > 0)
      renderScoreboardColumnHeaders(x, xr, colHeaderYPos, maxColIndexWidths, isTeamGame);

#ifdef USE_DUMMY_PLAYER_SCORES
   playerScores.deleteAndClear();      // Clean up
#endif

}


void GameUserInterface::renderTeamName(S32 index, bool isWinningTeam, S32 left, S32 right, S32 top) const
{
   static const S32 teamFontSize = 24;

   // First the box
   const Color &teamColor = getGame()->getTeamColor(index);
   const Color &borderColor = isWinningTeam ? Colors::white : teamColor;
   const S32 headerBoxHeight = teamFontSize + 2 * Gap;

   RenderUtils::drawFilledFancyBox(left, top, right, top + headerBoxHeight, 10, teamColor, 0.6f, borderColor);

   // Then the team name & score
   FontManager::pushFontContext(ScoreboardHeadlineContext);
   mGL->glColor(Colors::white);

   RenderUtils::drawString (left  + 40,  top + 2, teamFontSize, getGame()->getTeamName(index).getString());
   RenderUtils::drawStringf(right - 140, top + 2, teamFontSize, "%d", ((Team *)(getGame()->getTeam(index)))->getScore());

   FontManager::popFontContext();
}


void GameUserInterface::renderScoreboardColumnHeaders(S32 leftEdge, S32 rightEdge, S32 y, 
                                                      const S32 *colIndexWidths, bool isTeamGame) const
{
   mGL->glColor(Colors::gray50);

   RenderUtils::drawString_fixed(leftEdge,                                                 y, ColHeaderTextSize, "Name");
   RenderUtils::drawStringc     (rightEdge -  (KdOff   + colIndexWidths[KdIndex]    / 2),  y, ColHeaderTextSize, "Threat Level");
   RenderUtils::drawStringc     (rightEdge -  (PingOff - colIndexWidths[PingIndex]  / 2),  y, ColHeaderTextSize, "Ping");

   // Solo games need one more header
   if(!isTeamGame)
      RenderUtils::drawStringc   (rightEdge - (ScoreOff + colIndexWidths[ScoreIndex] / 2), y, ColHeaderTextSize, "Score");
}


// Renders a line on the scoreboard, and returns the widths of the rendered items in colWidths
void GameUserInterface::renderScoreboardLine(const Vector<ClientInfo *> &playerScores, bool isTeamGame, S32 row,
                                             S32 x, S32 y, U32 lineHeight, S32 rightEdge, S32 *colWidths) const
{
   const S32 playerFontSize = S32(lineHeight * 0.75f);
   const S32 symbolFontSize = S32(lineHeight * 0.75f * 0.75f);

   static const S32 vertAdjustFact = (playerFontSize - symbolFontSize) / 2 - 1;

   renderPlayerSymbolAndSetColor(playerScores[row], x, y + vertAdjustFact + 2, symbolFontSize);

   S32 nameWidth = RenderUtils::drawStringAndGetWidth(x, y, playerFontSize, playerScores[row]->getName().getString());

   colWidths[KdIndex]   = RenderUtils::drawStringfr          (rightEdge - KdOff,   y, playerFontSize, "%2.2f", playerScores[row]->getRating());
   colWidths[PingIndex] = RenderUtils::drawStringAndGetWidthf(rightEdge - PingOff, y, playerFontSize, "%d",    playerScores[row]->getPing());

   if(!isTeamGame)
      colWidths[ScoreIndex] = RenderUtils::drawStringfr(rightEdge - ScoreOff, y, playerFontSize, "%d", playerScores[row]->getScore());

   // Vertical scale ratio to maximum line height
   const F32 scaleRatio = (F32)lineHeight / 30.f;

   // Circle back and render the badges now that all the rendering with the name color is finished
   renderBadges(playerScores[row], x + nameWidth + 10 + Gap, y + (lineHeight / 2), scaleRatio);
}


// Static method
void GameUserInterface::renderBadges(ClientInfo *clientInfo, S32 x, S32 y, F32 scaleRatio)
{
   // Default to vector font for badges
   FontManager::pushFontContext(OldSkoolContext);

   F32 badgeRadius = 10.f * scaleRatio;
   S32 badgeOffset = S32(2 * badgeRadius) + 5;
   F32 badgeBackgroundEdgeSize = 2 * badgeRadius + 2.f;

   bool hasBBBBadge = false;

   for(S32 i = 0; i < BADGE_COUNT; i++)
   {
      MeritBadges badge = MeritBadges(i);    // C++ enums can be rather tedious...

      if(clientInfo->hasBadge(badge))
      {
         // Test for BBB badges.  We're only going to show the most valued one
         if(badge == BADGE_BBB_GOLD || badge == BADGE_BBB_SILVER || badge == BADGE_BBB_BRONZE || badge == BADGE_BBB_PARTICIPATION)
         {
            // If we've already got one, don't draw this badge.  This assumes the value of the badges decrease
            // with each iteration
            if(hasBBBBadge)
               continue;

            hasBBBBadge = true;
         }

         // Draw badge border
         mGL->glColor(Colors::gray20);
         RenderUtils::drawRoundedRect(Point(x,y), badgeBackgroundEdgeSize, badgeBackgroundEdgeSize, 3.f);

         GameObjectRender::renderBadge((F32)x, (F32)y, badgeRadius, badge);
         x += badgeOffset;
      }
   }

   FontManager::popFontContext();
}


void GameUserInterface::renderBasicInterfaceOverlay() const
{
   GameType *gameType = getGame()->getGameType();

   // Progress meter for file upload and download
   if(getGame()->getConnectionToServer())
   {
      F32 progress = getGame()->getConnectionToServer()->getFileProgressMeter();
      if(progress != 0)
      {
         mGL->glColor(Colors::yellow);
         RenderUtils::drawRect(25.f, 200.f, progress * (DisplayManager::getScreenInfo()->getGameCanvasWidth()-50) + 25.f, 210.f, GLOPT::TriangleFan);
         RenderUtils::drawRect(25, 200, DisplayManager::getScreenInfo()->getGameCanvasWidth()-25, 210, GLOPT::LineLoop);
      }
   }
   
   if(mInputModeChangeAlertDisplayTimer.getCurrent() != 0)
      renderInputModeChangeAlert();

   bool showScore = scoreboardIsVisible();

   if(showScore && getGame()->getTeamCount() > 0)      // How could teamCount be 0?
      renderScoreboard();
   
   // Render timer and associated doodads in the lower-right corner (includes teams-locked indicator)
   mTimeLeftRenderer.render(gameType, showScore, getGame()->areTeamsLocked(), true);
   
   renderTalkingClients();
   renderDebugStatus();
}


bool GameUserInterface::shouldRenderLevelInfo() const 
{
   return mLevelInfoDisplayer.isActive() || mMissionOverlayActive;
}


void GameUserInterface::renderLevelInfo() const
{
   // Level Info requires gametype.  It can be NULL when switching levels
   if(getGame()->getGameType() == NULL)
      return;

   if(shouldRenderLevelInfo())
      mLevelInfoDisplayer.render();
}


// Display alert about input mode changing
void GameUserInterface::renderInputModeChangeAlert() const
{
   F32 alpha = 1;

   if(mInputModeChangeAlertDisplayTimer.getCurrent() < 1000)
      alpha = mInputModeChangeAlertDisplayTimer.getCurrent() * 0.001f;

   mGL->glColor(Colors::paleRed, alpha);
   RenderUtils::drawCenteredStringf(vertMargin + 130, 20, "Input mode changed to %s",
                       getGame()->getInputMode() == InputModeJoystick ? "Joystick" : "Keyboard");
}


void GameUserInterface::renderTalkingClients() const
{
   S32 y = 150;

   for(S32 i = 0; i < getGame()->getClientCount(); i++)
   {
      ClientInfo *client = ((Game *)getGame())->getClientInfo(i);

      if(client->getVoiceSFX()->isPlaying())
      {
         const S32 TEXT_HEIGHT = 20;

         mGL->glColor( getGame()->getTeamColor(client->getTeamIndex()) );
         RenderUtils::drawString(10, y, TEXT_HEIGHT, client->getName().getString());
         y += TEXT_HEIGHT + 5;
      }
   }
}


void GameUserInterface::renderDebugStatus() const
{
   // When bots are frozen, render large pause icon in lower left
   if(EventManager::get()->isPaused())
   {
      mGL->glColor(Colors::white);

      const S32 PAUSE_HEIGHT = 30;
      const S32 PAUSE_WIDTH = 10;
      const S32 PAUSE_GAP = 6;
      const S32 BOX_INSET = 5;

      const S32 TEXT_SIZE = 15;
      const char *TEXT = "STEP: Alt-], Ctrl-]";

      S32 x, y;

      // Draw box
      x = DisplayManager::getScreenInfo()->getGameCanvasWidth() - horizMargin - 2 * (PAUSE_WIDTH + PAUSE_GAP) - BOX_INSET - RenderUtils::getStringWidth(TEXT_SIZE, TEXT);
      y = vertMargin + PAUSE_HEIGHT;

      // Draw Pause symbol
      RenderUtils::drawFilledRect(x, y, x + PAUSE_WIDTH, y - PAUSE_HEIGHT, Colors::black, Colors::white);

      x += PAUSE_WIDTH + PAUSE_GAP;
      RenderUtils::drawFilledRect(x, y, x + PAUSE_WIDTH, y - PAUSE_HEIGHT, Colors::black, Colors::white);

      x += PAUSE_WIDTH + PAUSE_GAP + BOX_INSET;

      y -= TEXT_SIZE + (PAUSE_HEIGHT - TEXT_SIZE) / 2 + 1;
      RenderUtils::drawString(x, y, TEXT_SIZE, TEXT);
   }
}


// Show server-side object ids... using illegal reachover to obtain them!
void GameUserInterface::renderObjectIds() const
{
   TNLAssert(getGame()->isTestServer(), "Will crash on non server!");
   if(getGame()->isTestServer())
      return;

   const Vector<DatabaseObject *> *objects = Game::getServerLevel()->findObjects_fast();

   for(S32 i = 0; i < objects->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objects->get(i));
      static const S32 height = 13;

      // ForceFields don't have a geometry.  When I gave them one, they just rendered the ID at the
      // exact same location as their owning projector - so we'll just skip them
      if(obj->getObjectTypeNumber() == ForceFieldTypeNumber)
         continue;

      S32 id = obj->getUserAssignedId();
      S32 width = RenderUtils::getStringWidthf(height, "[%d]", id);

      F32 x = obj->getPos().x;
      F32 y = obj->getPos().y;

      mGL->glColor(Colors::black);
      RenderUtils::drawFilledRect(x - 1, y - 1, x + width + 1, y + height + 1);

      mGL->glColor(Colors::gray70);
      RenderUtils::drawStringf(x, y, height, "[%d]", id);
   }
}


//void GameUserInterface::saveAlreadySeenLevelupMessageList()
//{
//   mGameSettings->setSetting("LevelupItemsAlreadySeenList",
//                                                                getAlreadySeenLevelupMessageString());
//}


//void GameUserInterface::loadAlreadySeenLevelupMessageList()
//{
//   setAlreadySeenLevelupMessageString(
//         mGameSettings->getSetting<string>("LevelupItemsAlreadySeenList")
//   );
//}


const string GameUserInterface::getAlreadySeenLevelupMessageString() const
{
   return IniSettings::bitArrayToIniString(mAlreadySeenLevelupMsg, UserSettings::LevelCount);
}


// Takes a string; we'll mark a message as being seen every time we encounter a 'Y'
void GameUserInterface::setAlreadySeenLevelupMessageString(const string &vals)
{
   IniSettings::iniStringToBitArray(vals, mAlreadySeenLevelupMsg, UserSettings::LevelCount);
}


void GameUserInterface::onChatMessageReceived(const Color &msgColor, const char *format, ...)
{
   // Ignore empty message
   if(!strcmp(format, ""))
      return;

   static char buffer[MAX_CHAT_MSG_LENGTH];

   va_list args;

   va_start(args, format);
   vsnprintf(buffer, sizeof(buffer), format, args);
   va_end(args);

   mChatMessageDisplayer1.onChatMessageReceived(msgColor, buffer);      // Standard chat stream
   mChatMessageDisplayer2.onChatMessageReceived(msgColor, buffer);      // Short, non-expiring chat stream
   mChatMessageDisplayer3.onChatMessageReceived(msgColor, buffer);      // Long, non-expiring chat stream
}


// Set which chat message display mode we're in (Ctrl-M)
void GameUserInterface::toggleChatDisplayMode()
{
   S32 m = mMessageDisplayMode + 1;

   if(m >= MessageDisplayModes)
      m = 0;

   mMessageDisplayMode = MessageDisplayMode(m);
}


// Return message being composed in in-game chat
const char *GameUserInterface::getChatMessage()
{
   return mHelperManager.getChatMessage();
}


// Some reusable containers --> will probably need to become non-static if we have more than one clientGame active
static Point screenSize, visSize, visExt;
static Vector<DatabaseObject *> rawRenderObjects;
static Vector<BfObject *> renderObjects;
static Vector<BotNavMeshZone *> renderZones;


static void fillRenderZones()
{
   renderZones.clear();
   for(S32 i = 0; i < rawRenderObjects.size(); i++)
      renderZones.push_back(static_cast<BotNavMeshZone *>(rawRenderObjects[i]));
}


// Fills renderZones for drawing botNavMeshZones
static void populateRenderZones(ClientGame *game, const Rect *extentRect = NULL)
{
   rawRenderObjects.clear();

   if(extentRect)
      game->getBotZoneDatabase().findObjects(BotNavMeshZoneTypeNumber, rawRenderObjects, *extentRect);
   else
      game->getBotZoneDatabase().findObjects(BotNavMeshZoneTypeNumber, rawRenderObjects);

   fillRenderZones();
}


static void renderBotPaths(ClientGame *game, Vector<BfObject *> &renderObjects)
{
   ServerGame *serverGame = game->getServerGame();

   if(serverGame)
      for(S32 i = 0; i < serverGame->getBotCount(); i++)
         renderObjects.push_back(serverGame->getBot(i));
}


static S32 QSORT_CALLBACK renderSortCompare(BfObject **a, BfObject **b)
{
   return (*a)->getRenderSortValue() - (*b)->getRenderSortValue();
}


// Note: With the exception of renderCommander, this function cannot be called if ship is NULL.  If it is never
// called with a NULL ship from renderCommander in practice, we can get rid of the caching of lastRenderPos (which will
// fail here if we ever have more than one UIGame instance.  If the following assert never trips, we can get rid of the 
// cached value, and perhaps the whole function itself.
Point GameUserInterface::getShipRenderPos() const
{
   static Point lastRenderPos;

   Ship *ship = getGame()->getLocalPlayerShip();

   TNLAssert(ship, "Expected a valid ship here!");    // <== see comment above!

   if(ship)
      lastRenderPos = ship->getRenderPos();

   return lastRenderPos;
}


void GameUserInterface::renderGameNormal() const
{
   // Start of the level, we only show progress bar
   if(mShowProgressBar)
      return;

   // Here we determine if we have a control ship.
   // If not (like after we've been killed), we'll still render the current position and things
   Ship *ship = getGame()->getLocalPlayerShip();

   if(!ship)     // If we don't know where the ship is, we can't render in this mode
      return;

   visExt = getGame()->computePlayerVisArea(ship);

   mGL->glPushMatrix();

   static const Point center(DisplayManager::getScreenInfo()->getGameCanvasWidth()  / 2,
                             DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2);

   mGL->glTranslate(center);       // Put (0,0) at the center of the screen

   // These scaling factors are different when changing the visible area by equiping the sensor module
   mGL->glScale(center.x / visExt.x, center.y / visExt.y);
   mGL->glTranslate(getShipRenderPos() * -1);

   GameObjectRender::renderStars(mStars, mStarColors, NumStars, 1.0, getShipRenderPos(), visExt * 2);

   // Render all the objects the player can see
   screenSize.set(visExt);
   Rect extentRect(getShipRenderPos() - screenSize, getShipRenderPos() + screenSize);

   // Fill rawRenderObjects with anything within extentRect (our visibility extent)
   rawRenderObjects.clear();
   getGame()->getLevel()->findObjects((TestFunc)isAnyObjectType, rawRenderObjects, extentRect);    

   // Cast objects in rawRenderObjects and put them in renderObjects
   renderObjects.clear();
   for(S32 i = 0; i < rawRenderObjects.size(); i++)
      renderObjects.push_back(static_cast<BfObject *>(rawRenderObjects[i]));

   // Normally a big no-no, we'll access the server's bot zones directly if we are running locally 
   // so we can visualize them without bogging the game down with the normal process of transmitting 
   // zones from server to client.  The result is that we can only see zones on our local server.
   if(mDebugShowMeshZones)
      populateRenderZones(getGame(), &extentRect);

   if(mShowDebugBots)
      renderBotPaths(getGame(), renderObjects);

   renderObjects.sort(renderSortCompare);

   // Render in three passes, to ensure some objects are drawn above others
   for(S32 i = -1; i < 2; i++)
   {
      if(mDebugShowMeshZones)
         for(S32 j = 0; j < renderZones.size(); j++)
            renderZones[j]->renderLayer(i);

      for(S32 j = 0; j < renderObjects.size(); j++)
         renderObjects[j]->renderLayer(i);

      Barrier::renderEdges(mGameSettings, i);    // Render wall edges

      mFxManager.render(i, getCommanderZoomFraction(), getShipRenderPos());
   }

   S32 team = NONE;
   if(getGame()->getLocalRemoteClientInfo())
      team = getGame()->getLocalRemoteClientInfo()->getTeamIndex();
   renderInlineHelpItemOutlines(team, getBackgroundTextDimFactor(false));

   FxTrail::renderTrails();

   getUIManager()->getUI<GameUserInterface>()->renderEngineeredItemDeploymentMarker(ship);

   // Again, we'll be accessing the server's data directly so we can see server-side item ids directly on the client.  Again,
   // the result is that we can only see zones on our local server.
   if(mDebugShowObjectIds)
      renderObjectIds();

   mGL->glPopMatrix();

   // Render current ship's energy
   if(ship)
   {
      UI::EnergyGaugeRenderer::render(ship->mEnergy);   
      UI::HealthGaugeRenderer::render(ship->mHealth);
   }

   // Render any screen-linked special effects, outside the matrix transformations
   mFxManager.renderScreenEffects();


   //renderOverlayMap();     // Draw a floating overlay map
}


void GameUserInterface::renderInlineHelpItemOutlines(S32 playerTeam, F32 alpha) const
{
   if(!HelpItemManager::shouldRender(getGame()))
      return;

   // Render a highlight/outline around any objects in our highlight type list, for help
   static Vector<const Vector<Point> *> polygons;
   polygons.clear();

   const Vector<HighlightItem> *itemsToHighlight = mHelpItemManager.getItemsToHighlight();      

   for(S32 i = 0; i < itemsToHighlight->size(); i++)
      for(S32 j = 0; j < renderObjects.size(); j++)
         if(itemsToHighlight->get(i).type == renderObjects[j]->getObjectTypeNumber() && 
                                             renderObjects[j]->shouldRender())
         {
            HighlightItem::Whose whose = itemsToHighlight->get(i).whose;

            S32 team = renderObjects[j]->getTeam();

            if( whose == HighlightItem::Any ||
               (whose == HighlightItem::Team && team == playerTeam) ||
               (whose == HighlightItem::TorNeut && (team == playerTeam || team == TEAM_NEUTRAL)) ||
               (whose == HighlightItem::Enemy && ((team >= 0 && team != playerTeam) || team == TEAM_HOSTILE)) ||
               (whose == HighlightItem::Neutral && team == TEAM_NEUTRAL) ||
               (whose == HighlightItem::Hostile && team == TEAM_HOSTILE) )

               polygons.push_back(renderObjects[j]->getOutline());
         }

#ifdef TNL_DEBUG
   if(getGame()->showAllObjectOutlines())
   {
      static Vector<U8> itemTypes;     // List of all items that are highlighted by our help system

      // Lazily initialize list
      if(itemTypes.size() == 0)
      {
#define HELP_TABLE_ITEM(a, itemType, c, d, e, f, g) \
         if(itemType != UnknownTypeNumber) \
            itemTypes.push_back(itemType);
         HELP_ITEM_TABLE
#undef HELP_TABLE_ITEM
      }

      fillVector.clear();
      getGame()->getLevel()->findObjects(itemTypes, fillVector, *getGame()->getWorldExtents());
      polygons.clear();
      for(S32 i = 0; i < fillVector.size(); i++)
         if(static_cast<BfObject *>(fillVector[i])->shouldRender())
            polygons.push_back(fillVector[i]->getOutline());
   }
#endif

   if(polygons.size() > 0)
   {
      Vector<Vector<Point> > outlines;

      offsetPolygons(polygons, outlines, HIGHLIGHTED_OBJECT_BUFFER_WIDTH);

      for(S32 j = 0; j < outlines.size(); j++)
         GameObjectRender::renderPolygonOutline(&outlines[j], Colors::green, alpha);
   }
}


void GameUserInterface::renderGameCommander() const
{
   // Start of the level, we only show progress bar
   if(mShowProgressBar)
      return;

   const S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   GameType *gameType = getGame()->getGameType();
   
   static Point worldExtents;    // Reuse this point to avoid construction/destruction cost
   worldExtents = mDispWorldExtents.getExtents();

   worldExtents.x *= canvasWidth  / F32(canvasWidth  - 2 * horizMargin);
   worldExtents.y *= canvasHeight / F32(canvasHeight - 2 * vertMargin);

   F32 aspectRatio = worldExtents.x / worldExtents.y;
   F32 screenAspectRatio = F32(canvasWidth) / F32(canvasHeight);

   if(aspectRatio > screenAspectRatio)
      worldExtents.y *= aspectRatio / screenAspectRatio;
   else
      worldExtents.x *= screenAspectRatio / aspectRatio;

   Ship *ship = getGame()->getLocalPlayerShip();

   visSize = ship ? getGame()->computePlayerVisArea(ship) * 2 : worldExtents;


   mGL->glPushMatrix();

   // Put (0,0) at the center of the screen
   mGL->glTranslate(DisplayManager::getScreenInfo()->getGameCanvasWidth() * 0.5f,
               DisplayManager::getScreenInfo()->getGameCanvasHeight() * 0.5f);    

   F32 zoomFrac = getCommanderZoomFraction();

   Point modVisSize = (worldExtents - visSize) * zoomFrac + visSize;
   mGL->glScale(canvasWidth / modVisSize.x, canvasHeight / modVisSize.y);

   Point offset = (mDispWorldExtents.getCenter() - getShipRenderPos()) * zoomFrac + getShipRenderPos();
   mGL->glTranslate(-offset.x, -offset.y);

   // zoomFrac == 1.0 when fully zoomed out to cmdr's map
   GameObjectRender::renderStars(mStars, mStarColors, NumStars, 1 - zoomFrac, offset, modVisSize);

   // Render the objects.  Start by putting all command-map-visible objects into renderObjects.  Note that this no longer captures
   // walls -- those will be rendered separately.
   rawRenderObjects.clear();

   if(ship && ship->hasModule(ModuleSensor))
      getGame()->getLevel()->findObjects((TestFunc)isVisibleOnCmdrsMapWithSensorType, rawRenderObjects);
   else
      getGame()->getLevel()->findObjects((TestFunc)isVisibleOnCmdrsMapType, rawRenderObjects);

   renderObjects.clear();

   // Copy rawRenderObjects into renderObjects
   for(S32 i = 0; i < rawRenderObjects.size(); i++)
      renderObjects.push_back(static_cast<BfObject *>(rawRenderObjects[i]));

   // Add extra bots if we're showing them
   if(mShowDebugBots)
      renderBotPaths(getGame(), renderObjects);

   // If we're drawing bot zones, get them now (put them in the renderZones vector)
   if(mDebugShowMeshZones)
      populateRenderZones(getGame());

   if(ship)
   {
      // Get info about the current player
      S32 playerTeam = -1;

      if(gameType)
      {
         playerTeam = ship->getTeam();
         const Color &teamColor = ship->getColor();

         for(S32 i = 0; i < renderObjects.size(); i++)
         {
            // Render ship visibility range, and that of our teammates
            if(isShipType(renderObjects[i]->getObjectTypeNumber()))
            {
               Ship *otherShip = static_cast<Ship *>(renderObjects[i]);

               // Get team of this object
               S32 otherShipTeam = otherShip->getTeam();
               if((otherShipTeam == playerTeam && gameType->isTeamGame()) || otherShip == ship)  // On our team (in team game) || the ship is us
               {
                  Point p = otherShip->getRenderPos();
                  Point visExt = getGame()->computePlayerVisArea(otherShip);

                  mGL->glColor(teamColor * zoomFrac * 0.35f);
                  RenderUtils::drawFilledRect(p.x - visExt.x, p.y - visExt.y, p.x + visExt.x, p.y + visExt.y);
               }
            }
         }

         const Vector<DatabaseObject *> *spyBugs = getGame()->getLevel()->findObjects_fast(SpyBugTypeNumber);

         // Render spy bug visibility range second, so ranges appear above ship scanner range
         for(S32 i = 0; i < spyBugs->size(); i++)
         {
            SpyBug *sb = static_cast<SpyBug *>(spyBugs->get(i));

            if(sb->isVisibleToPlayer(playerTeam, gameType->isTeamGame()))
            {
               GameObjectRender::renderSpyBugVisibleRange(sb->getRenderPos(), teamColor);
               mGL->glColor(teamColor * 0.8f);     // Draw a marker in the middle
               RenderUtils::drawCircle(sb->getRenderPos(), 2);
            }
         }
      }
   }

   // Now render the objects themselves
   renderObjects.sort(renderSortCompare);

   if(mDebugShowMeshZones)
      for(S32 i = 0; i < renderZones.size(); i++)
         renderZones[i]->renderLayer(0);

   // First pass
   for(S32 i = 0; i < renderObjects.size(); i++)
      renderObjects[i]->renderLayer(0);

   // Second pass
   Barrier::renderEdges(mGameSettings, 1);    // Render wall edges

   if(mDebugShowMeshZones)
      for(S32 i = 0; i < renderZones.size(); i++)
         renderZones[i]->renderLayer(1);

   for(S32 i = 0; i < renderObjects.size(); i++)
   {
      // Keep our spy bugs from showing up on enemy commander maps, even if they're known
 //     if(!(renderObjects[i]->getObjectTypeMask() & SpyBugType && playerTeam != renderObjects[i]->getTeam()))
         renderObjects[i]->renderLayer(1);
   }

   getUIManager()->getUI<GameUserInterface>()->renderEngineeredItemDeploymentMarker(ship);

   mGL->glPopMatrix();


   // Render current ship's energy
   if(ship)
   {
      UI::EnergyGaugeRenderer::render(ship->mEnergy);
      UI::HealthGaugeRenderer::render(ship->mHealth);
   }

   // Render any screen-linked special effects, outside the matrix transformations
   mFxManager.renderScreenEffects();
}



////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
// This is a test of a partial map overlay to assist in navigation
// still needs work, and early indications are that it is not
// a beneficial addition to the game.

//void GameUserInterface::renderOverlayMap()
//{
//   const S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
//   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();
//
//   Ship *ship = getShip(game->getConnectionToServer());
//
//   Point position = ship->getRenderPos();
//
//   S32 mapWidth = canvasWidth / 4;
//   S32 mapHeight = canvasHeight / 4;
//   S32 mapX = UserInterface::horizMargin;        // This may need to the the UL corner, rather than the LL one
//   S32 mapY = canvasHeight - UserInterface::vertMargin - mapHeight;
//   F32 mapScale = 0.1f;
//
//   F32 vertices[] = {
//         mapX, mapY,
//         mapX, mapY + mapHeight,
//         mapX + mapWidth, mapY + mapHeight,
//         mapX + mapWidth, mapY
//   };
//   mGL->renderVertexArray(vertices, 4, GLOPT::LineLoop);
//
//
//   mGL->glEnable(GL_SCISSOR_BOX);                    // Crop to overlay map display area
//   mGL->glScissor(mapX, mapY + mapHeight, mapWidth, mapHeight);  // Set cropping window
//
//   mGL->glPushMatrix();   // Set scaling and positioning of the overlay
//
//   glTranslate(mapX + mapWidth / 2.f, mapY + mapHeight / 2.f);          // Move map off to the corner
//   glScale(mapScale);                                     // Scale map
//   glTranslate(-position.x, -position.y);                           // Put ship at the center of our overlay map area
//
//   // Render the objects.  Start by putting all command-map-visible objects into renderObjects
//   Rect mapBounds(position, position);
//   mapBounds.expand(Point(mapWidth * 2, mapHeight * 2));      //TODO: Fix
//
//   rawRenderObjects.clear();
//   if(/*ship->isModulePrimaryActive(ModuleSensor)*/true)
//      mGameObjDatabase->findObjects((TestFunc)isVisibleOnCmdrsMapWithSensorType, rawRenderObjects);
//   else
//      mGameObjDatabase->findObjects((TestFunc)isVisibleOnCmdrsMapType, rawRenderObjects);
//
//   renderObjects.clear();
//   for(S32 i = 0; i < rawRenderObjects.size(); i++)
//      renderObjects.push_back(static_cast<BfObject *>(rawRenderObjects[i]));
//
//
//   renderObjects.sort(renderSortCompare);
//
//   for(S32 i = 0; i < renderObjects.size(); i++)
//      renderObjects[i]->render(0);
//
//   for(S32 i = 0; i < renderObjects.size(); i++)
//      // Keep our spy bugs from showing up on enemy commander maps, even if they're known
// //     if(!(renderObjects[i]->getObjectTypeMask() & SpyBugType && playerTeam != renderObjects[i]->getTeam()))
//         renderObjects[i]->render(1);
//
//   mGL->glPopMatrix();
//   mGL->glDisable(GL_SCISSOR_BOX);     // Stop cropping
//}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////


void GameUserInterface::renderSuspended() const
{
   mGL->glColor(Colors::yellow);
   S32 textHeight = 20;
   S32 textGap = 5;
   S32 ypos = DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2 - 3 * (textHeight + textGap);

   RenderUtils::drawCenteredString(ypos, textHeight, "==> Game is currently suspended, waiting for other players <==");
   ypos += textHeight + textGap;
   RenderUtils::drawCenteredString(ypos, textHeight, "When another player joins, the game will start automatically.");
   ypos += textHeight + textGap;
   RenderUtils::drawCenteredString(ypos, textHeight, "When the game restarts, the level will be reset.");
   ypos += 2 * (textHeight + textGap);
   RenderUtils::drawCenteredString(ypos, textHeight, "Press <SPACE> to resume playing now");
}


////////////////////////////////////////
////////////////////////////////////////

void ColorString::set(const string &s, const Color &c, U32 id)    // id defaults to 0
{
   str = s;
   color = c;
   groupId = id;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ChatMessageDisplayer::ChatMessageDisplayer(ClientGame *game, S32 msgCount, bool expire, bool topDown, S32 wrapWidth, S32 fontSize, S32 fontWidth)
{
   mDisplayChatMessageTimer.setPeriod(5000);    // How long messages stay visible (ms)
   mChatScrollTimer.setPeriod(100);             // Transition time when new msg arrives (ms) 

   mMessages.resize(msgCount + 1);              // Have an extra message for scrolling effect.  Will only display msgCount messages.

   reset();

   mGame      = game;
   mExpire    = expire;
   mTopDown   = topDown;
   mWrapWidth = wrapWidth;
   mFontSize  = fontSize;
   mFontGap   = fontWidth;
   
   mNextGroupId = 0;
}

// Destructor
ChatMessageDisplayer::~ChatMessageDisplayer()
{
   // Do nothing
}


// Effectivley clears all messages
void ChatMessageDisplayer::reset()
{
   mFirst = mLast = 0;
   mFull = false;
}


void ChatMessageDisplayer::idle(U32 timeDelta)
{
   mChatScrollTimer.update(timeDelta);

   // Clear out any expired messages
   if(mExpire && mDisplayChatMessageTimer.update(timeDelta))
   {
      mDisplayChatMessageTimer.reset();

      if(mFirst > mLast)
      {
         if(mTopDown)
            mChatScrollTimer.reset();

         advanceLast();         
      }
   }
}


// Make room for a new message at the head of our list
void ChatMessageDisplayer::advanceFirst()
{
   mFirst++;

   if(mLast % mMessages.size() == mFirst % mMessages.size())
   {
      mLast++;
      mFull = true;
   }
}


// Clear out messages from the back of our list; expire all messages with same id together.
void ChatMessageDisplayer::advanceLast()
{
   mLast++;

   U32 id = mMessages[mLast % mMessages.size()].groupId;

   while(mMessages[(mLast + 1) % mMessages.size()].groupId == id && mFirst > mLast)
      mLast++;

   mFull = false;

   TNLAssert(mLast <= mFirst, "index error! -- add check to correct this!");
}


// Replace %vars% in chat messages 
// Currently only evaluates names of keybindings (as used in the INI file), and %playerName%
// Vars are case insensitive
static string getSubstVarVal(ClientGame *game, const string &var)
{
   // %keybinding%
   InputCode inputCode = game->getSettings()->getInputCodeManager()->getKeyBoundToBindingCodeName(var);
   if(inputCode != KEY_UNKNOWN)
      return string("[") + InputCodeManager::inputCodeToString(inputCode) + "]";
   
   // %playerName%
   if(caseInsensitiveStringCompare(var, "playerName"))
      return game->getClientInfo()->getName().getString();

   // Not a variable... preserve formatting
   return "%" + var + "%";
}


// Add it to the list, will be displayed in render()
void ChatMessageDisplayer::onChatMessageReceived(const Color &msgColor, const string &msg)
{
   FontManager::pushFontContext(ChatMessageContext);
   Vector<string> lines = wrapString(substitueVars(msg), mWrapWidth, mFontSize, "      ");
   FontManager::popFontContext();

   // All lines from this message will share a groupId.  We'll use that to expire the group as a whole.
   for(S32 i = 0; i < lines.size(); i++)
   {
      advanceFirst();
      mMessages[mFirst % mMessages.size()].set(lines[i], msgColor, mNextGroupId); 
   }

   mNextGroupId++;

   // When displaying messages from the top of the screen, the animation happens when we expire messages
   mDisplayChatMessageTimer.reset();

   if(!mTopDown)
      mChatScrollTimer.reset();
}


// Check if we have any %variables% that need substituting
string ChatMessageDisplayer::substitueVars(const string &str)
{
   string s = str;      // Make working copy

   bool inside = false;

   std::size_t startPos, endPos;

   inside = false;

   for(std::size_t i = 0; i < s.length(); i++)
   {
      if(s[i] == '%')
      {
         if(!inside)    // Found beginning of variable
         {
            startPos = i + 1;
            inside = true;
         }
         else           // Found end of variable
         {
            endPos = i - startPos;
            inside = false;

            string var = s.substr(startPos, endPos);
            string val = getSubstVarVal(mGame, var);

            s.replace(startPos - 1, endPos + 2, val);

            i += val.length() - var.length() - 2;     // Make sure we don't evaluate the contents of val; i.e. no recursion
         }
      }
   }

   return s;
}


// Render any incoming player chat msgs
void ChatMessageDisplayer::render(S32 anchorPos, bool helperVisible, bool anouncementActive, F32 alpha) const
{
   // Are we in the act of transitioning between one message and another?
   bool isScrolling = (mChatScrollTimer.getCurrent() > 0);  

   // Check if there any messages to display... if not, bail
   if(mFirst == mLast && !(mTopDown && isScrolling))
      return;

   S32 lineHeight = mFontSize + mFontGap;


   // Reuse this to avoid startup and breakdown costs
   static ScissorsManager scissorsManager;

   // Only need to set scissors if we're scrolling.  When not scrolling, we control the display by only showing
   // the specified number of lines; there are normally no partial lines that need vertical clipping as 
   // there are when we're scrolling.  Note also that we only clip vertically, and can ignore the horizontal.
   if(isScrolling)    
   {
      // Remember that our message list contains an extra entry that exists only for scrolling purposes.
      // We want the height of the clip window to omit this line, so we subtract 1 below.  
      S32 displayAreaHeight = (mMessages.size() - 1) * lineHeight;     
      S32 displayAreaYPos = anchorPos + (mTopDown ? displayAreaHeight : lineHeight);

      scissorsManager.enable(true, mGame->getSettings()->getSetting<DisplayMode>(IniKey::WindowMode), 0, 
                             F32(displayAreaYPos - displayAreaHeight), F32(DisplayManager::getScreenInfo()->getGameCanvasWidth()), 
                             F32(displayAreaHeight));
   }

   // Initialize the starting rendering position.  This represents the bottom of the message rendering area, and
   // we'll work our way up as we go.  In all cases, newest messages will appear on the bottom, older ones on top.
   // Note that anchorPos reflects something different (i.e. the top or the bottom of the area) in each case.
   S32 y = anchorPos + S32(mChatScrollTimer.getFraction() * lineHeight);

   // Advance anchor from top to the bottom of the render area.  When we are rendering at the bottom, anchorPos
   // already represents the bottom, so no additional adjustment is necessary.
   if(mTopDown)
      y += (mFirst - mLast - 1) * lineHeight;

   // Render an extra message while we're scrolling (in some cases).  Scissors will control the total vertical height.
   S32 renderExtra = 0;
   if(isScrolling)
   {
      if(mTopDown)
         renderExtra = 1;
      else if(mFull)    // Only render extra item on bottom-up if list is fully occupied
         renderExtra = 1;
   }

   // Adjust our last line if we have an announcement
   U32 last = mLast;
   if(anouncementActive)
   {
      // Render one less line if we're past the size threshold for this displayer
      if(mFirst >= (U32)mMessages.size() - 1)
         last++;

      y -= lineHeight;
   }

   FontManager::pushFontContext(ChatMessageContext);

   // Draw message lines
   for(U32 i = mFirst; i != last - renderExtra; i--)
   {
      U32 index = i % (U32)mMessages.size();    // Handle wrapping in our message list

      mGL->glColor(mMessages[index].color, alpha);

      RenderUtils::drawString(UserInterface::horizMargin, y, mFontSize, mMessages[index].str.c_str());

      y -= lineHeight;
   }

   FontManager::popFontContext();

   // Restore scissors settings -- only used during scrolling
   scissorsManager.disable();
}


////////////////////////////////////////
////////////////////////////////////////


LevelListDisplayer::LevelListDisplayer()
{
   mLevelLoadDisplayFadeTimer.setPeriod(1000);
   mLevelLoadDisplay = true;
   mLevelLoadDisplayTotal = 0;
}


void LevelListDisplayer::idle(U32 timeDelta)
{
   if(mLevelLoadDisplayFadeTimer.update(timeDelta))
      clearLevelLoadDisplay();
}


// Shows the list of levels loaded when hosting a game
// If we want the list to fade out, pass true for fade, or pass false to make it disapear instantly
// fade param has no effect when show is true
void LevelListDisplayer::showLevelLoadDisplay(bool show, bool fade)
{
   mLevelLoadDisplay = show;

   if(!show)
   {
      if(fade)
         mLevelLoadDisplayFadeTimer.reset();
      else
         mLevelLoadDisplayFadeTimer.clear();
   }
}


void LevelListDisplayer::clearLevelLoadDisplay()
{
   mLevelLoadDisplayNames.clear();
   mLevelLoadDisplayTotal = 0;
}


void LevelListDisplayer::render() const
{
   if(mLevelLoadDisplay || mLevelLoadDisplayFadeTimer.getCurrent() > 0)
   {
      for(S32 i = 0; i < mLevelLoadDisplayNames.size(); i++)
      {
         mGL->glColor(Colors::white, (1.4f - ((F32) (mLevelLoadDisplayNames.size() - i) / 10.f)) *
                                        (mLevelLoadDisplay ? 1 : mLevelLoadDisplayFadeTimer.getFraction()) );
         RenderUtils::drawStringf(100, DisplayManager::getScreenInfo()->getGameCanvasHeight() - /*vertMargin*/ 0 - (mLevelLoadDisplayNames.size() - i) * 20,
                     15, "%s", mLevelLoadDisplayNames[i].c_str());
      }
   }
}



void LevelListDisplayer::addLevelName(const string &levelName)
{
   render();
   addProgressListItem("Loaded level " + levelName + "...");
}


// Add bit of text to progress item, and manage the list
void LevelListDisplayer::addProgressListItem(string item)
{
   static const S32 MaxItems = 15;

   mLevelLoadDisplayNames.push_back(item);

   mLevelLoadDisplayTotal++;

   // Keep the list from growing too long:
   if(mLevelLoadDisplayNames.size() > MaxItems)
      mLevelLoadDisplayNames.erase(0);
}


};

