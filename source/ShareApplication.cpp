
#include "ShareApplication.h"

#include <Alert.h>
#include <String.h>
#include <AppFileInfo.h>
#include <Path.h>
#include <File.h>
#include <FindDirectory.h>

#include <Roster.h>

#include "besupport/ConvertMessages.h"

namespace beshare {

#define BESHARE_SETTINGS_FILE_NAME "beshare_settings"	
#define BESHARE_USER_KEY_FILE_NAME "beshare_user_key"	

ShareApplication::ShareApplication(const char * optConnectTo) : 
	BApplication(BESHARE_MIME_TYPE), fWindow(NULL), fOptConnectTo(optConnectTo)
{
	// If there is a BeShare settings file in the current directory, use that.
	app_info appInfo;
	GetAppInfo(&appInfo);
	BEntry appEntry(&appInfo.ref);
	appEntry.GetParent(&appEntry);  // get the directory this executable is in
	BPath path(&appEntry);
	path.Append(BESHARE_SETTINGS_FILE_NAME);
	if ((fSettingsFileEntry.SetTo(path.Path()) != B_NO_ERROR) 
		|| (fSettingsFileEntry.Exists() == false)) {
		// Otherwise, get the default settings path (whether it exists or not)
		if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_NO_ERROR) {
			path.Append(BESHARE_SETTINGS_FILE_NAME);
			(void) fSettingsFileEntry.SetTo(path.Path());
		}
	}
}


ShareApplication::~ShareApplication()
{
	// empty
}


void
ShareApplication::SaveSettings(const BMessage & settings)
{
	BFile settingsFile(&fSettingsFileEntry, B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
	if (settingsFile.InitCheck() == B_NO_ERROR) {
		(void) settings.Flatten(&settingsFile);
		BAppFileInfo afi(&settingsFile);
		afi.SetType(BESHARE_MIME_TYPE);
	}
}


void
ShareApplication::RefsReceived(BMessage * msg)
{
	entry_ref ref; 
	if (msg->FindRef("refs", &ref) == B_NO_ERROR) 
			fSettingsFileEntry.SetTo(&ref);
}


void
ShareApplication::ReadyToRun()
{
	// Find (or auto-generate) our install_id 
	uint64 installID = 0;
	BPath sPath;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &sPath) == B_NO_ERROR) {
		sPath.Append(BESHARE_USER_KEY_FILE_NAME);
		BEntry e(sPath.Path());
		{
			// first attempt to read an ID
			BFile idFile(&e, B_READ_ONLY);
			BMessage installMsg;
			if ((idFile.InitCheck() == B_NO_ERROR) && 
				(installMsg.Unflatten(&idFile) == B_NO_ERROR)) 
				(void) installMsg.FindInt64("installid", (int64*)&installID);
		}

		if (installID == 0) {
			// And if we didn't get a good one, generate one and write it out.
			const uint32 max32 = (uint32)-1;
			srand48(time(NULL));
			installID = (((uint64)(drand48()*max32))<<32)|((uint64)(drand48()*max32));
			BFile idFile(&e, B_WRITE_ONLY|B_CREATE_FILE);
			BMessage installMsg;
			installMsg.AddInt64("installid", installID);
			(void) installMsg.Flatten(&idFile);
		}
	}
	
	// Attempt to load settings...
	BFile settingsFile(&fSettingsFileEntry, B_READ_ONLY);
	BMessage settings;
	
	if (settingsFile.InitCheck() == B_NO_ERROR)
		(void) settings.Unflatten(&settingsFile);
	
	fWindow = new ShareWindow(installID, settings, fOptConnectTo);
	fWindow->ReadyToRun();
	fWindow->Show();
}


void
ShareApplication::DispatchMessage(BMessage * msg, BHandler * handler)
{
	if (msg->what == B_QUIT_REQUESTED) {
		if (msg->IsSourceRemote()) {
			// Tell our window not to put up an 'are you sure' requester
			if ((fWindow)&&(fWindow->Lock())) {
				fWindow->SetEnableQuitRequester(false);
				fWindow->Unlock();
			}
		}
	}
	BApplication::DispatchMessage(msg, handler);
}


void
ShareApplication:: MessageReceived(BMessage * msg)
{
	switch(msg->what) {
	case 'pane':
		if ((fWindow)&&(fWindow->Lock())) {
			const char * posStr;
			for (int i = 0; msg->FindString("pos", i, &posStr) == B_NO_ERROR; i++) {
				int pos = atoi(posStr);

				const char * whichStr;
				
				int which = (msg->FindString("which", i, &whichStr) == B_NO_ERROR) ? atoi(whichStr) : i;

				const char * dirStr;
				
				if (msg->FindString("dir", i, &dirStr) != B_NO_ERROR)
					dirStr = "";

				fWindow->SetSplit(which, pos, (strchr(posStr, '%') != NULL), dirStr[0]);
			}
			fWindow->Unlock();
		}
	break;

	 case 'scrn':
		if ((fWindow)&&(fWindow->Lock())) {
			const char * fileName;
			fWindow->DoScreenShot((msg->FindString("name", &fileName) == B_NO_ERROR) ? fileName : "", fWindow);
			fWindow->Unlock();
		}
	break;

	case 'halt':
		if ((fWindow)&&(fWindow->Lock())) {
			fWindow->PauseAllUploads();
			fWindow->Unlock();
		}
	break;

	case 'resu':
		if ((fWindow)&&(fWindow->Lock())) {
			fWindow->ResumeAllUploads();
			fWindow->Unlock();
		}
	break;

	case 'name':
	{
		const char * name;
		if ((fWindow)&&(msg->FindString("name", &name) == B_NO_ERROR)&&(fWindow->Lock())) {
			fWindow->SetLocalUserName(name);
			fWindow->Unlock();
		}
	}
	break;

	case 'serv':
	{
		const char * server;
		if ((fWindow)&&(msg->FindString("server", &server) == B_NO_ERROR)&&(fWindow->Lock())) {
			fWindow->SetServer(server);
			fWindow->Unlock();
		}
	}
	break;

	case 'quer':
	{
		const char * query;
		if ((fWindow)&&(msg->FindString("query", &query) == B_NO_ERROR)&&(fWindow->Lock())) {
			fWindow->SetQuery(query);
			fWindow->Unlock();
		}
	}
	break;

	case 'stat':
	{
		const char * status;
		if ((fWindow)&&(msg->FindString("status", &status) == B_NO_ERROR)&&(fWindow->Lock())) {
			fWindow->SetLocalUserStatus(status);
			fWindow->Unlock();
		}
	}
	break;

	case 'smsg':
	{
		BMessage bmsg;
		if (msg->FindMessage("message", &bmsg) == B_NO_ERROR) {
			MessageRef mmsg = GetMessageFromPool();
			if ((mmsg())&&(ConvertFromBMessage(bmsg, *mmsg()) == B_NO_ERROR)&&(fWindow->Lock())) {
				fWindow->SendMessageToServer(mmsg);
				fWindow->Unlock();
			}
		}
	}
	break;

	default:
		BApplication::MessageReceived(msg);
	break;
	}
}

void
ShareApplication::AboutRequested()
{
	BString version;
	BString str("BeShare");
	str += "\nVersion ";
	str += VERSION_STRING;
	str += " — MUSCLE ";
	str += MUSCLE_VERSION_STRING;
	str += "\n\n";
	
	str += "Jeremy Friesner (jfriesne)\n";
	str += "Fredrik Modéen (modeenf)\n";
	str += "Augustin Cavalier (waddlesplash)\n";
	str += "Vitaliy Mikitchenko (vitvep)\n";

	BAlert *about = new BAlert("About", str.String(), "BeShare Page", "Development Page", "Okay");
	BTextView *v = about->TextView();
	if (v) {
		rgb_color red = {255, 0, 51, 255};
		rgb_color blue = {0, 102, 255, 255};

		v->SetStylable(true);
		char *text = (char*)v->Text();
		char *s = text;
		// set all Be in BeShare in blue and red
		while ((s = strstr(s, "BeShare")) != NULL) {
			int32 i = s - text;
			v->SetFontAndColor(i, i+1, NULL, 0, &blue);
			v->SetFontAndColor(i+1, i+2, NULL, 0, &red);
			s += 2;
		}
		// first text line 
		s = strchr(text, '\n');
		BFont font;
		v->GetFontAndColor(0, &font);
		font.SetSize(16);
		v->SetFontAndColor(0, s-text+1, &font, B_FONT_SIZE);
	};
	
	const char * url = NULL;
	switch(about->Go())
	{
		case 0: url = BESHARE_HOMEPAGE_URL;	break;
		case 1: url = BESHARE_SOURCE_URL; break;
	}
	if (url) be_roster->Launch("text/html", 1, (char**) &url);
}

};  // end namespace beshare
