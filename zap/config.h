//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _CONFIG_H_
#define _CONFIG_H_

// This file contains definitions of two structs that are used to store our
// INI settings and command line param settings, which are read separately,
// but processed jointly.  Some default values are provided here as well,
// especially for the INI settings...  if the INI is deleted, these defaults
// will be used to rebuild it.

#include "Color.h"      // For Color def
#include "ConfigEnum.h" // For sfxSets, DisplayMode
#include "Settings.h"

#include "tnlTypes.h"
#include "tnlNetStringTable.h"
#include "tnlVector.h"

#include <boost/shared_ptr.hpp>

#include <string>
#include <map>

using namespace std;
using namespace TNL;


namespace Zap
{

extern const char *MASTER_SERVER_LIST_ADDRESS;

////////////////////////////////////
////////////////////////////////////

class GameSettings;
struct CmdLineSettings;


class FolderManager 
{
private:
   string levelDir;
   string robotDir;
   string sfxDir;
   string musicDir;
   string iniDir;
   string logDir;
   string screenshotDir;
   string luaDir;
   string rootDataDir;
   string pluginDir;
   string fontsDir;
   string recordDir;

public:
   // Constructors
   FolderManager();
   FolderManager(const string &levelDir,    const string &robotDir,  const string &sfxDir,        const string &musicDir, 
                 const string &iniDir,      const string &logDir,    const string &screenshotDir, const string &luaDir,
                 const string &rootDataDir, const string &pluginDir, const string &fontsDir,      const string &recordDir);

   virtual ~FolderManager();     // Destructor

   string getLevelDir() const;
   string getIniDir() const;
   string getRecordDir() const;
   string getFontsDir() const;
   string getScreenshotDir() const;
   string getSfxDir() const;
   string getRobotDir() const;
   string getMusicDir() const;
   string getRootDataDir() const;
   string getLogDir() const;
   string getPluginDir() const;
   string getLuaDir() const;


   void resolveDirs(GameSettings *settings);                                  
   void resolveDirs(const string &root);
   void resolveLevelDir(GameSettings *settings);                                 
   string resolveLevelDir(const string &levelDir);

   string findLevelFile(const string &filename) const;
   static string findLevelFile(const string &levelDir, const string &filename);

   Vector<string> getScriptFolderList() const;
   Vector<string> getPluginFolderList() const;
   Vector<string> getHelperScriptFolderList() const;

   string findLevelGenScript(const string &fileName) const;
   string findPlugin(const string &filename) const;
   string findBotFile(const string &filename) const;
   string findScriptFile(const string &filename) const;

   void setLevelDir(const string &levelDir);
};


////////////////////////////////////////
////////////////////////////////////////

class GameSettings;

struct CmdLineSettings
{
   CmdLineSettings();      // Constructor
   virtual ~CmdLineSettings();

   void init();
   
   bool dedicatedMode;     // Will server be dedicated?

   string server;
   string masterAddress;   // Use this master server

   F32 loss;               // Simulate packet loss (0-1)
   U32 lag;                // Simulate server lag (in ms)
   U32 stutter;            // Simulate VPS CPU stutter (0-1000)

   bool forceUpdate;       // For testing updater
   string dedicated;       // Holds bind address specified on cmd line following -dedicated parameter
   string name;
   string password;

   string hostname;
   string hostaddr;        // Address to listen on when we're host (e.g. IP:localhost:1234 or IP:Any:6666 or whatever)
   string hostdescr;       // One-line description of server
   string serverPassword;  // Password required to connect to server
   string adminPassword;   // Password required to perform certain admin functions
   string levelChangePassword;   // Password required to change levels and such

   FolderManager dirs;

   S32 maxPlayers;

   DisplayMode displayMode;    // Fullscreen param supplied

   S32 winWidth;
   S32 xpos;
   S32 ypos;

   Vector<string> specifiedLevels;
};


////////////////////////////////////////
////////////////////////////////////////

// Keep track of which keys editor plugins have been bound to
struct PluginBinding
{
   string key;          // Key user presses
   string script;       // Plugin script that gets run
   string help;         // To be shown in help
};


////////////////////////////////////////
////////////////////////////////////////

// For holding user-specific settings
struct UserSettings
{
   // Not really an enum at the moment...
   enum ExperienceLevels {
      // 0-20, 20-50, 50-100, 100-200, 200-500, 500-1000, 1000-2000, 2000-5000, 5000+
      LevelCount = 9
   };

   UserSettings();      // Constructor
   ~UserSettings();     // Destructor

   string name;
   bool levelupItemsAlreadySeen[LevelCount];
};


////////////////////////////////////////
////////////////////////////////////////

class CIniFile;
class InputCodeManager;

//template <class T>
struct IniSettings      // With defaults specified
{

public:
   IniSettings();       // Constructor
   virtual ~IniSettings();

   Settings<IniKey::SettingsItem> mSettings;

   DisplayMode oldDisplayMode;

   Vector<PluginBinding> getDefaultPluginBindings() const;

   Vector<PluginBinding> pluginBindings;  // Keybindings for the editor plugins

   Vector<StringTableEntry> levelList;

   Vector<string> reservedNames;
   Vector<string> reservedPWs;

   Vector<string> prevServerListFromMaster;
   Vector<string> alwaysPingList;

   // Some static methods for converting between bit arrays and INI friendly strings
   static void clearbits(bool *items, S32 itemCount);
   static string bitArrayToIniString(const bool *items, S32 itemCount);
   static void iniStringToBitArray(const string &vals, bool *items, S32 itemCount);

   static void loadUserSettingsFromINI(CIniFile *ini, GameSettings *settings);    // Load user-specific settings
   static void saveUserSettingsToINI(const string &name, CIniFile *ini, GameSettings *settings);
};


void saveSettingsToINI  (CIniFile *ini, GameSettings *settings);
void loadSettingsFromINI(CIniFile *ini, GameSettings *settings);    // Load standard game settings

void writeSkipList(CIniFile *ini, const Vector<string> *levelSkipList);

};

#endif


