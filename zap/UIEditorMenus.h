//-----------------------------------------------------------------------------------
//
// Bitfighter - A multiplayer vector graphics space game
// Based on Zap demo released for Torque Network Library by GarageGames.com
//
// Derivative work copyright (C) 2008-2009 Chris Eykamp
// Original work copyright (C) 2004 GarageGames.com, Inc.
// Other code copyright as noted
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful (and fun!),
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//------------------------------------------------------------------------------------

#ifndef _UIEDITORMENUS_H_
#define _UIEDITORMENUS_H_


#include "UIMenus.h"
#include "UIEditor.h"      // For EditorObject

namespace Zap
{

class EditorObject;

// This class is now a container for various attribute editing menus; these are rendered differently than regular menus, and
// have other special attributes.  This class has been refactored such that it can be used directly, and no longer needs to be
// subclassed for each type of entity we want to edit attributes for.

class EditorAttributeMenuUI : public MenuUserInterface
{
   typedef MenuUserInterface Parent;
      
protected:
   EditorObject *mObject;      // Object whose attributes are being edited

public:
   EditorAttributeMenuUI(ClientGame *game) : Parent(game) { /* Do nothing */ }    // Constructor
   EditorObject *getObject() { return mObject; }
   void render();
   void onEscape();

   S32 getMenuWidth();

   virtual void startEditingAttrs(EditorObject *object);
   virtual void doneEditingAttrs();
   virtual void doneEditingAttrs(EditorObject *object);

   void addSaveAndQuitMenuItem();
};


};

#endif