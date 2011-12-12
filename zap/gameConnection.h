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

#ifndef _GAMECONNECTION_H_
#define _GAMECONNECTION_H_


#include "controlObjectConnection.h"
#include "dataConnection.h"            // For DataSendable interface
#include "statistics.h"
#include "SoundSystem.h"               // for NumSFXBuffers

#include "tnlNetConnection.h"
#include "Timer.h"
#include <time.h>
#include "boost/smart_ptr/shared_ptr.hpp"

#include "GameTypesEnum.h"



using namespace TNL;
using namespace std;

namespace Zap
{

static const char USED_EXTERNAL *gConnectStatesTable[] = {
      "Not connected...",
      "Sending challenge request...",
      "Punching through firewalls...",
      "Computing puzzle solution...",
      "Sent connect request...",
      "Connection timed out",
      "Connection rejected",
      "Connected",
      "Disconnected",
      "Connection timed out",
      ""
};


////////////////////////////////////////
////////////////////////////////////////

class ClientGame;
struct LevelInfo;
class GameSettings;
class LuaPlayerInfo;
class ClientInfo;
class LocalClientInfo;


class GameConnection: public ControlObjectConnection, public DataSendable
{
private:
   typedef ControlObjectConnection Parent;

   void initialize();

   time_t joinTime;
   bool mAcheivedConnection;

   // For saving passwords
   string mLastEnteredLevelChangePassword;
   string mLastEnteredAdminPassword;

   // These are only used on the server -- will be NULL on client
   boost::shared_ptr<ClientInfo> mClientInfo;         
   LuaPlayerInfo *mPlayerInfo;      // Lua access to this class


#ifndef ZAP_DEDICATED
   ClientGame *mClientGame;         // Sometimes this is NULL
#endif

   bool mInCommanderMap;
   bool mWaitingForPermissionsReply;
   bool mGotPermissionsReply;
   bool mIsBusy;              // True when the player is off chatting or futzing with options or whatever, false when they are "active"

   bool mWantsScoreboardUpdates;          // Indicates if client has requested scoreboard streaming (e.g. pressing Tab key)
   bool mReadyForRegularGhosts;

   //StringTableEntry mClientName;
   StringTableEntry mClientNameNonUnique; // For authentication, not unique name
   //Nonce mClientId;
   bool mClientClaimsToBeVerified;
   bool mClientNeedsToBeVerified;
   bool mIsVerified;                      // True if the connection has a verified account confirmed by the master
   Timer mAuthenticationTimer;
   S32 mAuthenticationCounter;

   void displayMessage(U32 colorIndex, U32 sfxEnum, const char *message);    // Helper function

   StringTableEntry mServerName;
   Vector<U32> mLoadout;

   GameSettings *mSettings;

   // Score for current game
   //S32 mScore;                   // Score this game
   //F32 mRating;                  // Rating for this game
   //S32 mTotalScore;              // Total number of points scored by anyone this game

   // Long term score tracking
   S32 mCumulativeScore;         // Total points scored my this connection over it's entire lifetime
   S32 mTotalCumulativeScore;    // Total points scored by anyone while this connection is alive
   U32 mGamesPlayed;             // Number of games played, obviously

public:
   Vector<U32> mOldLoadout;      // Server: to respawn with old loadout  Client: to check if using same loadout configuration
   U16 switchedTeamCount;

   U8 mVote;  // 0 = not voted,  1 = vote yes,  2 = vote no
   U32 mVoteTime;
   bool mChatMute;

   U32 mChatTimer;
   bool mChatTimerBlocked;
   string mChatPrevMessage;
   U32 mChatPrevMessageMode;
   bool checkMessage(const char *message, U32 mode);

   U32 mWrongPasswordCount;
   static const U32 MAX_WRONG_PASSWORD = 20;  // too many wrong password, and client get disconnect

   Vector<LevelInfo> mLevelInfos;

   static const S32 MASTER_SERVER_FAILURE_RETRY_TIME = 10000;   // 10 secs

   enum MessageColors
   {
      ColorWhite,
      ColorRed,
      ColorGreen,
      ColorBlue,
      ColorAqua,
      ColorYellow,
      ColorNuclearGreen,
      ColorCount              // Must be last
   };

   enum ParamType             // Be careful changing the order of this list... c2sSetParam() expects this for message creation
   {
      LevelChangePassword = 0,
      AdminPassword,
      ServerPassword,
      ServerName,
      ServerDescr,

      // Items not listed in c2sSetParam()::*keys[] should be added here
      LevelDir, 

      // Items not listed in c2sSetParam()::*types[] should be added here
      DeleteLevel,            

      ParamTypeCount       // Must be last
   };


#ifndef ZAP_DEDICATED
   GameConnection(ClientGame *game);      // Constructor for ClientGame
#endif
   GameConnection();                      // Constructor for ServerGame
   ~GameConnection();                     // Destructor


   // These from the DataSendable interface class
   TNL_DECLARE_RPC(s2rSendLine, (StringPtr line));
   TNL_DECLARE_RPC(s2rCommandComplete, (RangedU32<0,SENDER_STATUS_COUNT> status));


#ifndef ZAP_DEDICATED
   ClientGame *getClientGame();
   void setClientGame(ClientGame *game);
#endif

   Statistics mStatistics;       // Player statistics tracker

   Timer mSwitchTimer;           // Timer controlling when player can switch teams after an initial switch

   void setClientNameNonUnique(StringTableEntry name);
   void setServerName(StringTableEntry name);

   ClientInfo *getClientInfo();
   void setClientInfo(boost::shared_ptr<ClientInfo> clientInfo);


   LuaPlayerInfo *getPlayerInfo();

   bool lostContact();

   string getServerName();
   static string makeUnique(string name);    // Make sure a given name is unique across all clients & bots

   void reset();        // Clears/initializes some things between levels

   void submitAdminPassword(const char *password);
   void submitLevelChangePassword(string password);

   void suspendGame();
   void unsuspendGame();

   bool isBusy();
   void setIsBusy(bool busy);

   void sendLevelList();

   bool isReadyForRegularGhosts();
   void setReadyForRegularGhosts(bool ready);

   bool wantsScoreboardUpdates();
   void setWantsScoreboardUpdates(bool wantsUpdates);

   //S32 getScore() { return mScore; }
   //void setScore(S32 score) { mScore = score; }    // Called from ServerClientInfo

   //void addScore(S32 score) { mScore += score; }
   //void addToTotalScore(S32 score) { mTotalScore += score; mTotalCumulativeScore += score; }

   void addToTotalCumulativeScore(S32 score);

   F32 getCumulativeRating();
   //F32 getRating();

   //void setRating(F32 rating) { mRating = rating; }

   void endOfGameScoringHandler();

   Timer respawnTimer;


   virtual void onEndGhosting();    // Gets run when game is over


   // Tell UI we're waiting for password confirmation from server
   void setWaitingForPermissionsReply(bool waiting);
   bool waitingForPermissionsReply();

   // Tell UI whether we've recieved password confirmation from server
   void setGotPermissionsReply(bool gotReply);
   bool gotPermissionsReply();

   // Suspend/unsuspend game
   TNL_DECLARE_RPC(c2sSuspendGame, (bool suspend));
   TNL_DECLARE_RPC(s2cUnsuspend, ());

   TNL_DECLARE_RPC(c2sEngineerDeployObject, (RangedU32<0,EngineeredItemCount> type));      // Player using engineer module
   bool sEngineerDeployObject(U32 type);      // Player using engineer module, robots use this, bypassing the net interface. True if successful.

   // Chage passwords on the server
   void changeParam(const char *param, ParamType type);

   TNL_DECLARE_RPC(c2sAdminPassword, (StringPtr pass));
   TNL_DECLARE_RPC(c2sLevelChangePassword, (StringPtr pass));

   TNL_DECLARE_RPC(c2sSetAuthenticated, ());      // Tell server that the client is (or claims to be) authenticated

   TNL_DECLARE_RPC(c2sSetParam, (StringPtr param, RangedU32<0, ParamTypeCount> type));


   TNL_DECLARE_RPC(s2cSetIsAdmin, (bool granted));
   TNL_DECLARE_RPC(s2cSetIsLevelChanger, (bool granted, bool notify));

   TNL_DECLARE_RPC(s2cSetServerName, (StringTableEntry name));

   bool isInCommanderMap();

   TNL_DECLARE_RPC(c2sRequestCommanderMap, ());
   TNL_DECLARE_RPC(c2sReleaseCommanderMap, ());

   TNL_DECLARE_RPC(c2sRequestLoadout, (Vector<U32> loadout));     // Client has changed his loadout configuration
   void sRequestLoadout(Vector<U32> &loadout);                    // Robot has changed his loadout configuration

   TNL_DECLARE_RPC(s2cDisplayMessageESI, (RangedU32<0, ColorCount> color, RangedU32<0, NumSFXBuffers> sfx,
                   StringTableEntry formatString, Vector<StringTableEntry> e, Vector<StringPtr> s, Vector<S32> i));
   TNL_DECLARE_RPC(s2cDisplayMessageE, (RangedU32<0, ColorCount> color, RangedU32<0, NumSFXBuffers> sfx,
                   StringTableEntry formatString, Vector<StringTableEntry> e));
   TNL_DECLARE_RPC(s2cTouchdownScored, (U32 sfx, S32 team, StringTableEntry formatString, Vector<StringTableEntry> e));

   TNL_DECLARE_RPC(s2cDisplayMessage, (RangedU32<0, ColorCount> color, RangedU32<0, NumSFXBuffers> sfx, StringTableEntry formatString));
   TNL_DECLARE_RPC(s2cDisplayErrorMessage, (StringTableEntry formatString));    

   TNL_DECLARE_RPC(s2cDisplayMessageBox, (StringTableEntry title, StringTableEntry instr, Vector<StringTableEntry> message));

   TNL_DECLARE_RPC(s2cAddLevel, (StringTableEntry name, RangedU32<0, GameTypesCount> type));
   TNL_DECLARE_RPC(s2cRemoveLevel, (S32 index));

   TNL_DECLARE_RPC(c2sRequestLevelChange, (S32 newLevelIndex, bool isRelative));
   void c2sRequestLevelChange2(S32 newLevelIndex, bool isRelative);
   TNL_DECLARE_RPC(c2sRequestShutdown, (U16 time, StringPtr reason));
   TNL_DECLARE_RPC(c2sRequestCancelShutdown, ());
   TNL_DECLARE_RPC(s2cInitiateShutdown, (U16 time, StringTableEntry name, StringPtr reason, bool originator));
   TNL_DECLARE_RPC(s2cCancelShutdown, ());

   TNL_DECLARE_RPC(c2sSetIsBusy, (bool busy));

   TNL_DECLARE_RPC(c2sSetServerAlertVolume, (S8 vol));
   TNL_DECLARE_RPC(c2sRenameClient, (StringTableEntry newName));

   TNL_DECLARE_RPC(c2sRequestCurrentLevel, ());

   U8 mSendableFlags;
   ByteBuffer *mDataBuffer;

   TNL_DECLARE_RPC(s2rSendableFlags, (U8 flags));
   TNL_DECLARE_RPC(s2rSendDataParts, (U8 type, ByteBufferPtr data));
   bool s2rUploadFile(const char *filename, U8 type);

   //void setAuthenticated(bool isVerified);    // Client & Server... Runs on server after getting message from master, or on local connection
   void resetAuthenticationTimer();
   S32 getAuthenticationCounter();
   //bool isAuthenticated() { return mIsVerified; }
   void requestAuthenticationVerificationFromMaster();
   void updateAuthenticationTimer(U32 timeDelta);

   void displayMessageE(U32 color, U32 sfx, StringTableEntry formatString, Vector<StringTableEntry> e);

   const Vector<U32> &getLoadout();
   void writeConnectRequest(BitStream *stream);
   bool readConnectRequest(BitStream *stream, NetConnection::TerminationReason &reason);

   void onConnectionEstablished();

   void onConnectTerminated(TerminationReason r, const char *notUsed);

   void onConnectionTerminated(TerminationReason r, const char *string);

   TNL_DECLARE_NETCONNECTION(GameConnection);
};

LevelInfo getLevelInfo(char *level, S32 size);
void updateClientChangedName(ClientInfo *clientInfo, StringTableEntry);  


};

#endif

