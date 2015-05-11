//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _QUICK_MENU_UI_H
#define _QUICK_MENU_UI_H


#include "UIMenus.h"    // Parent class

namespace Zap
{

// This class is now a container for various attribute editing menus; these are rendered differently than regular menus, and
// have other special attributes.  This class has been refactored such that it can be used directly, and no longer needs to be
// subclassed for each type of entity we want to edit attributes for.

class QuickMenuUI : public MenuUserInterface    // There's really nothing quick about it!
{
   typedef MenuUserInterface Parent;

private:
   virtual void initialize();
   virtual string getTitle() const;
   S32 getMenuWidth() const;     
   Point mMenuLocation;

   S32 getYStart() const;

   virtual S32 getTextSize(MenuItemSize size) const;     // Let menus set their own text size
   virtual S32 getGap(MenuItemSize size) const;          // Gap is the space between items

protected:
   bool mDisableHighlight;   // Disable highlighting of selected menu item

   virtual S32 getSelectedMenuItem();
   bool usesEditorScreenMode() const;

public:
   // Constructors
   explicit QuickMenuUI(ClientGame *game, UIManager *uiManager);
   QuickMenuUI(ClientGame *game, UIManager *uiManager, const string &title);
   virtual ~QuickMenuUI();

   void render() const;

   virtual void onEscape();

   void addSaveAndQuitMenuItem();
   void addSaveAndQuitMenuItem(const char *menuText, const char *helpText);
   void setMenuCenterPoint(const Point &location);    // Sets the point at which the menu will be centered about
   virtual void doneEditing() = 0;

   void cleanupAndQuit();                             // Delete our menu items and reactivate the underlying UI

   void onDisplayModeChange();
};


////////////////////////////////////////
////////////////////////////////////////

class EditorAttributeMenuUI : public QuickMenuUI
{
   typedef QuickMenuUI Parent;
      
private:
   string getTitle() const;

public:
   explicit EditorAttributeMenuUI(ClientGame *game, UIManager *uiManager);    // Constructor
   virtual ~EditorAttributeMenuUI();                    // Destructor

   virtual bool startEditingAttrs(BfObject *object);
   virtual void doneEditing();
   virtual void doneEditingAttrs(BfObject *object);
};


////////////////////////////////////////
////////////////////////////////////////

class PluginMenuUI : public QuickMenuUI
{
   typedef QuickMenuUI Parent;

public:
   PluginMenuUI(ClientGame *game, UIManager *uiManager, const string &title); // Constructor
   virtual ~PluginMenuUI();                                                   // Destructor

   void setTitle(const string &title);
   virtual void doneEditing();
};


////////////////////////////////////////
////////////////////////////////////////

class SimpleTextEntryMenuUI : public QuickMenuUI
{
   typedef QuickMenuUI Parent;

private:
   S32 mData;          // See SimpleTextEntryType in UIEditor.h

public:
   SimpleTextEntryMenuUI(ClientGame *game, UIManager *uiManager, const string &title, S32 data);   // Constructor
   virtual ~SimpleTextEntryMenuUI();                                                               // Destructor

   virtual void doneEditing();
};



}  // namespace

#endif
