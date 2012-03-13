#include <storage/AppFileInfo.h>
#include <storage/Path.h>
#include <storage/File.h>
#include <storage/FindDirectory.h>

#include "besupport/ConvertMessages.h"
#include "ShareApplication.h"

namespace beshare {

#define BESHARE_SETTINGS_FILE_NAME "beshare_settings"   
#define BESHARE_USER_KEY_FILE_NAME "beshare_user_key"   

ShareApplication :: ShareApplication(const char * optConnectTo) : BApplication(BESHARE_MIME_TYPE), _window(NULL), _optConnectTo(optConnectTo)
{
   // If there is a BeShare settings file in the current directory, use that.
   app_info appInfo;
   GetAppInfo(&appInfo);
   BEntry appEntry(&appInfo.ref);
   appEntry.GetParent(&appEntry);  // get the directory this executable is in
   BPath path(&appEntry);
   path.Append(BESHARE_SETTINGS_FILE_NAME);
   if ((_settingsFileEntry.SetTo(path.Path()) != B_NO_ERROR)||(_settingsFileEntry.Exists() == false))
   {
      // Otherwise, get the default settings path (whether it exists or not)
      if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_NO_ERROR)
      {
         path.Append(BESHARE_SETTINGS_FILE_NAME);
         (void) _settingsFileEntry.SetTo(path.Path());
      }
   }
}

ShareApplication :: ~ShareApplication()
{
   // empty
}

void ShareApplication :: SaveSettings(const BMessage & settings)
{
   BFile settingsFile(&_settingsFileEntry, B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
   if (settingsFile.InitCheck() == B_NO_ERROR) 
   {
      (void) settings.Flatten(&settingsFile);
      BAppFileInfo afi(&settingsFile);
      afi.SetType(BESHARE_MIME_TYPE);
   }
}

void ShareApplication :: RefsReceived(BMessage * msg)
{
   entry_ref ref; 
   if (msg->FindRef("refs", &ref) == B_NO_ERROR) _settingsFileEntry.SetTo(&ref);
}
  
void ShareApplication :: ReadyToRun()
{
   // Find (or auto-generate) our install_id 
   uint64 installID = 0;
   BPath sPath;
   if (find_directory(B_USER_SETTINGS_DIRECTORY, &sPath) == B_NO_ERROR)
   {
      sPath.Append(BESHARE_USER_KEY_FILE_NAME);
      BEntry e(sPath.Path());
      {
         // first attempt to read an ID
         BFile idFile(&e, B_READ_ONLY);
         BMessage installMsg;
         if ((idFile.InitCheck() == B_NO_ERROR)&&(installMsg.Unflatten(&idFile) == B_NO_ERROR)) (void) installMsg.FindInt64("installid", (int64*)&installID);
      }
      if (installID == 0)
      {
         // And if we didn't get a good one, generate one and write it out.
         const uint32 max32 = (uint32)-1;
#if __MWERKS__
         srand(time(NULL));
         installID = (((uint64)(rand()*max32))<<32)|((uint64)(rand()*max32));
#else
         srand48(time(NULL));
         installID = (((uint64)(drand48()*max32))<<32)|((uint64)(drand48()*max32));
#endif      
         BFile idFile(&e, B_WRITE_ONLY|B_CREATE_FILE);
         BMessage installMsg;
         installMsg.AddInt64("installid", installID);
         (void) installMsg.Flatten(&idFile);
      }
   }
   
   // Attempt to load settings...
   BFile settingsFile(&_settingsFileEntry, B_READ_ONLY);
   BMessage settings;
   if (settingsFile.InitCheck() == B_NO_ERROR) (void) settings.Unflatten(&settingsFile);
   _window = new ShareWindow(installID, settings, _optConnectTo);
   _window->ReadyToRun();
   _window->Show();
}

void ShareApplication :: DispatchMessage(BMessage * msg, BHandler * handler)
{
   if (msg->what == B_QUIT_REQUESTED)
   {
      if (msg->IsSourceRemote())
      {
         // Tell our window not to put up an 'are you sure' requester
         if ((_window)&&(_window->Lock()))
         {
            _window->SetEnableQuitRequester(false);
            _window->Unlock();
         }
      }
   }
   BApplication::DispatchMessage(msg, handler);
}

void ShareApplication:: MessageReceived(BMessage * msg)
{
   switch(msg->what)
   {
      case 'pane':
         if ((_window)&&(_window->Lock())) 
         {
            const char * posStr;
            for (int i=0; msg->FindString("pos", i, &posStr) == B_NO_ERROR; i++)
            {
               int pos = atoi(posStr);

               const char * whichStr;
               int which = (msg->FindString("which", i, &whichStr) == B_NO_ERROR) ? atoi(whichStr) : i;


               const char * dirStr;
               if (msg->FindString("dir", i, &dirStr) != B_NO_ERROR) dirStr = "";

               _window->SetSplit(which, pos, (strchr(posStr, '%') != NULL), dirStr[0]);
            }
            _window->Unlock();
         }
      break;

      case 'scrn':
         if ((_window)&&(_window->Lock())) 
         {
            const char * fileName;
            _window->DoScreenShot((msg->FindString("name", &fileName) == B_NO_ERROR) ? fileName : "", _window);
            _window->Unlock();
         }
      break;

      case 'halt':
         if ((_window)&&(_window->Lock())) 
         {
            _window->PauseAllUploads();
            _window->Unlock();
         }
      break;

      case 'resu':
         if ((_window)&&(_window->Lock())) 
         {
            _window->ResumeAllUploads();
            _window->Unlock();
         }
      break;

      case 'name':
      {
         const char * name;
         if ((_window)&&(msg->FindString("name", &name) == B_NO_ERROR)&&(_window->Lock())) 
         {
            _window->SetLocalUserName(name);
            _window->Unlock();
         }
      }
      break;

      case 'serv':
      {
         const char * server;
         if ((_window)&&(msg->FindString("server", &server) == B_NO_ERROR)&&(_window->Lock())) 
         {
            _window->SetServer(server);
            _window->Unlock();
         }
      }
      break;

      case 'quer':
      {
         const char * query;
         if ((_window)&&(msg->FindString("query", &query) == B_NO_ERROR)&&(_window->Lock())) 
         {
            _window->SetQuery(query);
            _window->Unlock();
         }
      }
      break;

      case 'stat':
      {
         const char * status;
         if ((_window)&&(msg->FindString("status", &status) == B_NO_ERROR)&&(_window->Lock())) 
         {
            _window->SetLocalUserStatus(status);
            _window->Unlock();
         }
      }
      break;

      case 'smsg':
      {
         BMessage bmsg;
         if (msg->FindMessage("message", &bmsg) == B_NO_ERROR)
         {
            MessageRef mmsg = GetMessageFromPool();
            if ((mmsg())&&(ConvertFromBMessage(bmsg, *mmsg()) == B_NO_ERROR)&&(_window->Lock()))
            {
               _window->SendMessageToServer(mmsg);
               _window->Unlock();
            }
         }
      }
      break;

      default:
         BApplication::MessageReceived(msg);
      break;
   }
}

};  // end namespace beshare
