/*
 * LevelDatabaseRateThread.cpp
 *
 *  Created on: May 14, 2013
 *      Author: charon
 */

#include "LevelDatabaseRateThread.h"

#include "ClientGame.h"
#include "HttpRequest.h"

#include <sstream>

using namespace std;

namespace Zap
{

// Define statics
const string LevelDatabaseRateThread::LevelDatabaseRateUrl = "bitfighter.org/pleiades/levels/rate/";

const string LevelDatabaseRateThread::RatingStrings[] = {
#define LEVEL_RATING(a, strval) strval,
   LEVEL_RATINGS_TABLE
#undef LEVEL_RATING
};


LevelDatabaseRateThread::LevelDatabaseRateThread(ClientGame* game, LevelRating rating)
{
   mGame   = game;
   mRating = rating;
}


LevelDatabaseRateThread::~LevelDatabaseRateThread()
{
   // Do nothing
}


// Level needs a dbid to continue.  Returns true if things are ok, false if there is a problem.
// Static method
bool LevelDatabaseRateThread::checkDbid(ClientGame *game)
{
   if(game->getLevelDatabaseId() == NOT_IN_DATABASE)
   {
      game->displayErrorMessage("!!! Level ID not found -- Either level is not in the database, or needs to be redownloaded");
      return false;
   }

   return true;
}


U32 LevelDatabaseRateThread::run()
{
   if(!checkDbid(mGame))
   {
      delete this;
      return 1;
   }

   mGame->displaySuccessMessage("Rating level...");

   stringstream id;
   id << mGame->getLevelDatabaseId();

   HttpRequest req = HttpRequest(LevelDatabaseRateUrl + id.str() + "/" + RatingStrings[mRating]);
   req.setMethod(HttpRequest::PostMethod);
   req.setData("data[User][username]",      mGame->getPlayerName());
   req.setData("data[User][user_password]", mGame->getPlayerPassword());

   if(!req.send())
   {
      mGame->displayErrorMessage("!!! Error rating level: Cannot connect to server");

      delete this;
      return 0;
   }

   S32 responseCode = req.getResponseCode();
   if(responseCode != HttpRequest::OK && responseCode != HttpRequest::Found)
   {
      stringstream message("!!! Error rating level: ");
      message << responseCode;
      mGame->displayErrorMessage(req.getResponseBody().c_str());

      delete this;
      return 0;
   }

   mGame->displaySuccessMessage(req.getResponseBody().c_str());

   delete this;
   return 0;
}


} /* namespace Zap */
