//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _UITEAMDEFMENU_H_
#define _UITEAMDEFMENU_H_

#include "game.h"    // For Game::MAX_TEAMS
#include "UI.h"
#include "InputModeEnum.h"
#include "ConfigEnum.h"
#include "Color.h"
#include "Timer.h"
#include "SymbolShape.h"

namespace Zap
{

using namespace std;


class TeamDefUserInterface : public UserInterface
{
   typedef UserInterface Parent;

private:
   Timer errorMsgTimer;
   string errorMsg;

   UI::SymbolStringSet mMenuSubTitle;

   UI::SymbolString    mTopInstructions;

   UI::SymbolString    mBottomInstructions1;
   UI::SymbolString    mBottomInstructions2;
   UI::SymbolString    mBottomInstructions3a;
   UI::SymbolString    mBottomInstructions3b;
   UI::SymbolString    mBottomInstructions4;
   
   S32 selectedIndex;          // Highlighted menu item

   bool mEditingName;         
   bool mEditingColor;

   ColorEntryMode mColorEntryMode;

   LineEditor mHexColorEditors[Game::MAX_TEAMS];
   LineEditor mTeamNameEditors[Game::MAX_TEAMS];

   F32 getColorBase() const;
   F32 getAmount() const;
   void doneEditingColor();
   void cancelEditing();
   void startEditing();
   void resetEditors();

   void updateAllHexEditors();

   Level *getLevel();
   const Level *getConstLevel() const;

   const char *getEntryMessage() const;

   bool onKeyDown_editingName(InputCode inputCode);
   bool onKeyDown_editingColor(InputCode inputCode);

   void addTeamsFromPresets(Level *level, S32 count);
   void setTeamFromPreset(Level *level, S32 teamIndex, S32 preset);

public:
   explicit TeamDefUserInterface(ClientGame *game, UIManager *uiManager);     // Constructor
   virtual ~TeamDefUserInterface();

   const char *mMenuTitle;
   const char *mMenuFooter;

   void render() const;              // Draw the menu
   void idle(U32 timeDelta);
   bool onKeyDown(InputCode inputCode);
   void onTextInput(char ascii);
   void onMouseMoved();

   void onActivate();
   void onEscape();
   void onColorPicked(const Color &color);
};

};

#endif

