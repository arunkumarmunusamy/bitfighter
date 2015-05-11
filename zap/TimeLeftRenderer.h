//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _TIME_LEFT_RENDERER
#define _TIME_LEFT_RENDERER

#include "tnlTypes.h"
#include "Point.h"
#include "RenderManager.h"

using namespace TNL;

namespace Zap 
{

class GameType;
class Game;
class ClientGame;
class ScreenInfo;

namespace UI
{

class TimeLeftRenderer: RenderManager
{
private:
   ScreenInfo *mScreenInfo;

   S32 mLeadingPlayer;              // Player index of mClientInfos with highest score
   S32 mLeadingPlayerScore;         // Score of mLeadingPlayer
   S32 mSecondLeadingPlayer;        // Player index of mClientInfos with highest score
   S32 mSecondLeadingPlayerScore;   // Score of mLeadingPlayer

   Point renderTimeLeft      (const GameType *gameType, bool teamsLocked, bool render = true) const;  // Returns width and height
   S32 renderHeadlineScores  (const Game *game, S32 ypos) const;
   S32 renderTeamScores      (const GameType *gameType, S32 bottom, bool render) const;
   S32 renderIndividualScores(const GameType *gameType, S32 bottom, bool render) const;

public:
   static const S32 TimeLeftIndicatorMargin = 7;
   static const S32 TimeTextSize = 30;

   TimeLeftRenderer();     // Constructor

   void updateLeadingPlayerAndScore(const Game *game);

   Point render(const GameType *gameType, bool scoreboardVisible, bool teamsLocked, bool render) const;
};

} } // Nested namespace


#endif

