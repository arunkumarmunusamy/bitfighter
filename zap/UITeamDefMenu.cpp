////------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UITeamDefMenu.h"

#include "UIEditor.h"
#include "UIManager.h"

#include "ConfigEnum.h"

#include "EditorTeam.h"
#include "DisplayManager.h"
#include "ClientGame.h"
#include "Cursor.h"

#include "Colors.h"

#include "RenderUtils.h"
#include "stringUtils.h"
#include "MathUtils.h"

#include "UIColorPicker.h"

#include "FontManager.h"

#include <string>

namespace Zap
{

// Note: Do not make any of the following team names longer than MAX_TEAM_NAME_LENGTH, which is currently 32
// Note: Make sure we have at least 9 presets below...  (instructions are wired for keys 1-9)
TeamPreset TeamPresets[] = 
{
   { "Blue",      Color(  0,     0,    1 ) },
   { "Red",       Color(  1,     0,    0 ) },
   { "Yellow",    Color(  1,     1,    0 ) },
   { "Green",     Color(  0,     1,    0 ) },
   { "Pink",      Color(  1, .45f, .875f ) },
   { "Orange",    Color(  1,  .67f,    0 ) },
   { "Lilac",     Color(.79f,   .5, .96f ) },
   { "LightBlue", Color(.45f, .875f,   1 ) },
   { "Ruby",      Color(.67f,    0,    0 ) },
};


// Other ideas
//Team Black 0 0 0
//Team White 1 1 1
//Team Sapphire 0 0 0.7
//Team Emerald 0 0.7 0
//Team Lime 0.8 1 0
//Team DarkAngel 0 0.7 0.7
//Team Purple 0.7 0 0.7
//Team Peach 1 0.7 0


namespace UI
{

static SymbolString getSymbolString(const string &text, const InputCodeManager *inputCodeManager, S32 size, const Color &color)
{
   Vector<SymbolShapePtr> symbols;

   SymbolString::symbolParse(inputCodeManager, text, symbols, MenuContext, size, &color);
   return SymbolString(symbols, AlignmentCenter);
}

} // namespace UI


// Called by an assert in the constructor
static bool checkNameLengths()
{
   for(S32 i = 0; i < ARRAYSIZE(TeamPresets); i++)
      if(strlen(TeamPresets[i].name) > AbstractTeam::MAX_TEAM_NAME_LENGTH - 1)
         return false;

   return true;
}


// Constructor
TeamDefUserInterface::TeamDefUserInterface(ClientGame *game, UIManager *uiManager) :
   Parent(game, uiManager),
   mMenuSubTitle(8),
   mMenuTitle("CONFIGURE TEAMS")
{
   TNLAssert(ARRAYSIZE(TeamPresets) == Game::MAX_TEAMS, "Wrong number of presets!");
   TNLAssert(checkNameLengths(), "Team name is too long!");

   InputCodeManager *inputCodeManager = mGameSettings->getInputCodeManager();

   mTopInstructions =  getSymbolString("For quick configuration, press [[Alt+1]] - [[Alt+9]] to specify number of teams",
                                             inputCodeManager, 18, Colors::menuHelpColor);

   // Text at the bottom of the screen
   mBottomInstructions1 =  getSymbolString("[[1]] - [[9]] selects a team preset for current slot",
                                           inputCodeManager, 16, Colors::menuHelpColor);
   mBottomInstructions2 =  getSymbolString("[[Enter]] edits team name | [[C]] shows Color Picker | [[M]] changes color entry mode",
                                          inputCodeManager, 16, Colors::menuHelpColor);
   mBottomInstructions3a = getSymbolString("[[R]] [[G]] [[B]] to change preset color (with or without [[Shift]])",
                                          inputCodeManager, 16, Colors::menuHelpColor);
   mBottomInstructions3b = getSymbolString("[[H]] to edit color hex value",
                                          inputCodeManager, 16, Colors::menuHelpColor);
   mBottomInstructions4 =  getSymbolString("[[Insert]] or [[+]] to insert team | [[Del]] or [[-]] to remove selected team",
                                          inputCodeManager, 16, Colors::menuHelpColor);

   mColorEntryMode = mGameSettings->getSetting<ColorEntryMode>(IniKey::ColorEntryMode);
   mEditingColor = false;
}


// Destructor
TeamDefUserInterface::~TeamDefUserInterface()
{
   // Do nothing
}


static const U32 errorMsgDisplayTime = FOUR_SECONDS;
static const S32 fontsize = 19;
static const S32 fontgap = 12;
static const U32 yStart = UserInterface::vertMargin + 90;
static const U32 itemHeight = fontsize + 5;


Level *TeamDefUserInterface::getLevel()
{
   return getUIManager()->getUI<EditorUserInterface>()->getLevel();
}


const Level *TeamDefUserInterface::getConstLevel() const
{
   return getUIManager()->getUI<EditorUserInterface>()->getLevel();
}


void TeamDefUserInterface::onActivate()
{
   selectedIndex = 0;                        // First item selected when we begin
   mEditingName = mEditingColor = false;     // Not editing anything by default

   // Grab team names and populate our editors
   resetEditors();

   Level *level = getLevel();
   S32 teamCount = level->getTeamCount();

   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   ui->mOldTeams.resize(teamCount);  // Avoid unnecessary reallocations

   for(S32 i = 0; i < teamCount; i++)
   {
      ui->mOldTeams[i].setColor(level->getTeamColor(i));
      ui->mOldTeams[i].setName(level->getTeamName(i).getString());
   }

   // Display an intitial message to users
   errorMsgTimer.reset(errorMsgDisplayTime);
   errorMsg = "";
   Cursor::disableCursor();
}


void TeamDefUserInterface::resetEditors()
{
   Level *level = getLevel();
   S32 teamCount = level->getTeamCount();

   for(S32 i = 0; i < teamCount; i++)
      mTeamNameEditors[i].setString(level->getTeamName(i).getString());

   // Make sure hex values are correct
   if(mColorEntryMode == ColorEntryModeHex)
      updateAllHexEditors();
}


void TeamDefUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   if(errorMsgTimer.update(timeDelta))
      errorMsg = "";
}


// TODO: Clean this up a bit...  this menu was two-cols before, and some of that garbage is still here...
void TeamDefUserInterface::render() const
{
   const S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   FontManager::pushFontContext(MenuHeaderContext);
   mGL->glColor(Colors::green);
   RenderUtils::drawCenteredUnderlinedString(vertMargin, 30, mMenuTitle);
   
   RenderUtils::drawCenteredString(canvasHeight - vertMargin - 20, 18, "Arrow Keys to choose | ESC to exit");

   mGL->glColor(Colors::white);

   S32 x = canvasWidth / 2;

   mTopInstructions.render(x, 83);

   S32 y = canvasHeight - vertMargin - 116;
   S32 gap = 28;

   mBottomInstructions1.render(x, y);
   y += gap;

   mBottomInstructions2.render(x, y);
   y += gap;

   if(mColorEntryMode != ColorEntryModeHex)
      mBottomInstructions3a.render(x, y);
   else
      mBottomInstructions3b.render(x, y);
   y += gap;

   mBottomInstructions4.render(x, y);

   FontManager::popFontContext();

   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   S32 size = ui->getTeamCount();

   TNLAssert(selectedIndex < size, "Out of bounds!");

   // Draw the fixed teams
   mGL->glColor(Colors::NeutralTeamColor);
   RenderUtils::drawCenteredStringf(yStart, fontsize, "Neutral Team (can't change)");

   mGL->glColor(Colors::HostileTeamColor);
   RenderUtils::drawCenteredStringf(yStart + fontsize + fontgap, fontsize, "Hostile Team (can't change)");

   const Level *level = getConstLevel();

   for(S32 j = 0; j < size; j++)
   {
      S32 i = j + 2;    // Take account of the two fixed teams (neutral & hostile)

      U32 y = yStart + i * (fontsize + fontgap);

      if(selectedIndex == j)       // Highlight selected item
         drawMenuItemHighlight(0, y - 2, canvasWidth, y + itemHeight + 2);

      if(j < ui->getTeamCount())
      {
         string numstr = "Team " + itos(j + 1) + ": ";
         string namestr = numstr + mTeamNameEditors[j].getString();
         
         string colorstr;
         
         S32 teamCount = level->getTeamCount();

         const Color &color = level->getTeamColor(j);

         if(mColorEntryMode == ColorEntryModeHex)
            colorstr = "#" + mHexColorEditors[j].getString();
         else
         {
            F32 multiplier;

            if(mColorEntryMode == ColorEntryMode100)
               multiplier = 100;
            else if(mColorEntryMode == ColorEntryMode255)
               multiplier = 255;
            else
               TNLAssert(false, "Unknown entry mode!");

            colorstr = "(" + itos(S32(color.r * multiplier + 0.5)) + ", " + 
                             itos(S32(color.g * multiplier + 0.5)) + ", " +
                             itos(S32(color.b * multiplier + 0.5)) + ")";
         }
         
         static const string spacer1 = "  ";
         string nameColorStr = namestr + spacer1 + colorstr + " " + getEntryMessage();

         // Draw item text
         mGL->glColor(color);
         RenderUtils::drawCenteredString(y, fontsize, nameColorStr.c_str());

         // Draw cursor if we're editing
         if(j == selectedIndex)
         {
            if(mEditingName)
            {
               S32 x = RenderUtils::getCenteredStringStartingPos(fontsize, nameColorStr.c_str()) +
                       RenderUtils::getStringWidth(fontsize, numstr.c_str());

               mTeamNameEditors[j].drawCursor(x, y, fontsize);
            }
            else if(mEditingColor)
            {
               S32 x = RenderUtils::getCenteredStringStartingPos(fontsize, nameColorStr.c_str()) +
                       RenderUtils::getStringWidth(fontsize, namestr.c_str()) +
                       RenderUtils::getStringWidth(fontsize, spacer1.c_str()) +
                       RenderUtils::getStringWidth(fontsize, "#");

               mHexColorEditors[j].drawCursor(x, y, fontsize);
            }
         }
      }
   }

   if(errorMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if(errorMsgTimer.getCurrent() < (U32)ONE_SECOND)
         alpha = (F32) errorMsgTimer.getCurrent() / ONE_SECOND;

      mGL->glColor(Colors::red, alpha);
      RenderUtils::drawCenteredString(canvasHeight - vertMargin - 161, fontsize, errorMsg.c_str());
   }
}


// Run this as we're exiting the menu
void TeamDefUserInterface::onEscape()
{
   Level *level = getLevel();
   S32 teamCount = level->getTeamCount();

   // Save the names back to the mTeamInfos
   for(S32 i = 0; i < teamCount; i++)
      level->setTeamName(i, mTeamNameEditors[i].getString());

   // Make sure there is at least one team left...
   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   ui->makeSureThereIsAtLeastOneTeam();
   ui->teamsHaveChanged();

   getUIManager()->reactivatePrevUI();
}


class Team;
string origName;
Color origColor;
extern bool isPrintable(char c);


void TeamDefUserInterface::onTextInput(char ascii)
{
   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   if(mEditingName)
   {
      if(isPrintable(ascii))
         mTeamNameEditors[selectedIndex].addChar(ascii);
   }

   else if(mEditingColor)
   {
      if(isHex(ascii))
         mHexColorEditors[selectedIndex].addChar(toupper(ascii));
   }
}


bool TeamDefUserInterface::onKeyDown_editingName(InputCode inputCode)
{
   if(inputCode == KEY_ENTER)       // Finish editing
   {
      mEditingName = false;
      return true;
   }
   
   if(inputCode == KEY_TAB)         // Toggle what we're editing
   {
      mEditingName = false;
      mEditingColor = true;

      return true;
   }

   if(inputCode == KEY_ESCAPE)    // Stop editing, and restore the original value
   {
      cancelEditing();
      return true;
   }

   return mTeamNameEditors[selectedIndex].handleKey(inputCode);
}


bool TeamDefUserInterface::onKeyDown_editingColor(InputCode inputCode)
{
   if(inputCode == KEY_ENTER)          // Finish editing
   {
      doneEditingColor();
      return true;
   }

   if(inputCode == KEY_TAB)       // Toggle to edit name
   {
      doneEditingColor();
      mEditingName = true;
      return true;
   }

   if(inputCode == KEY_ESCAPE)    // Stop editing, and restore the original value
   {
      cancelEditing();
      return true;
   }

   return mHexColorEditors[selectedIndex].handleKey(inputCode);
}


bool TeamDefUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;

   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();
   Level *level = ui->getLevel();

   // If we're editing, need to send keypresses to editor
   if(mEditingName)
      return onKeyDown_editingName(inputCode);

   else if(mEditingColor)
      return onKeyDown_editingColor(inputCode);

   // Not editing, normal key processing follows

   if(inputCode == KEY_ENTER)
   {
      startEditing();
      mEditingName = true;

      return true;
   }

   if(inputCode == KEY_H)
   {
      if(mColorEntryMode != ColorEntryModeHex)
         return true;

      startEditing();
      mEditingColor = true;

      return true;
   }

   if(inputCode == KEY_DELETE || inputCode == KEY_MINUS)            // Del or Minus - Delete current team
   {
      if(ui->getTeamCount() == 1) 
      {
         errorMsgTimer.reset(errorMsgDisplayTime);
         errorMsg = "There must be at least one team";
         return true;
      }

      ui->removeTeam(selectedIndex);
      if(selectedIndex >= ui->getTeamCount())
         selectedIndex = ui->getTeamCount() - 1;

      return true;
   }
  
   if(inputCode == KEY_INSERT || inputCode == KEY_EQUALS)           // Ins or Plus (equals) - Add new item
   {
      S32 teamCount = ui->getTeamCount();

      if(teamCount >= Game::MAX_TEAMS)
      {
         errorMsgTimer.reset(errorMsgDisplayTime);
         errorMsg = "Too many teams for this interface";
         return true;
      }

      S32 presetIndex = teamCount % Game::MAX_TEAMS;

      EditorTeam *team = new EditorTeam(TeamPresets[presetIndex]);
      level->addTeam(team, teamCount);

      selectedIndex++;

      if(selectedIndex < 0)      // It can happen with too many deletes
         selectedIndex = 0;

      return true;
   }

   if(inputCode == KEY_R)
   {
      if(mColorEntryMode != ColorEntryModeHex)
      {
         Level *level = getLevel();

         Color color = level->getTeamColor(selectedIndex);
         color.r = CLAMP(color.r + getAmount(), 0, 1);
         level->setTeamColor(selectedIndex, color); 
      }

      return true;
   }

   if(inputCode == KEY_G)
   {
      if(mColorEntryMode != ColorEntryModeHex)
      {
         Level *level = getLevel();

         Color color = level->getTeamColor(selectedIndex);
         color.g = CLAMP(color.g + getAmount(), 0, 1);
         level->setTeamColor(selectedIndex, color); 
      }

      return true;
   }

   if(inputCode == KEY_B)
   {
      if(mColorEntryMode != ColorEntryModeHex)
      {
         Level *level = getLevel();

         Color color = level->getTeamColor(selectedIndex);
         color.b = CLAMP(color.b + getAmount(), 0, 1);
         level->setTeamColor(selectedIndex, color); 
      }

      return true;
   }

   if(inputCode == KEY_C)  // Want a mouse button?   || inputCode == MOUSE_LEFT)
   {
      UIColorPicker *uiCol = getUIManager()->getUI<UIColorPicker>();
      uiCol->set(ui->getTeam(selectedIndex)->getColor());
      getUIManager()->activate(uiCol);

      return true;
   }

   if(inputCode == KEY_M)      // Toggle ColorEntryMode
   {
      // Advance to the next entry mode
      mColorEntryMode = ColorEntryMode(mColorEntryMode + 1);

      if(mColorEntryMode >= ColorEntryModeCount)
         mColorEntryMode = ColorEntryMode(0);

      // Make sure hex values are correct
      if(mColorEntryMode == ColorEntryModeHex)
         updateAllHexEditors();

      mGameSettings->setSetting<ColorEntryMode>(IniKey::ColorEntryMode, mColorEntryMode);
      return true;
   }

   if(inputCode == KEY_ESCAPE || inputCode == BUTTON_BACK)       // Quit
   {
      playBoop();
      onEscape();
      return true;
   }

   if(inputCode == KEY_UP || inputCode == BUTTON_DPAD_UP)        // Prev item
   {
      selectedIndex--;
      if(selectedIndex < 0)
         selectedIndex = ui->getTeamCount() - 1;
      playBoop();
      Cursor::disableCursor();

      return true;
   }

   if(inputCode == KEY_DOWN || inputCode == BUTTON_DPAD_DOWN)    // Next item
   {
      selectedIndex++;
      if(selectedIndex >= ui->getTeamCount())
         selectedIndex = 0;
      playBoop();
      Cursor::disableCursor();

      return true;
   }

   // Keys 1-9 --> use team preset
   if(inputCode >= KEY_1 && inputCode <= KEY_9)
   {
      // Replace all teams with # of teams based on presets
      if(InputCodeManager::checkModifier(KEY_ALT))
      {
         addTeamsFromPresets(level, inputCode - KEY_1 + 1);
         return true;
      }

      // Replace selection with preset of number pressed
      U32 preset = (inputCode - KEY_1);
      setTeamFromPreset(getLevel(), selectedIndex, preset);

      return true;
   }

   return false;
}


void TeamDefUserInterface::updateAllHexEditors()
{
   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   for(S32 i = 0; i < ui->getTeamCount(); i++)
      mHexColorEditors[i].setString(ui->getTeam(i)->getColor().toHexString());
}


void TeamDefUserInterface::addTeamsFromPresets(Level *level, S32 count)
{
   level->clearTeams();

   for(S32 i = 0; i < count; i++)
   {
      AbstractTeam *team = new EditorTeam(TeamPresets[i]);    // Team manager will clean up
      level->addTeam(team);

      setTeamFromPreset(level, i, i);
   }
}


void TeamDefUserInterface::setTeamFromPreset(Level *level, S32 teamIndex, S32 preset)
{
   mTeamNameEditors[teamIndex].setString(TeamPresets[preset].name);
   mHexColorEditors[teamIndex].setString(TeamPresets[preset].color.toHexString());
}


// Gets called when user starts editing a team, not when the UI is activated
void TeamDefUserInterface::startEditing()
{
   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();
   origName  = ui->getTeam(selectedIndex)->getName().getString();
   origColor = ui->getTeam(selectedIndex)->getColor();
}


void TeamDefUserInterface::doneEditingColor()
{
   mEditingColor = false;

   if(mColorEntryMode == ColorEntryModeHex)
   {
      Level *level = getLevel();
      level->setTeamColor(selectedIndex, Color(mHexColorEditors[selectedIndex].getString()));

      // Finally, let's "normalize" the hex display to reflect how we're interpreting the color entered
      mHexColorEditors[selectedIndex].setString(level->getTeamColor(selectedIndex).toHexString());
   }
}


// User hits Escape while editing team details -- revert changes
void TeamDefUserInterface::cancelEditing()
{
   mEditingName = false;
   mEditingColor = false;

   Level *level = getLevel();

   mTeamNameEditors[selectedIndex].setString(level->getTeamName(selectedIndex).getString());
}


F32 TeamDefUserInterface::getAmount() const
{
   F32 s = InputCodeManager::checkModifier(KEY_SHIFT) ? -1.0f : 1.0f;
   return s / getColorBase();
}


F32 TeamDefUserInterface::getColorBase() const
{
   if(mColorEntryMode == ColorEntryMode100)
      return 100.0f;
   else if(mColorEntryMode == ColorEntryMode255)
      return 255.0f;
   else
      return 1;
}


const char *TeamDefUserInterface::getEntryMessage() const
{
   if(mColorEntryMode == ColorEntryMode100)
      return "[base 100]";
   else if(mColorEntryMode == ColorEntryMode255)
      return "[base 255]";
   else
      return "";
}


void TeamDefUserInterface::onMouseMoved()
{
   Parent::onMouseMoved();

   Cursor::enableCursor();

   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   S32 teams = ui->getTeamCount();

   selectedIndex = (S32)((DisplayManager::getScreenInfo()->getMousePos()->y - yStart + 6) / (fontsize + fontgap)) - 2; 

   if(selectedIndex >= teams)
      selectedIndex = teams - 1;

   if(selectedIndex < 0)
      selectedIndex = 0;
}


void TeamDefUserInterface::onColorPicked(const Color &color)
{
   Level *level = getLevel();
   level->setTeamColor(selectedIndex, color);
}


}

