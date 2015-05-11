//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "master.h"

#include "database.h"            // For writing to the database
#include "DatabaseAccessThread.h"
#include "GameJoltConnector.h"

#include "../zap/stringUtils.h"  // For itos, replaceString
#include "../zap/IniFile.h"      // For INI reading/writing


namespace Master 
{

// Constructor
MasterSettings::MasterSettings(const string &iniFile)     
{
   ini.SetPath(iniFile);

#  define SETTINGS_ITEM(typeName, enumVal, section, key, defaultVal, readValidator, writeValidator, comment)               \
            mSettings.add(new Setting<typeName, IniKey::SettingsItem>(IniKey::enumVal, defaultVal, key,                    \
                                                                      section, readValidator, writeValidator, comment));
      MASTER_SETTINGS_TABLE
#  undef SETTINGS_ITEM
}


void MasterSettings::readConfigFile()
{
   if(ini.getPath() == "")
      return;

   // Clear, then read
   ini.Clear();
   ini.ReadFile();

   // Now set up variables -- copies data from ini to settings
   loadSettingsFromINI();

   // Not sure if this should go here...
   if(getVal<U32>(IniKey::LatestReleasedCSProtocol) == 0 && getVal<U32>(IniKey::LatestReleasedBuildVersion) == 0)
      logprintf(LogConsumer::LogError, "Unable to find a valid protocol line or build_version in config file... disabling update checks!");
}


extern Vector<string> master_admins;


void MasterSettings::loadSettingsFromINI()
{
   // Read all settings defined in the new modern manner
   S32 sectionCount = ini.GetNumSections();

   for(S32 i = 0; i < sectionCount; i++)
   {
      string section = ini.getSectionName(i);

      // Enumerate all settings we've defined for [section]
      Vector<AbstractSetting<IniKey::SettingsItem>  *> settings = mSettings.getSettingsInSection(section);

      for(S32 j = 0; j < settings.size(); j++)
         settings[j]->setValFromString(ini.GetValue(section, settings[j]->getKey(), settings[j]->getDefaultValueString()));
   }

   // Got to do something about this!
   string str1 = ini.GetValue("host", "master_admin", "");
   parseString(str1.c_str(), master_admins, ',');


   // [stats] section --> most has been modernized
   DbWriter::DatabaseWriter::sqliteFile = ini.GetValue("stats", "sqlite_file_basename", DbWriter::DatabaseWriter::sqliteFile);


   // [motd_clients] section
   // This section holds each old client build number as a key.  This allows us to set
   // different messages for different versions
   string defaultMessage = "New version available at bitfighter.org";
   Vector<string> keys;
   ini.GetAllKeys("motd_clients", keys);

   motdClientMap.clear();

   for(S32 i = 0; i < keys.size(); i++)
   {
      U32 build_version = (U32)atoi(keys[i].c_str());    // Avoid conflicts with std::stoi() which is defined for VC++ 10
      string message = ini.GetValue("motd_clients", keys[i], defaultMessage);

      motdClientMap.insert(pair<U32, string>(build_version, message));
   }


   // [motd] section
   // Here we just get the name of the file.  We use a file so the message can be updated
   // externally through the website
   string motdFilename = ini.GetValue("motd", "motd_file", "motd");  // Default 'motd' in current directory

   // Grab the current message and add it to the map as the most recently released build
   motdClientMap[getVal<U32>(IniKey::LatestReleasedBuildVersion)] = getCurrentMOTDFromFile(motdFilename);
}


string MasterSettings::getCurrentMOTDFromFile(const string &filename) const
{
   string defaultMessage = "Welcome to Bitfighter!";

   FILE *f = fopen(filename.c_str(), "r");
   if(!f)
   {
      logprintf(LogConsumer::LogError, "Unable to open MOTD file \"%s\" -- using default MOTD.", filename.c_str());
      return defaultMessage;
   }

   char message[MOTD_LEN];
   bool ok = fgets(message, MOTD_LEN, f);
   fclose(f);

   if(!ok)
      return defaultMessage;


   string returnMessage(message);
   trim_right_in_place(returnMessage, "\n");  // Remove any trailing new lines

   return returnMessage;
}


// If clientBuildVersion is U32_MAX, then return the motd for the latest build
string MasterSettings::getMotd(U32 clientBuildVersion) const
{
   string motdString = "Welcome to Bitfighter!";

   // Use latest if build version is U32_MAX
   if(clientBuildVersion == U32_MAX)
      clientBuildVersion = getVal<U32>(IniKey::LatestReleasedBuildVersion);

   map <U32, string>::const_iterator iter = motdClientMap.find(clientBuildVersion);
   if(iter != motdClientMap.end())
      motdString = (*iter).second;

   return motdString;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
MasterServer::MasterServer(MasterSettings *settings)
{
   mSettings = settings;

   mStartTime = Platform::getRealMilliseconds();

   // Initialize our net interface so we can accept connections...  mNetInterface is deleted in destructor
   mNetInterface = createNetInterface();

   mCleanupTimer.reset(TEN_MINUTES);
   mReadConfigTimer.reset(FIVE_SECONDS);        // Reread the config file every 5 seconds... excessive?
   mJsonWriteTimer.reset(0, FIVE_SECONDS);      // Max frequency for writing JSON files -- set current to 0 so we'll write immediately
   mPingGameJoltTimer.reset(THIRTY_SECONDS);    // Game Jolt recommended frequency... sessions time out after 2 mins

   mJsonWritingSuspended = false;

   mLastMotd = mSettings->getMotd();            // When this changes, we'll broadcast a new MOTD to clients
   
   mDatabaseAccessThread = new DatabaseAccessThread();    // Deleted in destructor

   MasterServerConnection::setMasterServer(this);
}


// Destructor
MasterServer::~MasterServer()
{
   delete mNetInterface;

   delete mDatabaseAccessThread;
}


NetInterface *MasterServer::createNetInterface() const
{
   U32 port = mSettings->getVal<U32>(IniKey::Port);
   NetInterface *netInterface = new NetInterface(Address(IPProtocol, Address::Any, port));

   // Log a welcome message in the main log and to the console
   logprintf("[%s] Master Server \"%s\" started - listening on port %d", getTimeStamp().c_str(),
                                                                         getSetting<string>(IniKey::ServerName).c_str(),
                                                                         port);
   return netInterface;
}


U32 MasterServer::getStartTime() const
{
   return mStartTime;
}


const MasterSettings *MasterServer::getSettings() const
{
   return mSettings;
}


// Will trigger a JSON rewrite after timer has run its full cycle
void MasterServer::writeJsonDelayed()
{
   mJsonWriteTimer.reset();
   mJsonWritingSuspended = false;
}


// Indicates we want to write JSON as soon as possible... but never more 
// frequently than allowed by mJsonWriteTimer, which we don't reset here
void MasterServer::writeJsonNow()
{
   mJsonWritingSuspended = false;
}


const Vector<MasterServerConnection *> *MasterServer::getServerList() const
{
   return &mServerList;
}


const Vector<MasterServerConnection *> *MasterServer::getClientList() const
{
   return &mClientList;
}


void MasterServer::addServer(MasterServerConnection *server)
{
   mServerList.push_back(server);
}


void MasterServer::addClient(MasterServerConnection *client)
{
   mClientList.push_back(client);
}


void MasterServer::removeServer(S32 index)
{
   TNLAssert(index >= 0 && index < mServerList.size(), "Index out of range!");
   mServerList.erase_fast(index);
}


void MasterServer::removeClient(S32 index)
{
   TNLAssert(index >= 0 && index < mClientList.size(), "Index out of range!");
   mClientList.erase_fast(index);
}


NetInterface *MasterServer::getNetInterface() const
{
   return mNetInterface;
}


// Returns true if motd has changed since we were last here
bool MasterServer::motdHasChanged() const
{
   return mSettings->getMotd() != mLastMotd;
}


void MasterServer::idle(const U32 timeDelta)
{
   mNetInterface->checkIncomingPackets();
   mNetInterface->processConnections();

   // Reread config file
   if(mReadConfigTimer.update(timeDelta))
   {
      mSettings->readConfigFile();
      mReadConfigTimer.reset();

      if(motdHasChanged())
      {
         broadcastMotd();
         mLastMotd = mSettings->getMotd();
      }
   }

   // Cleanup, cleanup, everybody cleanup!
   if(mCleanupTimer.update(timeDelta))
   {
      MasterServerConnection::removeOldEntriesFromRatingsCache();    //<== need non-static access
      mCleanupTimer.reset();
   }

   // Handle writing our JSON file
   mJsonWriteTimer.update(timeDelta);

   if(!mJsonWritingSuspended && mJsonWriteTimer.getCurrent() == 0)
   {
      MasterServerConnection::writeClientServerList_JSON();

      mJsonWritingSuspended = true;    // No more writes until this is cleared
      mJsonWriteTimer.reset();         // But reset the timer so it start ticking down even if we aren't writing
   }


   if(mPingGameJoltTimer.update(timeDelta))
   {
      GameJolt::ping(mSettings, getClientList());
      mPingGameJoltTimer.reset();
   }


   // Process connections -- cycle through them and check if any have timed out
   U32 currentTime = Platform::getRealMilliseconds();

   for(S32 i = MasterServerConnection::gConnectList.size() - 1; i >= 0; i--)    
   {
      GameConnectRequest *request = MasterServerConnection::gConnectList[i];    

      if(currentTime - request->requestTime > (U32)FIVE_SECONDS)
      {
         if(request->initiator.isValid())
         {
            ByteBufferPtr ptr = new ByteBuffer((U8 *)MasterRequestTimedOut, strlen(MasterRequestTimedOut) + 1);

            request->initiator->m2cArrangedConnectionRejected(request->initiatorQueryId, ptr);   // 0 = ReasonTimedOut
            request->initiator->removeConnectRequest(request);
         }

         if(request->host.isValid())
            request->host->removeConnectRequest(request);

         MasterServerConnection::gConnectList.erase_fast(i);
         delete request;
      }
   }

   // Process any delayed disconnects; we use this to avoid repeating and flooding join / leave messages
   for(S32 i = MasterServerConnection::gLeaveChatTimerList.size() - 1; i >= 0; i--)
   {
      MasterServerConnection *c = MasterServerConnection::gLeaveChatTimerList[i];      

      if(!c || c->mLeaveLobbyChatTimer == 0)
         MasterServerConnection::gLeaveChatTimerList.erase(i);                         
      else
      {
         if(currentTime - c->mLeaveLobbyChatTimer > (U32)ONE_SECOND)
         {
            c->isInLobbyChat = false;

            const Vector<MasterServerConnection *> *clientList = getClientList();

            for(S32 j = 0; j < clientList->size(); j++)
               if(clientList->get(j) != c && clientList->get(j)->isInLobbyChat)
                  clientList->get(j)->m2cPlayerLeftGlobalChat(c->mPlayerOrServerName);

            MasterServerConnection::gLeaveChatTimerList.erase(i);
         }
      }
   }

   mDatabaseAccessThread->idle();
}


// Send MOTD to all connected clients -- used when MOTD has changed
void MasterServer::broadcastMotd() const
{
   S32 latestVersion = mSettings->getVal<U32>(IniKey::LatestReleasedBuildVersion);

   for(S32 i = 0; i < mClientList.size(); i++)
   {
      // Only send the new message to the most recent clients
      if(mClientList[i]->getClientBuild() == latestVersion)
         mClientList[i]->sendMotd();
   }
}


DatabaseAccessThread *MasterServer::getDatabaseAccessThread()
{
   return mDatabaseAccessThread;
}

}  // namespace

