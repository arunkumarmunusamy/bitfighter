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
#ifndef _HTFGAMETYPE_H_
#define _HTFGAMETYPE_H_

#include "gameType.h"


namespace Zap
{

class HTFGameType : public GameType
{
   typedef GameType Parent;
   static StringTableEntry aString;
   static StringTableEntry theString;

   Vector<GoalZone *> mZones;

   enum {
      ScoreTime = 5000,    // Time flag is in your zone to get points for your team
   };
public:
   HTFGameType() { /* nothing here */ }    // Constructor, such as it is

   bool isFlagGame() { return true; }

   // Server only
   void addFlag(FlagItem *flag);


   void addZone(GoalZone *zone);


   // Note -- neutral or enemy-to-all robots can't pick up the flag!!!  When we add robots, this may be important!!!
   void shipTouchFlag(Ship *theShip, FlagItem *theFlag);


   void itemDropped(Ship *ship, MoveItem *item);


   void shipTouchZone(Ship *s, GoalZone *z);


   void idle(GameObject::IdleCallPath path, U32 deltaT);

   // Same code as in retrieveGame, CTF
   void performProxyScopeQuery(GameObject *scopeObject, ClientInfo *clientInfo);

   void renderInterfaceOverlay(bool scoreboardVisible);

   GameTypes getGameType() const { return HTFGame; }
   const char *getGameTypeString() const { return "Hold the Flag"; }
   const char *getShortName() const { return "HTF"; }
   const char *getInstructionString() { return "Hold the flags at your capture zones!"; }
   bool isTeamGame() { return true; }
   bool canBeTeamGame() const { return true; }
   bool canBeIndividualGame() const { return false; }

   // What does a particular scoring event score?
   S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);

   TNL_DECLARE_CLASS(HTFGameType);
};

};

#endif
