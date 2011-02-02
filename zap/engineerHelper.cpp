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

#include "engineerHelper.h"
#include "engineeredObjects.h"      // For EngineerModuleDeployer
#include "UIGame.h"
#include "point.h"      // For Color def

namespace Zap
{

// Constructor
EngineerHelper::EngineerHelper()
{
   mEngineerCostructionItemInfos.push_back(EngineerConstructionItemInfo(EngineeredTurret, "Turret", KEY_1, BUTTON_1));
   mEngineerCostructionItemInfos.push_back(EngineerConstructionItemInfo(EngineeredForceField, "Force Field", KEY_2, BUTTON_2));
}

void EngineerHelper::onMenuShow()
{
   mSelectedItem = -1;
}


extern void drawSquare(const Point &pos, S32 size);
static Point deployPosition, deployNormal;
extern void glColor(const Color &c, float alpha = 1.0);

void EngineerHelper::render()
{
   S32 yPos = MENU_TOP;
   const S32 fontSize = 15;
   const Color engineerMenuHeaderColor (1, 0, 0);

   if(mSelectedItem == -1)    // Haven't selected an item yet
   {
      const S32 xPos = UserInterface::horizMargin + 50;

      drawMenuBorderLine(yPos, engineerMenuHeaderColor);

      glColor(engineerMenuHeaderColor);
      UserInterface::drawString(UserInterface::horizMargin, yPos, fontSize, "What do you want to Engineer?");
      yPos += fontSize + 10;

      bool showKeys = gIniSettings.showKeyboardKeys || gIniSettings.inputMode == Keyboard;

      for(S32 i = 0; i < mEngineerCostructionItemInfos.size(); i++)
      {
         // Draw key controls for selecting the object to be created

         if(gIniSettings.inputMode == Joystick)     // Only draw joystick buttons when in joystick mode
            renderControllerButton(UserInterface::horizMargin + (showKeys ? 0 : 20), yPos, mEngineerCostructionItemInfos[i].mButton, false);

         if(showKeys)
         {
            glColor3f(1, 1, 1);     // Render key in white
            renderControllerButton(UserInterface::horizMargin + 20, yPos, mEngineerCostructionItemInfos[i].mKey, false);
         }

         glColor3f(0.1, 1.0, 0.1);     
         S32 x = UserInterface::drawStringAndGetWidth(xPos, yPos, fontSize, mEngineerCostructionItemInfos[i].mName); 

         glColor3f(.2, .8, .8);    
         UserInterface::drawString(xPos + x, yPos, fontSize, mEngineerCostructionItemInfos[i].mHelp);      // The help string, if there is one

         yPos += fontSize + 7;
      }

      yPos += 2;

      drawMenuBorderLine(yPos - fontSize - 2, engineerMenuHeaderColor);
      yPos += 8;
      drawMenuCancelText(yPos, engineerMenuHeaderColor, fontSize);
   }
   else     // Have selected a module, need to indicate where to deploy
   {
      S32 xPos = UserInterface::horizMargin;
      glColor3f(0, 1, 0);
      UserInterface::drawStringf(xPos, yPos, fontSize, "Placing %s.", mEngineerCostructionItemInfos[mSelectedItem].mName);
      yPos += fontSize + 7;
      UserInterface::drawString(xPos, yPos, fontSize, "Aim at a spot on the wall, and activate the module again.");
   }
}


// Return true if key did something, false if key had no effect
// Runs on client
bool EngineerHelper::processKeyCode(KeyCode keyCode)
{
   if(Parent::processKeyCode(keyCode))    // Check for cancel keys
      return true;

   if(mSelectedItem == -1)    // Haven't selected an item yet
   {
      for(S32 i = 0; i < mEngineerCostructionItemInfos.size(); i++)
         if(keyCode == mEngineerCostructionItemInfos[i].mKey || keyCode == mEngineerCostructionItemInfos[i].mButton)
         {
            mSelectedItem = i;
            return true;
         }
      Ship *ship = dynamic_cast<Ship *>(gClientGame->getConnectionToServer()->getControlObject());
      if(!ship || (keyCode == keyMOD1[gIniSettings.inputMode] && ship->getModule(0) == ModuleEngineer)
         || (keyCode == keyMOD2[gIniSettings.inputMode] && ship->getModule(1) == ModuleEngineer))
      {
         exitHelper();
         return true;
      }
   }
   else                       // Placing item
   {
      Ship *ship = dynamic_cast<Ship *>(gClientGame->getConnectionToServer()->getControlObject());
      if(ship && (keyCode == keyMOD1[gIniSettings.inputMode] && ship->getModule(0) == ModuleEngineer || 
                  keyCode == keyMOD2[gIniSettings.inputMode] && ship->getModule(1) == ModuleEngineer))
      {
         // Check deployment status on client; will be checked again on server, but server will only handle likely valid placements
         EngineerModuleDeployer deployer;
         
         if(deployer.canCreateObjectAtLocation(ship, mEngineerCostructionItemInfos[mSelectedItem].mObjectType))     
         {
            GameConnection *gc = gClientGame->getConnectionToServer();
            if(gc)
               gc->c2sEngineerDeployObject(mEngineerCostructionItemInfos[mSelectedItem].mObjectType);
         }
         else
            gGameUserInterface.displayErrorMessage(deployer.getErrorMessage().c_str());
            
         exitHelper();
         return true;
      }
   }

   return false;
}


// Basically draws a red box where the ship is pointing
void EngineerHelper::renderDeploymentMarker(Ship *ship)
{
   if(mSelectedItem != -1 && EngineerModuleDeployer::findDeployPoint(ship, deployPosition, deployNormal))
   {
      glColor3f(1,0,0);  // red
      drawSquare(deployPosition, 5);
   }
}

};

