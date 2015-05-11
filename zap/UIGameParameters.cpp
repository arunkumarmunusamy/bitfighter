//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIGameParameters.h"

#include "UIEditor.h"
#include "UIManager.h"

#include "game.h"
#include "gameType.h"
#include "ClientGame.h"
#include "Cursor.h"

#include "stringUtils.h"


namespace Zap
{

// Default constructor
SavedMenuItem::SavedMenuItem()
{
   /* Unused */
}


SavedMenuItem::SavedMenuItem(MenuItem *menuItem)
{
   mParamName = menuItem->getPrompt();
   setValues(menuItem);
}

// Destructor
SavedMenuItem::~SavedMenuItem()
{
   // Do nothing
}


void SavedMenuItem::setValues(MenuItem *menuItem)
{
   mParamVal = menuItem->getValueForWritingToLevelFile();
}


string SavedMenuItem::getParamName()
{
   return mParamName;
}


string SavedMenuItem::getParamVal()
{
   return mParamVal;
}


////////////////////////////////////
////////////////////////////////////


// Constructor
GameParamUserInterface::GameParamUserInterface(ClientGame *game, UIManager *uiManager) : 
   Parent(game, uiManager)
{
   mMenuTitle = "GAME PARAMETERS MENU";
   mMenuSubTitle = "";
   mMaxMenuSize = S32_MAX;                // We never want scrolling on this menu!

   mLevelFilename = "";
}


// Destructor
GameParamUserInterface::~GameParamUserInterface()
{
   // Do nothing
}


void GameParamUserInterface::onActivate()
{
   TNLAssert(getUIManager()->cameFrom<EditorUserInterface>(), "GameParamUserInterface should only be called from the editor!");

   Level *level = getUIManager()->getUI<EditorUserInterface>()->getLevel();
   const GameType *gameType = level->getGameType();

   // Force rebuild of all params for current gameType; this will make sure we have the latest info if we've loaded a new level,
   // but will also preserve any values entered for gameTypes that are not current.
   clearCurrentGameTypeParams(gameType);
   
   // Load filename from editor only when we activate the menu
   mLevelFilename = stripExtension(getUIManager()->getUI<EditorUserInterface>()->getLevelFileName());
   if(mLevelFilename == EditorUserInterface::UnnamedFile)
      mLevelFilename = "";

   updateMenuItems(gameType);   
   mOrigGameParams = level->toLevelCode();   // Save a copy of the params coming in for comparison when we leave to see what changed
   Cursor::disableCursor();
}


// Find and delete any parameters associated with the current gameType
void GameParamUserInterface::clearCurrentGameTypeParams(const GameType *gameType)
{
   // Get the current GameType from the level being edited in the Editor
   const Vector<string> *keys = gameType->getGameParameterMenuKeys();

   for(S32 i = 0; i < keys->size(); i++)
   {
      MenuItemMap::iterator iter = mMenuItemMap.find(keys->get(i));

      boost::shared_ptr<MenuItem> menuItem;

      if(iter != mMenuItemMap.end())      
         mMenuItemMap.erase(iter);
   }
}


extern S32 QSORT_CALLBACK alphaSort(string *a, string *b);

static const Vector<string> buildGameTypesList()
{
   Vector<string> gameTypes;

   gameTypes = GameType::getGameTypeNames();
   gameTypes.sort(alphaSort);

   return gameTypes;
}


static const Vector<string> &getGameTypes()
{
   static const Vector<string> gameTypes = buildGameTypesList();

   return gameTypes;
}


static void changeGameTypeCallback(ClientGame *game, U32 gtIndex)
{
   // Instantiate our gameType object and cast it to GameType
   TNL::Object *theObject = TNL::Object::create(GameType::getGameTypeClassName(getGameTypes()[gtIndex]));
   GameType *gt = dynamic_cast<GameType *>(theObject);   

   TNLAssert(gt, "Whoa!");
   TNLAssert(game->getLevel(), "Whoa!");
   
   game->getLevel()->setGameType(gt);     // gt will be put into a RefPtr, which will handle cleanup

   // If we have a new gameType, we might have new game parameters; update the menu!
   game->getUIManager()->getUI<GameParamUserInterface>()->updateMenuItems(gt);
}


void GameParamUserInterface::updateMenuItems(const GameType *gameType)
{
   TNLAssert(gameType, "Missing game type!");

   string filename = mLevelFilename;
   // Grab the level filename from the menuitem if it has been built already.
   // This let's us persist a changed filename without having to leave the menu first
   if(getMenuItemCount() > 0)
      filename = getMenuItem(1)->getValue();

   clearMenuItems();

   // Note that on some gametypes instructions[1] is NULL
   string instructs = string(gameType->getInstructionString()[0]) + 
                             (gameType->getInstructionString()[1] ?  string(" ") + gameType->getInstructionString()[1] : "");

   addMenuItem(new ToggleMenuItem("Game Type:",       
                                  getGameTypes(),
                                  getGameTypes().getIndex(gameType->getGameTypeName()),
                                  true,
                                  changeGameTypeCallback,
                                  instructs));


   addMenuItem(new TextEntryMenuItem("Filename:",                         // name
                                     filename,                            // val
                                     EditorUserInterface::UnnamedFile,    // empty val
                                     "File where this level is stored",   // help
                                     MAX_FILE_NAME_LEN));

   const Vector<string> *keys = gameType->getGameParameterMenuKeys();

   for(S32 i = 0; i < keys->size(); i++)
   {
      MenuItemMap::iterator iter = mMenuItemMap.find(keys->get(i));

      boost::shared_ptr<MenuItem> menuItem;

      if(iter != mMenuItemMap.end())      // What is this supposed to do?  I can't seem to make this condition occur.
         menuItem = iter->second;
      else                 // Item not found
      {
         menuItem = gameType->getMenuItem(keys->get(i));
         TNLAssert(menuItem, "Failed to make a new menu item!");

         mMenuItemMap.insert(pair<string, boost::shared_ptr<MenuItem> >(keys->get(i), menuItem));
      }

      addWrappedMenuItem(menuItem);
   }
}


// Runs as we're exiting the menu
void GameParamUserInterface::onEscape()
{
   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   string newFilename = getMenuItem(1)->getValue();

   bool filenameChanged = mLevelFilename != newFilename;
   if(filenameChanged)
      ui->setLevelFileName(newFilename);

   GameType *gameType = getUIManager()->getUI<EditorUserInterface>()->getLevel()->getGameType();

   const Vector<string> *keys = gameType->getGameParameterMenuKeys();

   for(S32 i = 0; i < keys->size(); i++)
   {
      MenuItemMap::iterator iter = mMenuItemMap.find(keys->get(i));

      MenuItem *menuItem = iter->second.get();
      gameType->saveMenuItem(menuItem, keys->get(i));
   }

   if(anythingChanged() || filenameChanged)
   {
      EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();
      // TODO -->Save undo state here!!!
      ui->validateLevel();
   }

   // Now back to our previously scheduled program...  (which will be the editor, of course)
   getUIManager()->reactivatePrevUI();
}


void GameParamUserInterface::processSelection(U32 index)
{
   // Do nothing
}


bool GameParamUserInterface::anythingChanged()
{
   // While we were in the menu, no items changed... the only things that could have changed would be in the level
   // metadata and settings section.
   return mOrigGameParams != getUIManager()->getUI<EditorUserInterface>()->getLevel()->toLevelCode();
}


S32 GameParamUserInterface::getTextSize(MenuItemSize size) const
{
   return 18;
}


S32 GameParamUserInterface::getGap(MenuItemSize size) const
{
   return 12;
}


S32 GameParamUserInterface::getYStart() const
{
   return 70;
}



};

