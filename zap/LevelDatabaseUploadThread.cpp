#include "LevelDatabaseUploadThread.h"

#include "UIManager.h"
#include "HttpRequest.h"
#include "ClientGame.h"
#include "UIEditor.h"
#include "UIErrorMessage.h"
#include "ScreenShooter.h"
#include "stringUtils.h"

#include <sstream>

namespace Zap
{

const string LevelDatabaseUploadThread::UploadRequest = HttpRequest::LevelDatabaseBaseUrl + "/levels/upload";
const string LevelDatabaseUploadThread::UploadScreenshotFilename = "upload_screenshot";

LevelDatabaseUploadThread::LevelDatabaseUploadThread(ClientGame* game)
{
   mGame = game;
}


LevelDatabaseUploadThread::~LevelDatabaseUploadThread()
{
   // Do nothing
}


U32 LevelDatabaseUploadThread::run()
{
   EditorUserInterface* editor = mGame->getUIManager()->getUI<EditorUserInterface>();

   if(mGame->getLevelDatabaseId())
      editor->setSaveMessage("Updating Level...", true);
   else
      editor->setSaveMessage("Uploading New Level...", true);

   string fileData = readFile(joindir(mGame->getSettings()->getFolderManager()->screenshotDir, UploadScreenshotFilename + string(".png")));

   HttpRequest req(UploadRequest);
   req.setMethod(HttpRequest::PostMethod);
   req.setData("data[User][username]",      mGame->getPlayerName());
   req.setData("data[User][user_password]", mGame->getPlayerPassword());
   req.setData("data[Level][content]", editor->getLevelText());
   req.addFile("data[Level][screenshot]", UploadScreenshotFilename + string(".png"), (const U8*) fileData.c_str(), fileData.length());

   string levelgenFilename;
   levelgenFilename = mGame->getScriptName();

   if(levelgenFilename != "")
   {
      const string &levelgenContents = readFile(mGame->getSettings()->getFolderManager()->findLevelGenScript(levelgenFilename));
      req.setData("data[Level][levelgen]", levelgenContents);
   }

   if(!req.send())
   {
      editor->setSaveMessage(string("Error connecting to server"), false);

      delete this;
      return 0;
   }

   S32 responseCode = req.getResponseCode();
   if(responseCode != HttpRequest::OK && responseCode != HttpRequest::Found)
   {
      editor->showUploadErrorMessage(responseCode, req.getResponseBody());
      editor->clearSaveMessage();

      stringstream message;
      message << "Error " << responseCode << ": " << endl << req.getResponseBody() << endl;
      logprintf(LogConsumer::LogError, "%s",  message.str().c_str());

      delete this;
      return 0;
   }

   // The server responds with the DBID of the level we just uploaded
   mGame->setLevelDatabaseId(atoi(req.getResponseBody().c_str()));
   editor->saveLevel(false, false);
   editor->setSaveMessage("Uploaded successfully", true);

   delete this;
   return 0;
}

}
