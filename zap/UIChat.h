//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _UICHAT_H_
#define _UICHAT_H_


#include "UI.h"      // Parent class of ChatUserInterface
#include "Color.h"
#include "lineEditor.h"

#include "tnlNetStringTable.h"
#include "tnlNetBase.h"

#include <map>

namespace Zap
{

static const S32 IN_GAME_CHAT_DISPLAY_POS = 500;

class ClientGame;

class ChatMessage
{
public:
   ChatMessage();// Quickie constructor
   ChatMessage(string frm, string msg, Color col, bool isPriv, bool isSys);    // "Real" constructor
   virtual ~ChatMessage();

   Color color;      // Chat message colors
   string message;   // Hold chat messages
   string from;      // Hold corresponding nicks
   string time;      // Time message arrived
   bool isPrivate;   // Holds public/private status of message
   bool isSystem;    // Message from system?
};


///////////////////////////////////////
///////////////////////////////////////

// All our chat classes will inherit from this
class AbstractChat: RenderManager
{
private:
   static std::map<string, Color> mFromColors;       // Map nicknames to colors
   static U32 mColorPtr;
   static const S32 MESSAGES_TO_RETAIN = 80;         // Plenty for now... far too many, really
   static const S32 MESSAGE_OVERFLOW_SHIFT = 25;     // Number of characters to shift when typing a long message

   static U32 mMessageCount;
   ClientGame *mGame;

   Color getNextColor() const;                       // Get next available color for a new nick
   Color getColor(string name) const;


protected:
   // Message data
   static ChatMessage mMessages[MESSAGES_TO_RETAIN];
   LineEditor mLineEditor;

   U32 mChatCursorPos;                     // Where is cursor?

   ChatMessage getMessage(U32 index) const;
   U32 getMessageCount() const;
   bool composingMessage() const;

public:
   explicit AbstractChat(ClientGame *game);        // Constructor
   virtual ~AbstractChat();               // Destructor
   void newMessage(const string &from, const string &message, bool isPrivate, bool isSystem, bool fromSelf);   // Handle incoming msg

   void clearChat();                      // Clear message being composed
   virtual void issueChat();              // Send chat message

   void leaveLobbyChat();                 // Send msg to master telling them we're leaving chat

   void renderMessages(U32 yPos, U32 lineCountToDisplay) const;
   void renderMessageComposition(S32 ypos) const;   // Render outgoing chat message composition line

   void renderChatters(S32 xpos, S32 ypos) const;   // Render list of other people in chat room
   void deliverPrivateMessage(const char *sender, const char *message);

   // Handle players joining and leaving lobby chat
   bool isPlayerInLobbyChat(const StringTableEntry &playerNick);
   void setPlayersInLobbyChat(const Vector<StringTableEntry> &playerNicks);
   void playerJoinedLobbyChat(const StringTableEntry &playerNick);
   void playerLeftLobbyChat(const StringTableEntry &playerNick);


   // Sizes and other things to help with positioning
   static const S32 CHAT_FONT_SIZE = 14;      // Font size to display those messages
   static const S32 CHAT_TIME_FONT_SIZE = 8;  // Size of the timestamp
   static const S32 CHAT_FONT_MARGIN = 3;     // Vertical margin
   static const S32 CHAT_NAMELIST_SIZE = 11;  // Size of names of people in chatroom


   static Vector<StringTableEntry> mPlayersInLobbyChat;
};


///////////////////////////////////////
///////////////////////////////////////

class ChatUserInterface : public UserInterface, public AbstractChat
{
   typedef UserInterface Parent;
   typedef AbstractChat ChatParent;

private:
   Color mMenuSubTitleColor;

   virtual void renderHeader() const;
   //virtual void renderFooter();
   virtual void onLobbyChat();       // What to do if user presses [F5]
   bool mRenderUnderlyingUI;

public:
   explicit ChatUserInterface(ClientGame *game, UIManager *uiManager);  // Constructor
   virtual ~ChatUserInterface();                                        // Destructor

   // UI related
   void render() const;
   bool onKeyDown(InputCode inputCode);
   void onTextInput(char ascii);

   void onActivate();
   void onActivateLobbyMode();

   void onEscape();

   void idle(U32 timeDelta);

   void setRenderUnderlyingUI(bool render);
};


///////////////////////////////////////
///////////////////////////////////////

class SuspendedUserInterface : public ChatUserInterface
{
   typedef ChatUserInterface Parent;

private:
   void renderHeader() const;
   void onLobbyChat();                  // What to do if user presses [F5]

public:
   explicit SuspendedUserInterface(ClientGame *game, UIManager *uiManager);    // Constructor
   virtual ~SuspendedUserInterface();
};


};

#endif

