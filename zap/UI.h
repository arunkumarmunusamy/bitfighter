//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _UI_H_
#define _UI_H_

#ifdef ZAP_DEDICATED
#  error "UI.h shouldn't be included in dedicated build"
#endif

#include "RenderManager.h"
#include "lineEditor.h"
#include "InputCode.h"
#include "SymbolShape.h"

#include "Timer.h"
#include "tnl.h"
#include "tnlLog.h"

#include <string>

using namespace TNL;
using namespace std;

namespace Zap
{

using namespace UI;


static const F32 HIGHLIGHTED_OBJECT_BUFFER_WIDTH = 14.0;      // Width to buffer objects higlighted by inline help system

////////////////////////////////////////
////////////////////////////////////////

class Game;
class ClientGame;
class GameSettings;
class UIManager;
class Color;
class MasterServerConnection;

class UserInterface: public RenderManager
{
   friend class UIManager;    // Give UIManager access to private and protected members

private:
   ClientGame *mClientGame;
   UIManager *mUiManager;

   U32 mTimeSinceLastInput;

   static void doDrawAngleString(F32 x, F32 y, F32 size, F32 angle, const char *string, bool autoLineWidth = true);
   static void doDrawAngleString(S32 x, S32 y, F32 size, F32 angle, const char *string, bool autoLineWidth = true);

protected:
   GameSettings *mGameSettings;
   bool mDisableShipKeyboardInput;                 // Disable ship movement while user is in menus

public:
   explicit UserInterface(ClientGame *game, UIManager *uiManager);   // Constructor
   virtual ~UserInterface();                                         // Destructor

   static const S32 MOUSE_SCROLL_INTERVAL = 100;
   static const S32 MAX_PASSWORD_LENGTH = 32;      // Arbitrary, doesn't matter, but needs to be _something_

   static const S32 MaxServerNameLen = 40;
   static const S32 MaxServerDescrLen = 254;

   static const U32 StreakingThreshold = 5;        // This many kills in a row makes you a streaker!

   ClientGame *getGame() const;

   UIManager *getUIManager() const;

#ifdef TNL_OS_XBOX
   static const S32 horizMargin = 50;
   static const S32 vertMargin = 38;
#else
   static const S32 horizMargin = 15;
   static const S32 vertMargin = 15;
#endif

   static S32 messageMargin;

   U32 getTimeSinceLastInput();

   // Activate menus via the UIManager, please!
   void activate();
   void reactivate();

   virtual void render() const;
   virtual void idle(U32 timeDelta);
   virtual void onActivate();
   virtual void onDeactivate(bool nextUIUsesEditorScreenMode);
   virtual void onReactivate();
   virtual void onDisplayModeChange();

   virtual bool usesEditorScreenMode() const;   // Returns true if the UI attempts to use entire screen like editor, false otherwise

   void renderConsole()const;             // Render game console
   virtual void renderMasterStatus(const MasterServerConnection *connectionToMaster) const;

   // Helpers to simplify dealing with key bindings
   static InputCode getInputCode(GameSettings *settings, BindingNameEnum binding);
   string getEditorBindingString (EditorBindingNameEnum  binding);
   string getSpecialBindingString(SpecialBindingNameEnum binding);

   void setInputCode(BindingNameEnum binding, InputCode inputCode);
   bool checkInputCode(BindingNameEnum, InputCode inputCode);
   const char *getInputCodeString(BindingNameEnum binding) const;

   void setUiManager(UIManager *uiManager);

   // Input event handlers
   virtual bool onKeyDown(InputCode inputCode);
   virtual void onKeyUp(InputCode inputCode);
   virtual void onTextInput(char ascii);
   virtual void onMouseMoved();
   virtual void onMouseDragged();

   // Old school -- deprecated
   void renderMessageBox(const string &title, const string &instr, const string &message, S32 vertOffset = 0, S32 style = 1) const;

   // New school
   void renderMessageBox(const SymbolShapePtr &title, const SymbolShapePtr &instr, const SymbolShapePtr *message, S32 msgLines, S32 vertOffset = 0, S32 style = 1) const;

   static void renderCenteredFancyBox(S32 boxTop, S32 boxHeight, S32 inset, S32 cornerInset, const Color &fillColor, F32 fillAlpha, const Color &borderColor);

   static void dimUnderlyingUI(F32 amount = 0.75f);

   static void renderDiagnosticKeysOverlay();

   static void drawMenuItemHighlight(S32 x1, S32 y1, S32 x2, S32 y2, bool disabled = false);
   static void playBoop();    // Make some noise!

   virtual void onColorPicked(const Color &color);
};


////////////////////////////////////////
////////////////////////////////////////

// Used only for multiple mClientGame in one instance
struct UserInterfaceData
{
   S32 vertMargin, horizMargin;
   S32 chatMargin;
};

};

#endif


