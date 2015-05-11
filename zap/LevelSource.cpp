//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "LevelSource.h"

#include "config.h"           // For FolderManager
#include "gameType.h"
#include "GameSettings.h"
#include "Level.h"

#include "Md5Utils.h"
#include "stringUtils.h"

#include "tnlAssert.h"

#include <sstream>

namespace Zap
{

// Constructor
LevelInfo::LevelInfo()      
{
   initialize();
}


// Constructor, only used on client
LevelInfo::LevelInfo(const StringTableEntry &name, GameTypeId type)
{
   initialize();

   mLevelName = name;  
   mLevelType = type; 
}


// Constructor
LevelInfo::LevelInfo(const string &filename, const string &folder)
{
   initialize();

   this->filename = filename;
   this->folder   = folder;
}


// Constructor
LevelInfo::LevelInfo(const string &levelName, GameTypeId levelType, S32 minPlayers, S32 maxPlayers, const string &script)
{
   mLevelName = levelName;
   mLevelType = levelType;
   minRecPlayers = minPlayers;
   maxRecPlayers = maxPlayers;
   mScriptFileName = script;
}


// Destructor
LevelInfo::~LevelInfo()
{
   // Do nothing
}


void LevelInfo::initialize()
{
   mLevelType = BitmatchGame;
   minRecPlayers = 0;
   maxRecPlayers = 0;
   mHosterLevelIndex = -1;
}


void LevelInfo::writeToStream(ostream &stream, const string &hash) const
{
   stream << hash          << ",\"" << mLevelName.getString() << "\"," << GameType::getGameTypeName(mLevelType) << "," 
          << minRecPlayers << ","   << maxRecPlayers          << ","   << mScriptFileName                       << '\n'; 
}


const char *LevelInfo::getLevelTypeName()
{
   return GameType::getGameTypeName(mLevelType);
}


// Provide a default name if name is blank
void LevelInfo::ensureLevelInfoHasValidName()
{
   if(mLevelName == "")
      mLevelName = filename;   
}


////////////////////////////////////////
////////////////////////////////////////

// Statics
const string LevelSource::TestFileName = "editor.tmp";


// Constructor
LevelSource::LevelSource()
{
   // Do nothing
}


// Destructor
LevelSource::~LevelSource()
{
   // Do nothing
}


S32 LevelSource::getLevelCount() const
{
   return mLevelInfos.size();
}


LevelInfo LevelSource::getLevelInfo(S32 index)
{
   return mLevelInfos[index];
}


// Remove any "s in place
static void stripQuotes(string &str)      // not const; will be modified!
{
   str.erase(std::remove(str.begin(), str.end(), '"'), str.end());
}


// Static method
bool LevelSource::getLevelInfoFromDatabase(const string &hash, LevelInfo &levelInfo)
{
   return false;
}


// Parse through the chunk of data passed in and find parameters to populate levelInfo with
// This is only used on the server to provide quick level information without having to load the level
// (like with playlists or menus).  Static method.
void LevelSource::getLevelInfoFromCodeChunk(const string &code, LevelInfo &levelInfo)
{
   istringstream stream(code);
   string line;

   bool foundGameType   = false, foundLevelName  = false, foundMinPlayers = false, 
        foundMaxPlayers = false, foundScriptName = false;

   static const S32 gameTypeLen      = strlen("GameType");
   static const S32 levelNameLen     = strlen("LevelName");
   static const S32 minMaxPlayersLen = strlen("MinPlayers");
   static const S32 scriptLen        = strlen("Script");

   std::size_t pos;

   // Iterate until we've either exhausted all the lines, or found everything we're looking for
   while(getline(stream, line) && (
         !foundGameType || !foundLevelName || !foundMinPlayers || !foundMaxPlayers || !foundScriptName))
   {
      // Check for GameType
      if(!foundGameType)
      {
         pos = line.find("GameType");
         if(pos != string::npos)
         {
            string gameTypeName = line.substr(0, pos + gameTypeLen); 

            // ValidateGameType is guaranteed to return a valid GameType name.  Or your money back!!!
            const string validatedName = GameType::validateGameType(gameTypeName);

            GameTypeId gameTypeId = GameType::getGameTypeIdFromName(validatedName);
            levelInfo.mLevelType = gameTypeId;

            foundGameType = true;
            continue;
         }
      }

      // Check for LevelName
      if(!foundLevelName)
      {
         if(line.substr(0, levelNameLen) == "LevelName")
         {
            pos = line.find_first_not_of(" ", levelNameLen + 1);
            if(pos != string::npos)
            {
               string levelName = line.substr(pos);
               stripQuotes(levelName);
               levelInfo.mLevelName = trim(levelName);
            }

            foundLevelName = true;
            continue;
         }
      }

      // Check for MinPlayers
      if(!foundMinPlayers)
      {
         if(line.substr(0, minMaxPlayersLen) == "MinPlayers")
         {
            pos = line.find_first_not_of(" ", minMaxPlayersLen + 1);
            if(pos != string::npos)
               levelInfo.minRecPlayers = atoi(line.substr(pos).c_str());

            foundMinPlayers = true;
            continue;
         }
      }

      // Check for MaxPlayers
      if(!foundMaxPlayers)
      {
         if(line.substr(0, minMaxPlayersLen) == "MaxPlayers")
         {
            pos = line.find_first_not_of(" ", minMaxPlayersLen + 1);
            if(pos != string::npos)
               levelInfo.maxRecPlayers = atoi(line.substr(pos).c_str());

            foundMaxPlayers = true;
            continue;
         }
      }

      // Check for Script
      if(!foundScriptName)
      {
         if(line.substr(0, scriptLen) == "Script")
         {
            pos = line.find_first_not_of(" ", scriptLen + 1);
            if(pos != string::npos)
            {
               string scriptName = line.substr(pos);
               stripQuotes(scriptName);
               levelInfo.mScriptFileName = scriptName;
            }
            foundScriptName = true;
            continue;
         }
      }
   }
}


// User has uploaded a file and wants to add it to the current playlist
pair<S32, bool> LevelSource::addLevel(LevelInfo levelInfo)
{
   // Check if we already have this one -- matches by filename and folder
   for(S32 i = 0; i < mLevelInfos.size(); i++)
   {
      if(mLevelInfos[i].filename == levelInfo.filename && mLevelInfos[i].folder == levelInfo.folder)
         return pair<S32, bool>(i, false);
   }

   // We don't have it... so add it!
   mLevelInfos.push_back(levelInfo);

   return pair<S32, bool>(getLevelCount() - 1, true);
}
void LevelSource::addNewLevel(const LevelInfo &levelInfo)
{
   mLevelInfos.push_back(levelInfo);
}


string LevelSource::getLevelName(S32 index)
{
   if(index < 0 || index >= mLevelInfos.size())
      return "";
   else
      return mLevelInfos[index].mLevelName.getString(); 
}


string LevelSource::getLevelFileName(S32 index)
{
   if(index < 0 || index >= mLevelInfos.size())
      return "";
   else
      return mLevelInfos[index].filename;
}


void LevelSource::setLevelFileName(S32 index, const string &filename)
{
   mLevelInfos[index].filename = filename;
}


GameTypeId LevelSource::getLevelType(S32 index)
{
   return mLevelInfos[index].mLevelType;
}


void LevelSource::remove(S32 index)
{
   mLevelInfos.erase(index);
}


extern S32 QSORT_CALLBACK alphaSort(string *a, string *b);     // Sort alphanumerically

// static method
Vector<string> LevelSource::findAllLevelFilesInFolder(const string &levelDir)
{
   Vector<string> levelList;

   // Build our level list by looking at the filesystem 
   const string extList[] = {"level"};

   if(!getFilesFromFolder(levelDir, levelList, extList, ARRAYSIZE(extList)))    // Returns true if error 
   {
      logprintf(LogConsumer::LogError, "Could not read any levels from the levels folder \"%s\".", levelDir.c_str());
      return levelList;   
   }

   levelList.sort(alphaSort);   // Just to be sure...
   return levelList;
}


bool LevelSource::populateLevelInfoFromSourceByIndex(S32 index)
{
   // If findLevelFile fails, it will return "", which populateLevelInfoFromSource will handle properly
   string filename = FolderManager::findLevelFile(mLevelInfos[index].folder, mLevelInfos[index].filename);
   return populateLevelInfoFromSource(filename, mLevelInfos[index]);
}


// Should be overridden in each subclass of LevelSource
bool LevelSource::loadLevels(FolderManager *folderManager)
{
   return true;
}


////////////////////////////////////////
////////////////////////////////////////


MultiLevelSource::MultiLevelSource()
{
   // Do nothing
}


MultiLevelSource::~MultiLevelSource()
{
   // Do nothing
}


// Populate all our levelInfos from disk; return true if we managed to load any, false otherwise
bool MultiLevelSource::loadLevels(FolderManager *folderManager)
{
   bool anyLoaded = false;

   for(S32 i = 0; i < mLevelInfos.size(); i++)
   {
      if(Parent::populateLevelInfoFromSourceByIndex(i))
         anyLoaded = true;
      else
      {
         mLevelInfos.erase(i);
         i--;
      }
   }

   return anyLoaded;
}


// Load specified level, put results in gameObjectDatabase.  Return md5 hash of level
Level *MultiLevelSource::getLevel(S32 index) const
{
   TNLAssert(index >= 0 && index < mLevelInfos.size(), "Index out of bounds!");

   const LevelInfo *levelInfo = &mLevelInfos[index];

   string filename = FolderManager::findLevelFile(levelInfo->folder, levelInfo->filename);

   if(filename == "")
   {
      logprintf("Unable to find level file \"%s\".  Skipping...", levelInfo->filename.c_str());
      return NULL;
   }

   Level *level = new Level();      // Deleted by Game

   if(!level->loadLevelFromFile(filename))
   {
      logprintf("Unable to process level file \"%s\".  Skipping...", levelInfo->filename.c_str());
      delete level;
      return NULL;
   }

   return level;
}


// Returns a textual level descriptor good for logging and error messages and such
string MultiLevelSource::getLevelFileDescriptor(S32 index) const
{
   return "levelfile \"" + mLevelInfos[index].filename + "\"";
}


// Populates levelInfo with data from fullFilename -- returns true if successful, false otherwise
// Reads 4kb of file and uses what it finds there to populate the levelInfo
bool MultiLevelSource::populateLevelInfoFromSource(const string &fullFilename, LevelInfo &levelInfo)
{
   // Check if we got a dud... (FolderManager::findLevelFile() will, for example, return "" if it fails)
   if(fullFilename.empty())
      return false;

	FILE *f = fopen(fullFilename.c_str(), "rb");
	if(!f)
   {
      logprintf(LogConsumer::LogWarning, "Could not read level file %s [%s]... Skipping...",
                                          levelInfo.filename.c_str(), fullFilename.c_str());
      return false;
   }

   Level level;
   level.loadLevelFromFile(fullFilename);

// some ideas for getting the area of a level:
//   if(loadLevel())
//      {
//         loaded = true;
//         logprintf(LogConsumer::ServerFilter, "Done. [%s]", getTimeStamp().c_str());
//      }
//      else
//      {
//         logprintf(LogConsumer::ServerFilter, "FAILED!");
//
//         if(mLevelSource->getLevelCount() > 1)
//            removeLevel(mCurrentLevelIndex);
//         else
//         {
//            // No more working levels to load...  quit?
//            logprintf(LogConsumer::LogError, "All the levels I was asked to load are corrupt.  Exiting!");
//
//            mShutdownTimer.reset(1); 
//            mShuttingDown = true;
//            mShutdownReason = "All the levels I was asked to load are corrupt or missing; "
//                              "Sorry dude -- hosting mode shutting down.";
//
//            // To avoid crashing...
//            if(!getGameType())
//            {
//               GameType *gameType = new GameType();
//               gameType->addToGame(this, getLevel());
//            }
//            mLevel->makeSureTeamCountIsNotZero();
//
//            return;
//         }
//      }
//   }
//
//   computeWorldObjectExtents();                       // Compute world Extents nice and early
//
//   if(!mGameRecorderServer && !mShuttingDown && getSettings()->getSetting<YesNo>(IniKey::GameRecording))
//      mGameRecorderServer = new GameRecorderServer(this);
//
//
//   ////// This block could easily be moved off somewhere else   
//   fillVector.clear();
//   getLevel()->findObjects(TeleporterTypeNumber, fillVector);
//
//   Vector<pair<Point, const Vector<Point> *> > teleporterData(fillVector.size());
//   pair<Point, const Vector<Point> *> teldat;
//
//   for(S32 i = 0; i < fillVector.size(); i++)
//   {
//      Teleporter *teleporter = static_cast<Teleporter *>(fillVector[i]);
//
//      teldat.first  = teleporter->getPos();
//      teldat.second = teleporter->getDestList();
//
//      teleporterData.push_back(teldat);
//   }
//
//   // Get our parameters together
//   Vector<DatabaseObject *> barrierList;
//   getLevel()->findObjects((TestFunc)isWallType, barrierList, *getWorldExtents());
//
//   Vector<DatabaseObject *> turretList;
//   getLevel()->findObjects(TurretTypeNumber, turretList, *getWorldExtents());
//
//   Vector<DatabaseObject *> forceFieldProjectorList;
//   getLevel()->findObjects(ForceFieldProjectorTypeNumber, forceFieldProjectorList, *getWorldExtents());
//
//   bool triangulate;
//
//   // Try and load Bot Zones for this level, set flag if failed
//   // We need to run buildBotMeshZones in order to set mAllZones properly, which is why I (sort of) disabled the use of hand-built zones in level files
//#ifdef ZAP_DEDICATED
//   triangulate = false;
//#else
//   triangulate = !isDedicated();
//#endif
//
//   BotNavMeshZone::calcLevelSize(getWorldExtents(), barrierList, teleporterData);
//
//   ////////////////////////////
//
//





   char data[1024 * 4];  // Should be enough to fit all parameters at the beginning of level; we don't need to read everything
   S32 size = (S32)fread(data, 1, sizeof(data), f);
   fclose(f);

   getLevelInfoFromCodeChunk(string(data, size), levelInfo);     // Fills levelInfo with data from file


   levelInfo.ensureLevelInfoHasValidName();
   return true;
}


bool MultiLevelSource::isEmptyLevelDirOk() const
{
   return false;
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor -- pass in a list of level names and a folder; create LevelInfos for each
FolderLevelSource::FolderLevelSource(const Vector<string> &levelList, const string &folder)
{
   for(S32 i = 0; i < levelList.size(); i++)
      mLevelInfos.push_back(LevelInfo(levelList[i], folder));
}


// Destructor
FolderLevelSource::~FolderLevelSource()
{
   // Do noting
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor -- pass in a list of level names and a file; create LevelInfos for each
FileListLevelSource::FileListLevelSource(const Vector<string> &levelList, const string &folder, GameSettings *settings)
{
   mGameSettings = settings;

   for(S32 i = 0; i < levelList.size(); i++)
      mLevelInfos.push_back(LevelInfo(levelList[i], folder));
}


// Destructor
FileListLevelSource::~FileListLevelSource()
{
   // Do nothing
}


// Load specified level, put results in gameObjectDatabase.  Return md5 hash of level.
Level *FileListLevelSource::getLevel(S32 index) const
{
   TNLAssert(index >= 0 && index < mLevelInfos.size(), "Index out of bounds!");

   const LevelInfo *levelInfo = &mLevelInfos[index];

   string filename = FolderManager::findLevelFile(GameSettings::getFolderManager()->getLevelDir(), levelInfo->filename);

   if(filename == "")
   {
      logprintf("Unable to find level file \"%s\".  Skipping...", levelInfo->filename.c_str());
      return NULL;
   }

   Level *level = new Level();      // Will be deleted by caller

   if(!level->loadLevelFromFile(filename))
   {
      logprintf("Unable to process level file \"%s\".  Skipping...", levelInfo->filename.c_str());
      delete level;
      return NULL;
   }

   return level;
}


// Static method
Vector<string> FileListLevelSource::findAllFilesInPlaylist(const string &fileName, const string &levelDir)
{
   Vector<string> levels;
   string contents;

   readFile(fileName, contents);
   Vector<string> lines = parseString(contents);

   for(S32 i = 0; i < lines.size(); i++)
   {
      string filename = trim(chopComment(lines[i]));
      if(filename == "")    // Probably a comment or blank line
         continue;

      string fullFileName = FolderManager::findLevelFile(levelDir, filename);

      if(fullFileName == "")
      {
         logprintf("Unable to find level file \"%s\".  Skipping...", filename.c_str());
         continue;
      }

      levels.push_back(filename);      // We will append the folder name later
   }

   return levels;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor -- single level
StringLevelSource::StringLevelSource(const string &levelCode)
{
   mLevelCodes.push_back(levelCode);

   LevelInfo levelInfo;
   mLevelInfos.push_back(levelInfo);
}


// Constructor -- mulitlpe levels (only used for testing, at the moment)
StringLevelSource::StringLevelSource(const Vector<string> &levelCodes)
{
   mLevelCodes = levelCodes;

   for(S32 i = 0; i < levelCodes.size(); i++)
   {
      LevelInfo levelInfo;
      mLevelInfos.push_back(levelInfo);
   }
}


// Destructor
StringLevelSource::~StringLevelSource()
{
   // Do nothing
}


bool StringLevelSource::populateLevelInfoFromSourceByIndex(S32 levelInfoIndex)
{
   getLevelInfoFromCodeChunk(mLevelCodes[levelInfoIndex], mLevelInfos[levelInfoIndex]);
   return true;
}


bool StringLevelSource::populateLevelInfoFromSource(const string &fullFilename, LevelInfo &levelInfo)
{
   TNLAssert(false, "This is never called!");
   return true;
}


Level *StringLevelSource::getLevel(S32 index) const
{
   Level *level = new Level();

   level->loadLevelFromString(mLevelCodes[index], "");
   return level; 
}


// Returns a textual level descriptor good for logging and error messages and such
string StringLevelSource::getLevelFileDescriptor(S32 index) const
{
   return "string input (" + itos((U32) mLevelCodes[index].length()) + " chars)";
}


bool StringLevelSource::isEmptyLevelDirOk() const
{
   return true;      // No folder needed -- level was passed into constructor!
}


}
