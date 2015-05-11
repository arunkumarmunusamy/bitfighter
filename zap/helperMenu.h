//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _HELPERMENU_H_
#define _HELPERMENU_H_

#include "SlideOutWidget.h"    // Parent class
#include "AToBScroller.h"      // Parent class, for internal transitions
#include "RenderManager.h"

#include "InputModeEnum.h"
#include "InputCodeEnum.h"

#include "Color.h"
#include "tnlVector.h"

using namespace TNL;
using namespace Zap::UI;

namespace Zap
{


class ClientGame;
class UIManager;
class HelperManager;


struct OverlayMenuItem
{
   InputCode key;             // Keyboard key used to select in menu
   InputCode button;          // Controller button used to select in menu
   bool showOnMenu;           // Should this item actually be added to the menu?
   U32 itemIndex;             // Only used on Loadout menu
   const char *name;          // Name used on menu
   const Color *itemColor;
   const char *help;          // An additional bit of help text, also displayed on menu
   const Color *helpColor;    // Pointer to color with which to render the item
   const Color *buttonOverrideColor;
};


struct HelperMenuLegendItem {
   HelperMenuLegendItem(const string &itemText, const Color &itemColor) : text(itemText), color(itemColor) { }
   string text;
   Color color;
};


class HelperMenu : public SlideOutWidget, public AToBScroller
{
   typedef SlideOutWidget Slider;
   typedef AToBScroller   Scroller;

public:
   enum HelperMenuType {
      ChatHelperType,
      QuickChatHelperType,
      LoadoutHelperType,
      EngineerHelperType,
      ShuffleTeamsHelperType
   };

private:
   virtual const char *getCancelMessage() const;
   virtual InputCode getActivationKey();

   ClientGame *mClientGame;
   HelperManager *mHelperManager;

   S32 mOldBottom;
   S32 mOldCount;

   S32 mHorizLabelOffset;

   // Some render helpers
   void drawMenuItems(const OverlayMenuItem *items, S32 count, S32 yPos, S32 bottom, bool newItems, S32 horizOffset) const;
   void renderPressEscapeToCancel(S32 xPos, S32 yPos, const Color &baseColor, InputMode inputMode) const;
   void renderLegend(S32 xPos, S32 yPos, const Vector<HelperMenuLegendItem> &legend) const;

   S32 getMenuHeight() const;
   S32 getMenuBottomPos() const;

   virtual bool getActivationKeyClosesHelper();

protected:
   static const S32 MENU_TOP          = 175;   // Location of top of title of overlay menu frame
   static const S32 MENU_FONT_SIZE    = 15;    // Size of standard items
   static const S32 MENU_FONT_SPACING =  7;    // Vertical gap between lines
   static const S32 MENU_PADDING      =  9;    // Padding around outer edge of overlay
   static const S32 ITEM_HELP_PADDING =  5;    // Gap between item and its help text
   static const S32 ITEM_INDENT       = 50;    // Amount individual menu items are indented to make room for keys
   static const S32 BottomPadding     = MENU_PADDING;


   // Shortcut helper function
   virtual void exitHelper();

   void drawItemMenu(S32 widthOfButtons, S32 widthOfTextBlock) const;

   // Items we are rendering
   OverlayMenuItem *mCurrentRenderItems;
   S32 mCurrentRenderCount;
                              
   const OverlayMenuItem *mPrevRenderItems;
   S32 mPrevRenderCount;

   const char *mTitle;
   Vector<HelperMenuLegendItem> *mLegend;

   ClientGame *getGame() const;

   S32 getTotalDisplayWidth  (S32 widthOfButtons, S32 widthOfTextBlock) const;
   S32 getCurrentDisplayWidth(S32 widthOfButtons, S32 widthOfTextBlock) const;
   S32 getLegendHeight() const;

   S32 getButtonWidth(const OverlayMenuItem *items, S32 itemCount) const;
   void setExpectedWidth_MidTransition(S32 width);

public:
   HelperMenu();
   virtual ~HelperMenu();

   void initialize(ClientGame *game, HelperManager *helperManager);

   virtual void render() const = 0;
   virtual void idle(U32 delta);
   virtual void onActivated();

   virtual void onWidgetClosed();                     // Gets run when closing animation is complete

   virtual bool processInputCode(InputCode inputCode);
   virtual void onTextInput(char ascii);

   virtual void activateHelp(UIManager *uiManager);   // Open help to an appropriate page
   virtual bool isMovementDisabled() const;           // Is ship movement disabled while this helper is active?
   virtual bool isChatDisabled() const;               // Returns true if chat and friends should be disabled while this is active

   S32 getMaxItemWidth(const OverlayMenuItem *items, S32 count) const;

   virtual HelperMenuType getType() = 0;


   // For testing
   static InputCode getInputCodeForOption(const OverlayMenuItem *items, S32 itemCount, U32 index, bool keyBut);  
};


};

#endif


