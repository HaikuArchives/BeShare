
#include <Alert.h>
#include <Screen.h>
#include <PopUpMenu.h>
#include <File.h>
#include <Path.h>	 
#include <FindDirectory.h>	 
#include <NodeMonitor.h>	 
#include <Resources.h>	 
#include <Beep.h>
#include <BitmapStream.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>
#include <TranslatorFormats.h>

#include "CLVColumnLabelView.h"
#include "CLVColumn.h"
#include "SplitPane.h"

#include "util/StringTokenizer.h"
#include "util/Socket.h"
#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "message/Message.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/PathMatcher.h"
#include "util/NetworkUtilityFunctions.h"
#include "iogateway/PlainTextMessageIOGateway.h"

#include "ShareApplication.h"
#include "ShareStrings.h"
#include "ShareUtils.h"
#include "PrivateChatWindow.h"
#include "ShareWindow.h"
#include "UserListView.h"
#include "ResultsView.h"
#include "ShareNetClient.h"
#include "ShareFileTransfer.h"
#include "ShareColumn.h"
#include "ShareMIMEInfo.h"
#include "RemoteUserItem.h"
#include "RemoteFileItem.h"
#include "ColorPicker.h"



namespace beshare {

// Any servers in this list will *always* be added to the server menu on startup.
// Most servers need not be listed here, as the auto-server-updater-thingy will
// add them at run time based on the servers.txt file it downloads
static const char * _defaultServers[] = 
{
#ifdef DEBUG_BESHARE
	"localhost:2960",	// For develop..
#endif
	"beshare.TyComSystems.com",	// Minox's default server
};

// Any connection that hasn't transferred data in >= 5 minutes will be cut
#define MORIBUND_TIMEOUT_SECONDS (5*60)

// Window sizing constraints
#define MIN_WIDTH	308
#define MIN_HEIGHT	280
#define MAX_WIDTH	65535
#define MAX_HEIGHT	65535

// Default window position
#define WINDOW_START_X 30
#define WINDOW_START_Y 50		 
#define WINDOW_START_W 775
#define WINDOW_START_H 430		 

// Size constants for the non-resizable parts of the GUI
#define USER_LIST_WIDTH		125
#define STATUS_VIEW_WIDTH	490
#define UPPER_VIEW_HEIGHT	(fontHeight+7.0f)
#define CHAT_VIEW_HEIGHT	125
#define USER_ENTRY_WIDTH	175
#define USER_STATUS_WIDTH	125

#define SPECIAL_COLUMN_CHAR 0x01

#define FILE_NAME_COLUMN_NAME		str(STR_FILE_NAME_KEY)
#define FILE_OWNER_COLUMN_NAME	 	str(STR_USER_KEY)
#define FILE_SESSION_COLUMN_NAME	str(STR_SESSIONID_KEY)
#define FILE_OWNER_BANDWIDTH_NAME	str(STR_CONNECTION_KEY)
#define FILE_MODIFICATION_TIME_NAME str(STR_MODIFICATION_TIME)

#define DEFAULT_COLUMN_WIDTH 40.0f

static uint8 DefaultData[256] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x3F, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x3F, 0xFE, 0xFE, 0xFE, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x3F, 0xFE, 0xFE, 0x62, 0xFE, 0xFE, 0xFE, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0x00, 0xFE, 0xFE, 0xFE, 0xB0, 0x62, 0x62, 0x62, 0xFE, 0xFE, 0xFE, 0x00, 0x00, 0xFF,
	0xFF, 0x00, 0xFE, 0xFE, 0xFE, 0xB0, 0xB0, 0x89, 0xB0, 0x62, 0x62, 0x89, 0xFE, 0xFE, 0x00, 0x00,
	0x00, 0xFE, 0xFE, 0xFD, 0xFE, 0xFE, 0xB0, 0xB0, 0xB0, 0x89, 0xB0, 0xFE, 0xFE, 0x00, 0x00, 0xB0,
	0xB0, 0x00, 0x00, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xB0, 0xB0, 0xFD, 0xFD, 0x29, 0x89, 0x00, 0x00,
	0xFF, 0xB0, 0x00, 0x00, 0x00, 0xFD, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0x00, 0x89, 0x00, 0xB0, 0xB0,
	0xFF, 0xFF, 0xB0, 0x00, 0x89, 0x00, 0x00, 0xFA, 0xFA, 0xFA, 0x00, 0x00, 0x00, 0xB0, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xB0, 0x00, 0x62, 0x89, 0x00, 0xFA, 0x00, 0xB0, 0xB0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x00, 0x00, 0x00, 0x00, 0xB0, 0xB0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x00, 0xB0, 0xFF, 0xB0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const BRect defaultPrivateRect(100,300,500,475);

ShareWindow::ShareWindow(uint64 installID, BMessage & settingsMsg, const char * connectServer) 
	:
	ChatWindow(
		BRect(WINDOW_START_X, WINDOW_START_Y, WINDOW_START_X + WINDOW_START_W, WINDOW_START_Y + WINDOW_START_H)
		,"BeShare", B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, 0L),
	_queryEnabled(false),
	_isConnecting(false),
	_isConnected(false),
	_currentPage(0),
	_bytesShown(0LL),
	_netClient(NULL),
	_defaultBitmap(BRect(0,0,15,15),B_COLOR_8_BIT,DefaultData,false,false),
	_maxSimultaneousUploadSessions(5),
	_maxSimultaneousUploadSessionsPerUser(2),
	_maxSimultaneousDownloadSessions(5),
	_maxSimultaneousDownloadSessionsPerUser(2),
	_uploadBandwidth(0),
	_connectionReaper(NULL),
	_language(GetDefaultLanguageForLocale()),
	_languageSet(false),
	_lastPrivateMessageTarget(""),
	_messageWasSentToPrivateChatWindow(false),
	_queryInProgressRunner(NULL),
	_radarSweep(0.0f),
	_lastInProgress(false),
	_firstUserDefinedAttribute(true),
	_enableQuitRequester(true),
	_idle(false),
	_idleTimeoutMinutes(0),
	_lastInteractionAt(system_time()),
	_awayStatus(FACTORY_DEFAULT_USER_AWAY_STATUS),
	_idleSendPending(false),
	_showServerStatus(false),
	_installID(installID),
	_acceptThread(this),
	_checkServerListThread(this),
	_autoReconnectAttemptCount(0),
	_autoReconnectRunner(NULL),
	_maxDownloadRate(0),
	_maxUploadRate(0),
	_doubleBufferBitmap(NULL),
	_doubleBufferView(new BView(BRect(), NULL, B_FOLLOW_ALL_SIDES, 0)),
	_totalBytesUploaded(0),
	_totalBytesDownloaded(0),
	_compressionLevel(0),
	_dequeueCount(0)
{
	const float vMargin = 5.0f;
	const float hMargin = 5.0f;

	// Add our sounds to the Sounds prefs panel, if they aren't there already
	add_system_beep_event(SYSTEM_SOUND_USER_NAME_MENTIONED);
	add_system_beep_event(SYSTEM_SOUND_PRIVATE_MESSAGE_RECEIVED);
	add_system_beep_event(SYSTEM_SOUND_AUTOCOMPLETE_FAILURE);
	add_system_beep_event(SYSTEM_SOUND_DOWNLOAD_FINISHED);
	add_system_beep_event(SYSTEM_SOUND_UPLOAD_STARTED);
	add_system_beep_event(SYSTEM_SOUND_UPLOAD_FINISHED);
	add_system_beep_event(SYSTEM_SOUND_WATCHED_USER_SPEAKS);
	add_system_beep_event(SYSTEM_SOUND_PRIVATE_MESSAGE_WINDOW);
	add_system_beep_event(SYSTEM_SOUND_INACTIVE_CHAT_WINDOW_RECEIVED_TEXT);

	#if DEBUG_BESHARE
		//PrintLanguage();
	#endif
	
	font_height plainFontAttrs;
	be_plain_font->GetHeight(&plainFontAttrs);
	float fontHeight = plainFontAttrs.descent+plainFontAttrs.ascent+8.0f;

	BMessenger toMe(this);

	// load colors
	for (int32 i = 0; i < NUM_COLORS; i++) {
		rgb_color temp;
		if (RestoreColorFromMessage("colors", temp, settingsMsg, i) == B_NO_ERROR)
			SetColor(i, temp);
	}

	bool cce;
	SetCustomColorsEnabled((settingsMsg.FindBool("customcolors", &cce) == B_NO_ERROR) ? cce : true);
	
	(void) settingsMsg.FindInt32("maxdownloadrate", (int32*)&_maxDownloadRate);
	(void) settingsMsg.FindInt32("maxuploadrate",	(int32*)&_maxUploadRate);

	const char * windowTitle;
	if (settingsMsg.FindString("windowtitle", &windowTitle) == B_NO_ERROR)
		SetCustomWindowTitle(windowTitle);

	{
		int32 temp;
		if (settingsMsg.FindInt32("language", &temp) == B_NO_ERROR) {
			_language = temp;
			_languageSet = true;
		}
	}
	
	SetLanguage(_language);	// always call this!
	
	float tempFloat;
	if (settingsMsg.FindFloat("fontsize", &tempFloat) == B_NO_ERROR)
		SetFontSize(tempFloat);

	// Recall saved window position
	BRect pos;
	if (settingsMsg.FindRect("windowpos", &pos) == B_NO_ERROR) {
		// check to make sure the window isn't opening off-screen
		BRect screenBounds;
		{
			BScreen s;
			screenBounds = s.Frame();
		}
		
		if (pos.left > screenBounds.Width())
			pos.left = WINDOW_START_X;
		
		if (pos.top > screenBounds.Height())
			pos.top = WINDOW_START_Y;

		MoveTo(pos.left, pos.top);
		ResizeTo(pos.Width(), pos.Height());
	}

	// Recall positions of private windows
	BMessage privMsg;
	for (int p = 0; settingsMsg.FindMessage("privwindows", p, &privMsg) == B_NO_ERROR; p++)
		_privateChatInfos.AddTail(privMsg);

	const char * pat;
	if (settingsMsg.FindString("watchpattern", &pat) == B_NO_ERROR)
		_watchPattern = pat;
	
	if (settingsMsg.FindString("autoprivpattern", &pat) == B_NO_ERROR)
		_autoPrivPattern = pat;

	// Recall our "active columns" from the settings message
	BMessage columnsSubMessage;
	if (settingsMsg.FindMessage("columns", &columnsSubMessage) == B_NO_ERROR) {
		char* name;
		type_code type;
		int32 count;
		for (int32 i = 0; (columnsSubMessage.GetInfo(B_FLOAT_TYPE, i, &name, &type, &count) == B_NO_ERROR); i++) {
			float width;
			if (columnsSubMessage.FindFloat(name, &width) == B_NO_ERROR)
				_activeAttribs.Put(name, width);
		}
	} else {
		// Put default columns here (size, name, owner, ?)
		_activeAttribs.Put(FILE_NAME_COLUMN_NAME, 330.0f);
		_activeAttribs.Put(FILE_OWNER_COLUMN_NAME, 100.0f);
		_activeAttribs.Put("beshare:File Size", 60.0f);
	}

	// The directory to download files to
	(void)GetAppSubdir("downloads", _downloadsDir, true);

	// Set up our share-file server thread.	This thread is responsible for
	// accepting incoming connections from other BeShare clients and notifying
	// us so that we can start a file transfer to them.
	// Try a few "well-known" ports first, if we can't get any of them, dynamically allocate one
	{
		for (uint16 i = DEFAULT_LISTEN_PORT; i <= DEFAULT_LISTEN_PORT + LISTEN_PORT_RANGE; i++) {
			uint16 port = (i<DEFAULT_LISTEN_PORT+LISTEN_PORT_RANGE)?i:0; 
			if ((_acceptThread.SetPort(port) == B_NO_ERROR)&&(_acceptThread.StartInternalThread() == B_NO_ERROR))
				break;	// okay to go!
		}
	}

	// Recall any aliases
	const char * aliasName;
	const char * aliasValue;
	for (int i = 0; ((settingsMsg.FindString("aliasname", i, &aliasName) == B_NO_ERROR)
		&& (settingsMsg.FindString("aliasvalue", i, &aliasValue) == B_NO_ERROR)); i++) 
		_aliases.Put(aliasName, aliasValue);

	// Recall any bans we had in effect
	uint32 banIP;
	uint64 banUntil;
	for (int i=0; ((settingsMsg.FindInt32("banip", i, (int32*)&banIP) == B_NO_ERROR)
		&& (settingsMsg.FindInt64("banuntil", i, (int64*)&banUntil) == B_NO_ERROR)); i++)
		_bans.Put(banIP, banUntil);

	// set up the net client: this is what talks to the MUSCLE server for us
	(void)GetAppSubdir("shared", _shareDir, true);
	_netClient = new ShareNetClient(_shareDir, (_acceptThread.GetPort() > 0) ? (int32)_acceptThread.GetPort() : -1);
	AddHandler(_netClient);

	SetSizeLimits(MIN_WIDTH, MAX_WIDTH, MIN_HEIGHT, MAX_HEIGHT);

	_menuBar = new BMenuBar(BRect(), "Menu Bar");

	BMenu * fileMenu = new BMenu(str(STR_FILE));
	//BMenu * fileMenu = new BMenu(B_TRANSLATE("File"));
	fileMenu->AddItem(_connectMenuItem = new BMenuItem(str(STR_CONNECT_TO_SERVER), new BMessage(SHAREWINDOW_COMMAND_RECONNECT_TO_SERVER), shortcut(SHORTCUT_CONNECT)));
	fileMenu->AddItem(_disconnectMenuItem = new BMenuItem(str(STR_DISCONNECT), new BMessage(SHAREWINDOW_COMMAND_DISCONNECT_FROM_SERVER), shortcut(SHORTCUT_DISCONNECT), B_SHIFT_KEY));
	fileMenu->AddItem(new BSeparatorItem);

	fileMenu->AddItem(new BMenuItem(str(STR_OPEN_SHARED_FOLDER), new BMessage(SHAREWINDOW_COMMAND_OPEN_SHARED_FOLDER), shortcut(SHORTCUT_OPEN_SHARED_FOLDER)));
	fileMenu->AddItem(new BMenuItem(str(STR_OPEN_DOWNLOADS_FOLDER), new BMessage(SHAREWINDOW_COMMAND_OPEN_DOWNLOADS_FOLDER), shortcut(SHORTCUT_OPEN_DOWNLOADS_FOLDER)));
	fileMenu->AddItem(new BMenuItem(str(STR_OPEN_LOGS_FOLDER), new BMessage(SHAREWINDOW_COMMAND_OPEN_LOGS_FOLDER), shortcut(SHORTCUT_OPEN_LOGS_FOLDER)));
	fileMenu->AddItem(new BSeparatorItem);


	fileMenu->AddItem(new BMenuItem(str(STR_OPEN_PRIVATE_CHAT_WINDOW), new BMessage(SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW), shortcut(SHORTCUT_OPEN_PRIVATE_CHAT_WINDOW)));		 

	fileMenu->AddItem(new BMenuItem(str(STR_CLEAR_CHAT_LOG), new BMessage(SHAREWINDOW_COMMAND_CLEAR_CHAT_LOG), shortcut(SHORTCUT_CLEAR_CHAT_LOG)));
	fileMenu->AddItem(new BMenuItem(str(STR_RESET_LAYOUT), new BMessage(SHAREWINDOW_COMMAND_RESET_LAYOUT), shortcut(SHORTCUT_RESET_LAYOUT)));
	fileMenu->AddItem(new BSeparatorItem);
	fileMenu->AddItem(new BMenuItem(str(STR_ABOUT_BESHARE), new BMessage(SHAREWINDOW_COMMAND_ABOUT), 0));
	fileMenu->AddItem(new BSeparatorItem);
	fileMenu->AddItem(new BMenuItem(str(STR_QUIT), new BMessage(B_QUIT_REQUESTED), shortcut(SHORTCUT_QUIT)));
	_menuBar->AddItem(fileMenu);

	_attribMenu = new BMenu(str(STR_ATTRIBUTES));
	_menuBar->AddItem(_attribMenu);

	// Add save/restore presets submenus to attributes menu
	{
		for (uint32 pr=0; pr<ARRAYITEMS(_attribPresets); pr++) if (settingsMsg.FindMessage("attributepresets", pr, &_attribPresets[pr]) != B_NO_ERROR) _attribPresets[pr].what = 0;
		BMenu * savePresets = new BMenu(str(STR_SAVE_PRESET));
		BMenu * restorePresets = new BMenu(str(STR_RESTORE_PRESET));
		for (uint32 ps = 1; ps < 1 + ARRAYITEMS(_attribPresets); ps++) {
			int which = ps % ARRAYITEMS(_attribPresets);
			savePresets->AddItem(CreatePresetItem(SHAREWINDOW_COMMAND_SAVE_ATTRIBUTE_PRESET, which, true, true));
			restorePresets->AddItem(_restorePresets[which] = CreatePresetItem(SHAREWINDOW_COMMAND_RESTORE_ATTRIBUTE_PRESET, which, (_attribPresets[which].what > 0), false));
		}
		
		_attribMenu->AddItem(savePresets);
		_attribMenu->AddItem(restorePresets);
		_attribMenu->AddSeparatorItem();
	}

	BMenu * settingsMenu = new BMenu(str(STR_SETTINGS));
	_menuBar->AddItem(settingsMenu);

	bool fw;
	if ((settingsMsg.FindBool("firewalled", &fw) == B_NO_ERROR)&&(fw))
		_netClient->SetFirewalled(true);

	settingsMenu->AddItem(MakeLimitSubmenu(settingsMsg, SHAREWINDOW_COMMAND_SET_UPLOAD_LIMIT, str(STR_MAX_SIMULTANEOUS_UPLOADS), "uploads", _maxSimultaneousUploadSessions));
	settingsMenu->AddItem(MakeLimitSubmenu(settingsMsg, SHAREWINDOW_COMMAND_SET_UPLOAD_PER_USER_LIMIT, str(STR_MAX_SIMULTANEOUS_UPLOADS_PER_USER), "uploadsperuser", _maxSimultaneousUploadSessionsPerUser));
	settingsMenu->AddItem(MakeLimitSubmenu(settingsMsg, SHAREWINDOW_COMMAND_SET_DOWNLOAD_LIMIT, str(STR_MAX_SIMULTANEOUS_DOWNLOADS), "downloads", _maxSimultaneousDownloadSessions));
	settingsMenu->AddItem(MakeLimitSubmenu(settingsMsg, SHAREWINDOW_COMMAND_SET_DOWNLOAD_PER_USER_LIMIT, str(STR_MAX_SIMULTANEOUS_DOWNLOADS_PER_USER), "downloadsperuser", _maxSimultaneousDownloadSessionsPerUser));

	BMenu * bMenu = new BMenu(str(STR_UPLOAD_BANDWIDTH));
	bMenu->SetRadioMode(true);
	settingsMenu->AddItem(bMenu);

	int32 bw;
	if (settingsMsg.FindInt32("bandwidth", &bw) == B_NO_ERROR)
		_uploadBandwidth = bw;
	
	AddBandwidthOption(bMenu, "300 baud",			75);
	AddBandwidthOption(bMenu, "14.4 kbps",		14400);
	AddBandwidthOption(bMenu, "28.8 kbps",		28800);
	AddBandwidthOption(bMenu, "33.6 kbps",		33600);
	AddBandwidthOption(bMenu, "57.6 kbps",		57600);
	AddBandwidthOption(bMenu, "ISDN-64k",		64000);
	AddBandwidthOption(bMenu, "ISDN-128k",	 128000);
	AddBandwidthOption(bMenu, "DSL",			384000);
	AddBandwidthOption(bMenu, "Cable",		 768000);
	AddBandwidthOption(bMenu, "T1",			1500000);
	AddBandwidthOption(bMenu, "T3",			4500000);
	AddBandwidthOption(bMenu, "OC-3",		3*51840000);
	AddBandwidthOption(bMenu, "OC-12",	 12*51840000);

	bool lu;

	BMenu * filterMenus[NUM_DESTINATIONS] = {new BMenu(str(STR_DISPLAY)), new BMenu(str(STR_LOG))};
	int filterLabels[NUM_FILTERS] = {STR_TIMESTAMPS, STR_USER_EVENTS, STR_UPLOADS, STR_CHAT_NOUN, STR_PRIVATE_MESSAGES, STR_INFO_MESSAGES, STR_WARNING_MESSAGES, STR_ERROR_MESSAGES, STR_USER_NUMBER};

	_toggleFileLogging = new BMenuItem(str(STR_LOGGING_ENABLED), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_FILE_LOGGING), shortcut(SHORTCUT_TOGGLE_FILE_LOGGING));
	_toggleFileLogging->SetMarked((settingsMsg.FindBool("filelogging", &lu) == B_NO_ERROR) ? lu : false);
	filterMenus[DESTINATION_LOG_FILE]->AddItem(_toggleFileLogging);
	filterMenus[DESTINATION_LOG_FILE]->AddSeparatorItem();
 
	for (int f = 0; f < NUM_DESTINATIONS; f++) {
		char filterSaveName[32];
		sprintf(filterSaveName, "chatfilter%i", f);

	 	for (int g = 0; g < NUM_FILTERS; g++) {
			BMenuItem * mi = _filterItems[f][g] = new BMenuItem(str(filterLabels[g]), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_CHAT_FILTER));
			mi->SetMarked((settingsMsg.FindBool(filterSaveName, g, &lu) == B_NO_ERROR) ? lu : ((f != DESTINATION_DISPLAY)||(g != FILTER_TIMESTAMPS)));
			filterMenus[f]->AddItem(mi);
	 	}
		
		settingsMenu->AddItem(filterMenus[f]);
	}

	BMenu * langMenu = new BMenu(str(STR_LANGUAGE));
	langMenu->SetRadioMode(true);
	settingsMenu->AddItem(langMenu);

	for (int l = 0; l < NUM_LANGUAGES; l++) {
		BMessage * lmsg = new BMessage(SHAREWINDOW_COMMAND_SELECT_LANGUAGE);
		lmsg->AddInt32("language", l);
		BMenuItem * nextLMenu = new BMenuItem(GetLanguageName(l, true), lmsg);
		
		if (l == _language)
			nextLMenu->SetMarked(true);
		
		langMenu->AddItem(nextLMenu);
	}

	{
		uint32 ps;
		_pageSize = (settingsMsg.FindInt32("pagesize", (int32*)&ps) == B_NO_ERROR) ? ps : 1000;
		BMenu * pageSizeMenu = new BMenu(str(STR_RESULTS_PER_PAGE));
		pageSizeMenu->SetRadioMode(true);
		uint32 pageSizes[] = {500, 1000, 2000, 3000, 5000, 8000, 10000, 100000};
		
		for (size_t p = 0; p < ARRAYITEMS(pageSizes); p++) {
			ps = pageSizes[p];
			BMessage * pmsg = new BMessage(SHAREWINDOW_COMMAND_SET_PAGE_SIZE);
			pmsg->AddInt32("pagesize", ps);
			char temp[64];
			
			if (ps >= 1000)
				sprintf(temp, "%lu,000", ps/1000);
			else
				sprintf(temp, "%lu",	 ps);
		
			BMenuItem * nextPMenu = new BMenuItem(temp, pmsg);
		
			if (ps == _pageSize)
				nextPMenu->SetMarked(true);
		
			pageSizeMenu->AddItem(nextPMenu);
		}
	
		settingsMenu->AddItem(pageSizeMenu);
	}

	{
		const char * awayStatus;
		if (settingsMsg.FindString("awaystatus", &awayStatus) == B_NO_ERROR)
			_awayStatus = awayStatus;

		uint32 away;
		_idleTimeoutMinutes = (settingsMsg.FindInt32("autoaway", (int32*)&away) == B_NO_ERROR) ? away : 0;
		BMenu * autoAwayMenu = new BMenu(str(STR_AUTO_AWAY));
		autoAwayMenu->SetRadioMode(true);
		uint32 awayTimes[] = {0, 2, 5, 10, 15, 20, 30, 60, 120};
		for (size_t p = 0; p < ARRAYITEMS(awayTimes); p++) {
			away = awayTimes[p];
			BMessage * amsg = new BMessage(SHAREWINDOW_COMMAND_SET_AUTO_AWAY);
			amsg->AddInt32("autoaway", away);
			
			char temp[64];
			if (away > 0)
				sprintf(temp, "%lu %s", away, str(STR_MINUTES));
			else
				strcpy(temp, str(STR_DISABLED));
		
			BMenuItem * nextAMenu = new BMenuItem(temp, amsg);
			
			if (away == _idleTimeoutMinutes)
				nextAMenu->SetMarked(true);
			
			autoAwayMenu->AddItem(nextAMenu);
		}
		settingsMenu->AddItem(autoAwayMenu);
	}

	{
		int32 compLevel;
		
		if (settingsMsg.FindInt32("complevel", &compLevel) == B_NO_ERROR)
			_compressionLevel = compLevel;

		BMenu * dataCompMenu = new BMenu(str(STR_DATA_COMPRESSION));
		dataCompMenu->SetRadioMode(true);
		const uint32 compLevels[] = {			0,		3,		 6,		9};
		const int	compLabels[] = {STR_DISABLED, STR_LOW, STR_MEDIUM, STR_HIGH};
		for (size_t i = 0; i < ARRAYITEMS(compLevels); i++) {
			BMessage * cmsg = new BMessage(SHAREWINDOW_COMMAND_SET_COMPRESSION_LEVEL);
			cmsg->AddInt32("complevel", compLevels[i]);
			BMenuItem * nextCMenu = new BMenuItem(str(compLabels[i]), cmsg);
		
			if (compLevels[i] == _compressionLevel)
				nextCMenu->SetMarked(true);
			
			dataCompMenu->AddItem(nextCMenu);
	 	}
		settingsMenu->AddItem(dataCompMenu);
	}

	const char * onLogin;
	for (int ol=0; settingsMsg.FindString("onlogin", ol, &onLogin) == B_NO_ERROR; ol++)
		_onLoginStrings.AddTail(onLogin);

	_fullUserQueries = new BMenuItem(str(STR_FULL_USER_QUERIES), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_FULL_USER_QUERIES), shortcut(SHORTCUT_FULL_USER_QUERIES));
	_fullUserQueries->SetMarked((settingsMsg.FindBool("fulluserqueries", &lu) == B_NO_ERROR) ? lu : true); 
	settingsMenu->AddItem(_fullUserQueries);

	_sharingEnabled = new BMenuItem(str(STR_FILE_SHARING_ENABLED), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_FILE_SHARING_ENABLED), shortcut(SHORTCUT_FILE_SHARING_ENABLED));
	_sharingEnabled->SetMarked((settingsMsg.FindBool("filesharingenabled", &lu) != B_NO_ERROR)||(lu));
	settingsMenu->AddItem(_sharingEnabled);

	_shortestUploadsFirst = new BMenuItem(str(STR_SHORTEST_UPLOADS_FIRST), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_SHORTEST_UPLOADS_FIRST));
	_shortestUploadsFirst->SetMarked((settingsMsg.FindBool("shortestfirst", &lu) == B_NO_ERROR) ? lu : true); 
	settingsMenu->AddItem(_shortestUploadsFirst);

	_autoClearCompletedDownloads = new BMenuItem(str(STR_AUTOCLEAR_COMPLETED_DOWNLOADS), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_AUTOCLEAR_COMPLETED_DOWNLOADS), 0);
	if (settingsMsg.FindBool("autoclear", &lu) == B_NO_ERROR)
		_autoClearCompletedDownloads->SetMarked(lu);
	settingsMenu->AddItem(_autoClearCompletedDownloads);

	_retainFilePaths = new BMenuItem(str(STR_RETAIN_FILE_PATHS), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_RETAIN_FILE_PATHS), 0);
	if (settingsMsg.FindBool("retainfilepaths", &lu) == B_NO_ERROR)
		_retainFilePaths->SetMarked(lu);
	settingsMenu->AddItem(_retainFilePaths);

	_loginOnStartup = new BMenuItem(str(STR_LOGIN_ON_STARTUP), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_LOGIN_ON_STARTUP), 0);
	if (settingsMsg.FindBool("loginonstartup", &lu) == B_NO_ERROR)
		_loginOnStartup->SetMarked(lu);
	settingsMenu->AddItem(_loginOnStartup);

	String setColorsString = str(STR_SET_COLORS); setColorsString += B_UTF8_ELLIPSIS;
	_colorItem = new BMenuItem(setColorsString(), new BMessage(SHAREWINDOW_COMMAND_SHOW_COLOR_PICKER));
	settingsMenu->AddItem(_colorItem);
	
	settingsMenu->AddItem(new BSeparatorItem);

	_autoUpdateServers = new BMenuItem(str(STR_AUTOUPDATE_SERVER_LIST), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_AUTOUPDATE_SERVER_LIST), 0);
	if (settingsMsg.FindBool("autoupdateservers", &lu) != B_NO_ERROR)
		lu = true;
	_autoUpdateServers->SetMarked(lu);
	settingsMenu->AddItem(_autoUpdateServers);

	_firewalled = new BMenuItem(str(STR_IM_FIREWALLED), new BMessage(SHAREWINDOW_COMMAND_TOGGLE_FIREWALLED));

	settingsMenu->AddItem(_firewalled);

	AddChild(_menuBar);						
	BRect contentBounds = Bounds();
	contentBounds.top = _menuBar->Bounds().Height();

	// Create group/area views (top level stuff)
	BView * contentView = new BView(contentBounds, "ContentView", B_FOLLOW_ALL_SIDES, 0);
	AddBorderView(contentView);
	AddChild(contentView);
	{
		// Fill out the upperLevel view
		BRect upperViewBounds(0, 0, contentBounds.Width() - hMargin, UPPER_VIEW_HEIGHT);
		BView * upperView = new BView(upperViewBounds, "UpperView", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, 0);
		AddBorderView(upperView);
		contentView->AddChild(upperView);
		{
			// Fill out the query view
			BRect queryViewBounds(0, 0, upperViewBounds.Width()-STATUS_VIEW_WIDTH, upperViewBounds.Height());
			_queryView = new BView(queryViewBounds, NULL, B_FOLLOW_ALL_SIDES, 0);
			AddBorderView(_queryView);
			upperView->AddChild(_queryView);
		
			const char * q = str(STR_QUERY);
			_queryMenu = new BMenu(q);
			float qw = _queryView->StringWidth(q)+36.0f;
			_queryView->AddChild(AddBorderView(new BMenuField(BRect(hMargin,4,qw,fontHeight), NULL, NULL, _queryMenu)));
		
			float right = queryViewBounds.Width()-hMargin;
			float stringWidth = _queryMenu->StringWidth(str(STR_STOP_QUERY))+10.0f;
			_disableQueryButton = new BButton(BRect(right-stringWidth,3,right,fontHeight), NULL, str(STR_STOP_QUERY), new BMessage(SHAREWINDOW_COMMAND_DISABLE_QUERY), B_FOLLOW_RIGHT | B_FOLLOW_TOP);
			AddBorderView(_disableQueryButton);
			_queryView->AddChild(_disableQueryButton);
			right -= (stringWidth + hMargin);

			stringWidth = _queryMenu->StringWidth(str(STR_START_QUERY))+10.0f;
			_enableQueryButton = new BButton(BRect(right-stringWidth,3,right,fontHeight), NULL, str(STR_START_QUERY), new BMessage(SHAREWINDOW_COMMAND_ENABLE_QUERY), B_FOLLOW_RIGHT | B_FOLLOW_TOP);
			AddBorderView(_enableQueryButton);
			_queryView->AddChild(_enableQueryButton);
			right -= (stringWidth + hMargin);

			const char * startupQuery;
			if (settingsMsg.FindString("query", &startupQuery) != B_NO_ERROR)
				startupQuery = "*.mp3";
			_fileNameQueryEntry = new BTextControl(BRect(qw-10.0f,6,right,fontHeight), NULL, NULL, startupQuery, new BMessage(SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY), B_FOLLOW_ALL_SIDES);
			AddBorderView(_fileNameQueryEntry);
			_fileNameQueryEntry->SetTarget(toMe);
			_queryView->AddChild(_fileNameQueryEntry);
			// Restore any additional strings....
			const char * listQuery;
			for (int qh = 1; (settingsMsg.FindString("query", qh, &listQuery) == B_NO_ERROR); qh++) {
				BMessage * msg = new BMessage(SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY);
				msg->AddString("query", listQuery);
				_queryMenu->AddItem(new BMenuItem(listQuery, msg));
			}
	 	}

	 	{
			// Fill out the status view
			BRect statusViewBounds(upperViewBounds.Width()-STATUS_VIEW_WIDTH, 0, upperViewBounds.Width(), upperViewBounds.Height());
			_statusView = new BView(statusViewBounds, NULL, B_FOLLOW_RIGHT| B_FOLLOW_TOP_BOTTOM, 0);
			AddBorderView(_statusView);
			upperView->AddChild(_statusView);
			{
				// Fill out the Server menu and text control
				float serverMenuWidth = _queryMenu->StringWidth(str(STR_SERVER))+25.0f;
				_serverMenu = new BMenu(str(STR_SERVER));
				_statusView->AddChild(AddBorderView(_serverMenuField = new BMenuField(BRect(0,4,serverMenuWidth,statusViewBounds.Height()), NULL, NULL, _serverMenu)));

				const char * firstName = NULL;
				const char * sn = NULL;
				for (int i = 0; (settingsMsg.FindString("serverlist", i, &sn) == B_NO_ERROR); i++) {
					if (firstName == NULL)
						firstName = sn;
					AddServerItem(sn, true, -1);
				}

				// Add any default servers that aren't in the list already
				if (firstName == NULL)
					firstName = _defaultServers[0];
			
				for (uint32 j=0; j<ARRAYITEMS(_defaultServers); j++)
					AddServerItem(_defaultServers[j], true, (j==0)?0:1);

				if (settingsMsg.FindString("server", &sn) == B_NO_ERROR)
					firstName = sn;
			
				_serverEntry = new BTextControl(BRect(serverMenuWidth, 6, STATUS_VIEW_WIDTH-(USER_ENTRY_WIDTH+USER_STATUS_WIDTH+hMargin), statusViewBounds.Height()), NULL, NULL, firstName, new BMessage(SHAREWINDOW_COMMAND_USER_CHANGED_SERVER));
				AddBorderView(_serverEntry);
				_serverEntry->SetTarget(toMe);
				_serverEntry->SetDivider(0.0f);
				_statusView->AddChild(_serverEntry);	
			}

			{
				// Fill out the UserName menu and text control
				float userNameMenuWidth = _queryMenu->StringWidth(str(STR_USER_NAME_COLON))+25.0f;
				float userNameMenuLeft = STATUS_VIEW_WIDTH-(USER_ENTRY_WIDTH+USER_STATUS_WIDTH);
				_userNameMenu = new BMenu(str(STR_USER_NAME_COLON));
				_statusView->AddChild(AddBorderView(new BMenuField(BRect(userNameMenuLeft,4,userNameMenuLeft+userNameMenuWidth,statusViewBounds.Height()), NULL, NULL, _userNameMenu)));

				const char * un = NULL;
				const char * first = NULL;
				for (int i = 0; (settingsMsg.FindString("usernamelist", i, &un) == B_NO_ERROR); i++) {
					if (first == NULL)
						first = un;
				
					AddUserNameItem(un);
				}

				if (settingsMsg.FindString("username", &un) != B_NO_ERROR)
					un = first ? first : FACTORY_DEFAULT_USER_NAME;
			
				_netClient->SetLocalUserName(un);

				_userNameEntry = new BTextControl(BRect(userNameMenuLeft+userNameMenuWidth,6,STATUS_VIEW_WIDTH-(USER_STATUS_WIDTH+1),statusViewBounds.Height()), NULL, NULL, un, new BMessage(SHAREWINDOW_COMMAND_USER_CHANGED_NAME));
				AddBorderView(_userNameEntry);
				_userNameEntry->SetTarget(toMe);
		
				_statusView->AddChild(_userNameEntry);
			}

			{
				// Fill out the UserStatus menu and text control
				String statusColon = str(STR_STATUS);
				statusColon += ':';
				float userStatusMenuWidth = _queryMenu->StringWidth(statusColon())+25.0f;
				float userStatusMenuLeft = hMargin+(STATUS_VIEW_WIDTH-USER_STATUS_WIDTH);
				_userStatusMenu = new BMenu(statusColon());
				_statusView->AddChild(AddBorderView(new BMenuField(BRect(userStatusMenuLeft,4,userStatusMenuLeft+userStatusMenuWidth,statusViewBounds.Height()), NULL, NULL, _userStatusMenu)));

				const char * us = NULL;
				const char * first = NULL;
				for (int i = 0; (settingsMsg.FindString("userstatuslist", i, &us) == B_NO_ERROR); i++) {
					if (first == NULL)
						first = us;
					AddUserStatusItem(us);
				}
			
				if (_userStatusMenu->CountItems() == 0) {	
					AddUserStatusItem(FACTORY_DEFAULT_USER_STATUS);
					AddUserStatusItem(FACTORY_DEFAULT_USER_AWAY_STATUS);
				}

				if (settingsMsg.FindString("userstatus", &us) != B_NO_ERROR)
					us = first ? first : FACTORY_DEFAULT_USER_STATUS;
			
				_netClient->SetLocalUserStatus(us);

				_userStatusEntry = new BTextControl(BRect(userStatusMenuLeft+userStatusMenuWidth,6,STATUS_VIEW_WIDTH-1,statusViewBounds.Height()), NULL, NULL, us, new BMessage(SHAREWINDOW_COMMAND_USER_CHANGED_STATUS));
				AddBorderView(_userStatusEntry);
				_userStatusEntry->SetTarget(toMe);
		
				_statusView->AddChild(_userStatusEntry);
			}
		}
	}
	
	BRect middleBounds(2, UPPER_VIEW_HEIGHT, contentBounds.Width()-hMargin, contentBounds.Height()-CHAT_VIEW_HEIGHT);

	BRect resultsBounds(0, 0, middleBounds.Width()-(USER_LIST_WIDTH+hMargin), middleBounds.Height());
	BView * resultsView = new BView(resultsBounds, "IOView", B_FOLLOW_ALL_SIDES, 0);
	AddBorderView(resultsView);

	CLVContainerView* resultsContainerView;
	_resultsView = new ResultsView(SHAREWINDOW_COMMAND_SWITCH_TO_PAGE, BRect(hMargin, vMargin, resultsView->Bounds().Width()-(B_V_SCROLL_BAR_WIDTH+2), resultsView->Bounds().Height()-(vMargin+fontHeight+B_H_SCROLL_BAR_HEIGHT+8)),&resultsContainerView, NULL, B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE,B_MULTIPLE_SELECTION_LIST,false,true,true,true,B_FANCY_BORDER);
	AddBorderView(resultsContainerView);
	_resultsView->SetSortFunction((CLVCompareFuncPtr) CompareFunc);
	_resultsView->SetTarget(toMe);
	_resultsView->SetSelectionMessage(new BMessage(SHAREWINDOW_COMMAND_RESULT_SELECTION_CHANGED));
	_resultsView->SetInvocationMessage(new BMessage(SHAREWINDOW_COMMAND_BEGIN_DOWNLOADS));
	resultsView->AddChild(resultsContainerView);

	_resultsView->AddColumn(new CLVColumn("", 20.0f, CLV_LOCK_AT_BEGINNING | CLV_NOT_MOVABLE | CLV_NOT_RESIZABLE));

	const float pageButtonWidth = 25.0f;
	BView * dlButtonView = new BView(BRect(resultsBounds.left+2+pageButtonWidth, resultsView->Bounds().Height()-(fontHeight+vMargin+2),resultsBounds.right-(2+pageButtonWidth),resultsBounds.Height()-2), NULL, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM, 0);
	AddBorderView(dlButtonView);
	resultsView->AddChild(dlButtonView);

	//Download button
	float clearButtonWidth = dlButtonView->StringWidth(str(STR_CLEAR_FINISHED_FAILED_TRANSFERS))+20.0f;
	BRect dlBounds = dlButtonView->Bounds();
	_requestDownloadsButton = new BButton(BRect(0, 0, dlBounds.Width()-(clearButtonWidth+hMargin), dlBounds.Height()), NULL, str(STR_DOWNLOAD_SELECTED_FILES), new BMessage(SHAREWINDOW_COMMAND_BEGIN_DOWNLOADS), B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_NAVIGABLE|B_FULL_UPDATE_ON_RESIZE);
	AddBorderView(_requestDownloadsButton);
	dlButtonView->AddChild(_requestDownloadsButton);

	_clearFinishedDownloadsButton = new BButton(BRect(dlBounds.Width()-clearButtonWidth, 0, dlBounds.Width(), dlBounds.Height()), NULL, str(STR_CLEAR_FINISHED_FAILED_TRANSFERS), new BMessage(SHAREWINDOW_COMMAND_CLEAR_FINISHED_DOWNLOADS), B_FOLLOW_RIGHT | B_FOLLOW_TOP_BOTTOM);
	AddBorderView(_clearFinishedDownloadsButton);
	dlButtonView->AddChild(_clearFinishedDownloadsButton);

	BRect dlFrame = dlButtonView->Frame();
	_prevPageButton = new BButton(BRect(resultsBounds.left, dlFrame.top, dlFrame.left - hMargin, dlFrame.bottom), NULL, "<", new BMessage(SHAREWINDOW_COMMAND_PREVIOUS_PAGE), B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);
	AddBorderView(_prevPageButton);
	resultsView->AddChild(_prevPageButton);
	
	_nextPageButton = new BButton(BRect(dlFrame.right + vMargin, dlFrame.top, resultsBounds.right, dlFrame.bottom), NULL, ">", new BMessage(SHAREWINDOW_COMMAND_NEXT_PAGE), B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
	AddBorderView(_nextPageButton);
	resultsView->AddChild(_nextPageButton);

	BRect transferBounds(resultsBounds.right+hMargin, resultsBounds.top+3, middleBounds.Width()-hMargin, middleBounds.bottom-30);
	BView * transferView = new BView(transferBounds, NULL, B_FOLLOW_RIGHT | B_FOLLOW_TOP_BOTTOM, 0);
	AddBorderView(transferView);

	_transferList = new TransferListView(BRect(2, 2, transferBounds.Width()-(2+B_V_SCROLL_BAR_WIDTH), transferBounds.Height()-(5+fontHeight+vMargin)), SHAREWINDOW_COMMAND_BAN_USER);
	AddBorderView(_transferList);
	_transferList->SetTarget(toMe);
	_transferList->SetSelectionMessage(new BMessage(SHAREWINDOW_COMMAND_RESULT_SELECTION_CHANGED));
	_transferList->SetInvocationMessage(new BMessage(SHAREWINDOW_COMMAND_LAUNCH_TRANSFER_ITEM));
	transferView->AddChild(AddBorderView(new BScrollView(NULL, _transferList, B_FOLLOW_ALL_SIDES, 0L, false, true, B_FANCY_BORDER)));

	_cancelTransfersButton = new BButton(BRect(0, _transferList->Frame().bottom+vMargin-1, transferBounds.Width(), transferBounds.Height()-1), NULL, str(STR_REMOVE_SELECTED), new BMessage(SHAREWINDOW_COMMAND_CANCEL_DOWNLOADS), B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM, B_WILL_DRAW|B_NAVIGABLE|B_FULL_UPDATE_ON_RESIZE);
	AddBorderView(_cancelTransfersButton);
	transferView->AddChild(_cancelTransfersButton);

	_resultsTransferSplit = new SplitPane(middleBounds, resultsView, transferView, B_FOLLOW_ALL_SIDES);
	_resultsTransferSplit->SetResizeViewOne(true, true);
	AddBorderView(_resultsTransferSplit);

	BRect bottomBounds(hMargin, contentBounds.Height()+vMargin-CHAT_VIEW_HEIGHT, contentBounds.right-hMargin, contentBounds.Height()-vMargin);
	BRect chatViewBounds(0, 0, bottomBounds.Width()-(USER_LIST_WIDTH+hMargin), bottomBounds.Height());
	_chatView = new BView(chatViewBounds, NULL, B_FOLLOW_ALL_SIDES, 0);	// this will be populated by base class!
	AddBorderView(_chatView);

	BView * userListView = new BView(BRect(chatViewBounds.right+hMargin, bottomBounds.top, bottomBounds.right, bottomBounds.bottom), NULL, B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM, 0);
	AddBorderView(userListView);

	CLVContainerView* userContainerView;

	_usersView = new UserListView(SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW, BRect(2, 2, userListView->Bounds().Width()-(B_V_SCROLL_BAR_WIDTH+2), userListView->Bounds().Height()-(B_H_SCROLL_BAR_HEIGHT+2)),&userContainerView,NULL,B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE,B_MULTIPLE_SELECTION_LIST,false,true,true,true,B_FANCY_BORDER);
	AddBorderView(userContainerView);

	_usersView->SetSortFunction((CLVCompareFuncPtr) UserCompareFunc);
	_usersView->SetMessage(new BMessage(SHAREWINDOW_COMMAND_SELECT_USER));
	_usersView->SetTarget(toMe);

	userListView->AddChild(userContainerView);

	_chatUsersSplit = new SplitPane(bottomBounds, _chatView, userListView, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
	_chatUsersSplit->SetResizeViewOne(true, true);
	_chatUsersSplit->SetMinSizeOne(BPoint(100.0f, 0.0f));	// making the chat view too skinny can lock up BeShare :^P
	AddBorderView(_chatUsersSplit);

	_mainSplit = new SplitPane(BRect(contentBounds.left, UPPER_VIEW_HEIGHT+1.0f, contentBounds.right, contentBounds.bottom-20), _resultsTransferSplit, _chatUsersSplit, B_FOLLOW_ALL_SIDES);
	AddBorderView(_mainSplit);
	_mainSplit->SetResizeViewOne(true, true);

	ResetLayout();
	RestoreSplitPane(settingsMsg, _resultsTransferSplit, "resultstransfersplit"); 
	RestoreSplitPane(settingsMsg, _chatUsersSplit, "chatuserssplit"); 
	RestoreSplitPane(settingsMsg, _mainSplit, "mainsplit"); 

	contentView->AddChild(_mainSplit);

	AddUserColumn(settingsMsg, STR_NAME,			0.43f, NULL, 0);
	AddUserColumn(settingsMsg, STR_STATUS,		 0.33f, NULL, 0);
	AddUserColumn(settingsMsg, STR_ID,			 0.24f, NULL, CLV_RIGHT_JUSTIFIED);
	AddUserColumn(settingsMsg, STR_FILES,			0.38f, NULL, CLV_RIGHT_JUSTIFIED);
	AddUserColumn(settingsMsg, STR_CONNECTION_KEY,	0.57f, str(STR_CONNECTION_KEY)+2, 0);
	AddUserColumn(settingsMsg, STR_LOAD,			0.37f, NULL, CLV_RIGHT_JUSTIFIED);
	AddUserColumn(settingsMsg, STR_CLIENT,		 0.37f, NULL, 0);

	int numColumns = _usersView->CountColumns();
	if (numColumns > 0) {
		{
			int32 * userDisplayOrder = new int32[numColumns];
			for (int di=0; di<numColumns; di++) 
				if (settingsMsg.FindInt32("usercolumnsorder", di, &userDisplayOrder[di]) != B_NO_ERROR)
					userDisplayOrder[di] = di;
			
			_usersView->SetDisplayOrder(userDisplayOrder);
			delete [] userDisplayOrder;
		}
		{
			int32 * sortKeys	= new int32[numColumns];
			CLVSortMode * sortModes = new CLVSortMode[numColumns];
			int numSortKeys = 0;
			for (int di=0; (settingsMsg.FindInt32("usersortkey", di, &sortKeys[di]) == B_NO_ERROR); di++) {
				int32 temp;
				sortModes[di] = (settingsMsg.FindInt32("usersortmode", di, &temp) == B_NO_ERROR) ? (CLVSortMode)temp : NoSort;
				numSortKeys++;
			}
		
			if (numSortKeys > 0)
				_usersView->SetSorting(numSortKeys, sortKeys, sortModes);
			delete [] sortKeys;
			delete [] sortModes;
		}
	}

	// Restore any downloads that were going on when we last quit, and that might even now be resuscitatable.
	{
		BMessage xfrMsg;
		for (int i=0; settingsMsg.FindMessage("transfer", i, &xfrMsg) == B_NO_ERROR; i++) {
			ShareFileTransfer * xfer = new ShareFileTransfer(_downloadsDir, _netClient->GetLocalSessionID(), 0, 0, _maxDownloadRate);
			xfer->SetFromArchive(xfrMsg);
			AddHandler(xfer);
			xfer->AbortSession(true, true);	// start up already errored out but ready to restart
			_transferList->AddItem(xfer);
			_transferList->ScrollTo(0, 999999.0f);	// scroll to the bottom
		}
	}

	UpdateConnectStatus(true);
	UpdateQueryEnabledStatus();

	// Columns that we know will available for any file we can put up right away
	CreateColumn(NULL, FILE_NAME_COLUMN_NAME,		false);
	CreateColumn(NULL, "beshare:File Size",		false);
	CreateColumn(NULL, "beshare:Kind",			 false);
	CreateColumn(NULL, FILE_OWNER_COLUMN_NAME,	 false);
	CreateColumn(NULL, FILE_OWNER_BANDWIDTH_NAME,	false);
	CreateColumn(NULL, FILE_SESSION_COLUMN_NAME,	false);
	CreateColumn(NULL, "beshare:Modification Time", false);
	CreateColumn(NULL, "beshare:Path",			 false);

	// tell the ReflowingTextView how to send us querychange messages when "beshare://" is clicked
	BMessage qMsg(SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY);
	qMsg.AddBool("activate", true);
	SetCommandURLTarget(toMe, qMsg, BMessage(SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW));

	// The reaper will have us examine our transfer sessions every so
	// often, and kill any that are active-but-not-transferring-anything.
	// This prevents someone with a broken connection from piling up
	// a lot of useless/stalled requests, DOS'ing your client.
	_connectionReaper = new BMessageRunner(toMe, new BMessage(SHAREWINDOW_COMMAND_CHECK_FOR_MORIBUND_CONNECTIONS), 60*1000000LL); // check once per minute

	PostMessage(SHAREWINDOW_COMMAND_PRINT_STARTUP_MESSAGES);

	if (connectServer) {
		String cs = connectServer;
		int32 slashIdx = cs.IndexOf('/');
		
		if (slashIdx >= 0) {
			_queryOnConnect = cs.Substring(slashIdx+1);
			cs = cs.Substring(0, slashIdx);
		}
		_serverEntry->SetText(cs());
	}

	if ((connectServer) || (_loginOnStartup->IsMarked()))
		PostMessage(SHAREWINDOW_COMMAND_RECONNECT_TO_SERVER);

	UpdatePagingButtons();

	// Restore attribute presets
	{
		BMessage currentAttributeSettings;
		if (settingsMsg.FindMessage("currentattributesettings", &currentAttributeSettings) == B_NO_ERROR) {
			BMessage cmd(SHAREWINDOW_COMMAND_RESTORE_ATTRIBUTE_PRESET);
			cmd.AddMessage("settings", &currentAttributeSettings);
			PostMessage(&cmd);
		}
	}

	// Start a thread to see if there are any new servers around
	if (_autoUpdateServers->IsMarked()) {
		ThreadWorkerSessionRef plainSessionRef(new ThreadWorkerSession());
		plainSessionRef()->SetGateway(AbstractMessageIOGatewayRef(new PlainTextMessageIOGateway));
		
		if ((_checkServerListThread.StartInternalThread() == B_NO_ERROR)
			&& (_checkServerListThread.AddNewConnectSession(AUTO_UPDATER_SERVER, 80, plainSessionRef) != B_NO_ERROR))
			_checkServerListThread.ShutdownInternalThread();
	}
	
	PostMessage(CHATWINDOW_COMMAND_UPDATE_COLORS);

	const char * tempFontString;
	
	if (settingsMsg.FindString("font", &tempFontString) == B_NO_ERROR)
		SetFont(tempFontString, false);
}


BMenuItem*
ShareWindow::CreatePresetItem(int32 what, int32 which, bool enabled, bool shiftShortcut) const
{
	BMessage * msg = new BMessage(what);
	msg->AddInt32("which", which);

	char temp[32];
	sprintf(temp, "%li", which);
	BMenuItem * mi = new BMenuItem(temp, msg, '0'+which, shiftShortcut ? B_SHIFT_KEY : 0);
	mi->SetEnabled(enabled);
	return mi;
}


void
ShareWindow ::
AddUserColumn(const BMessage & settingsMsg, int labelID, float dw, const char * optLabel, uint32 extraFlags)
{
	char buf[128];
	sprintf(buf, "usercolumnwidth_%i", labelID);

	float width;
	if (settingsMsg.FindFloat(buf, &width) != B_NO_ERROR) width = dw*_usersView->Bounds().Width();
	_usersView->AddColumn(new CLVColumn(optLabel ? optLabel : str(labelID), width, CLV_SORT_KEYABLE | extraFlags));
}

void 
ShareWindow ::
SaveUserColumn(BMessage & settingsMsg, int labelID, CLVColumn * col) const
{
	char buf[128];
	sprintf(buf, "usercolumnwidth_%i", labelID);
	settingsMsg.AddFloat(buf, col->Width());
}

void
ShareWindow ::
RestoreSplitPane(const BMessage & settingsMsg, SplitPane * sp, const char * name) const
{
	BMessage temp;
	if (settingsMsg.FindMessage(name, &temp) == B_NO_ERROR) sp->SetState(&temp);
}

void
ShareWindow ::
AddServerItem(const char * serverName, bool quiet, int index)
{
	for (int i=_serverMenu->CountItems()-1; i>=0; i--) if (strcasecmp(_serverMenu->ItemAt(i)->Label(), serverName) == 0) return;

	BMessage * msg = new BMessage(SHAREWINDOW_COMMAND_USER_SELECTED_SERVER);
	msg->AddString("server", serverName);
	if ((index < 0)||(index >= _serverMenu->CountItems()))	_serverMenu->AddItem(new BMenuItem(serverName, msg));
												else _serverMenu->AddItem(new BMenuItem(serverName, msg), index);

	if (quiet == false)
	{ 
	 String serverLabel(serverName);
	 if (serverLabel.Length() > 90) serverLabel = serverLabel.Substring(0,90);
	 char buf[256];
	 sprintf(buf, str(STR_ADDED_SERVER), serverLabel());
	 LogMessage(LOG_INFORMATION_MESSAGE, buf);
	}
}

void
ShareWindow ::
AddUserNameItem(const char * un)
{
	BMessage * msg = new BMessage(SHAREWINDOW_COMMAND_USER_SELECTED_USER_NAME);
	msg->AddString("username", un);
	_userNameMenu->AddItem(new BMenuItem(un, msg));
}
		
void
ShareWindow ::
AddUserStatusItem(const char * us)
{
	BMessage * msg = new BMessage(SHAREWINDOW_COMMAND_USER_SELECTED_USER_STATUS);
	msg->AddString("userstatus", us);
	_userStatusMenu->AddItem(new BMenuItem(us, msg));
}
		
void
ShareWindow ::
SaveSplitPane(BMessage & settingsMsg, const SplitPane * sp, const char * name) const
{
	BMessage state;
	sp->GetState(state);
	settingsMsg.AddMessage(name, &state);
}

ShareWindow::~ShareWindow()
{
	if (_colorPicker->Lock()) _colorPicker->Quit();

	ResetAutoReconnectState(true);	// make sure the autoreconnect runner is deleted

	_checkServerListThread.ShutdownInternalThread();
	_acceptThread.ShutdownInternalThread();

	delete _queryInProgressRunner;
	_queryInProgressRunner = NULL;

	delete _connectionReaper;
	_connectionReaper = NULL;

	// delete MIME infos that aren't part of our menu hierarchy (the ones in the menu will be deleted by the BMenu)
	ShareMIMEInfo * mi;
	for (HashtableIterator<ShareMIMEInfo*, bool> iter(_emptyMimeInfos.GetIterator()); iter.HasData(); iter++) {
		mi = iter.GetKey();
		delete mi;
	}

	BMessage temp;
	GenerateSettingsMessage(temp);	// _settingMsg is saved to disk by the application object later (we are only holding a reference to it)
	if (AreMessagesEqual(temp, _stateMessage) == false) ((ShareApplication*)be_app)->SaveSettings(temp);

	// Close all private chat windows
	PrivateChatWindow* next;
	for (HashtableIterator<PrivateChatWindow*, String> iter(_privateChatWindows.GetIterator()); iter.HasData(); iter++) {
		next = iter.GetKey();
		if (next->Lock())
			next->Quit();
	}
		
	ClearUsers();

	delete _netClient;

	if (_doubleBufferBitmap)
	{
	 _doubleBufferBitmap->RemoveChild(_doubleBufferView);
	 delete _doubleBufferBitmap;
	}
	delete _doubleBufferView;
}


void 
ShareWindow::GenerateSettingsMessage(BMessage & settingsMsg)
{
	settingsMsg.MakeEmpty();

	// Save state of all open private message windows, and close them
	PrivateChatWindow* next;
	for (HashtableIterator<PrivateChatWindow*, String> iter(_privateChatWindows.GetIterator()); iter.HasData(); iter++) {
		next = iter.GetKey();
		if (next->LockWithTimeout(50000) == B_NO_ERROR) { // timeout to avoid possible deadlock with priv window Lock()'ing us while it is locked!
			BMessage stateMsg;
			next->SaveStateTo(stateMsg);
			next->Unlock();
			SavePrivateWindowInfo(stateMsg);
		}
	}

	// Save any active, pending, or errored-out downloads; maybe we can continue them later.
	for (int i=_transferList->CountItems()-1; i>=0; i--)
	{
	 ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(i);
	 if ((next->GetRemoteInstallID() > 0)&&(next->IsUploadSession() == false)&&((next->IsFinished() == false)||(next->GetOriginalFileSet().GetNumItems() > 0)))
	 if ((next->GetRemoteInstallID() > 0)&&(next->IsUploadSession() == false)&&((next->IsFinished() == false)||(next->GetOriginalFileSet().GetNumItems() > 0)))
	 {
		BMessage xfrMsg;
		next->SaveToArchive(xfrMsg);
		settingsMsg.AddMessage("transfer", &xfrMsg);	 
	 }
	}

	// Save attribute presets
	{
	 BMessage currentAttributeSettings;
	 SaveAttributesPreset(currentAttributeSettings);
	 settingsMsg.AddMessage("currentattributesettings", &currentAttributeSettings);
	}

	for (uint32 pr=0; pr<ARRAYITEMS(_attribPresets); pr++) (void) settingsMsg.AddMessage("attributepresets", &_attribPresets[pr]);

	for (uint32 p=0; p<_privateChatInfos.GetNumItems(); p++) settingsMsg.AddMessage("privwindows", &_privateChatInfos[p]);

	SaveSplitPane(settingsMsg, _resultsTransferSplit, "resultstransfersplit"); 
	SaveSplitPane(settingsMsg, _chatUsersSplit, "chatuserssplit"); 
	SaveSplitPane(settingsMsg, _mainSplit, "mainsplit"); 
	
	settingsMsg.AddInt32("maxdownloadrate", _maxDownloadRate);
	settingsMsg.AddInt32("maxuploadrate",	_maxUploadRate);

	settingsMsg.AddString("windowtitle", GetCustomWindowTitle()());
	if (_languageSet) settingsMsg.AddInt32("language", _language);
	settingsMsg.AddInt32("pagesize", _pageSize);
	settingsMsg.AddInt32("autoaway", _idleTimeoutMinutes);
	settingsMsg.AddInt32("complevel", _compressionLevel);
	settingsMsg.AddString("awaystatus", _awayStatus());
	settingsMsg.AddFloat("fontsize", GetFontSize());
	settingsMsg.AddString("font", GetFont()());

	if (_netClient->GetLocalUserName()[0]) settingsMsg.AddString("username", _netClient->GetLocalUserName());
	if (_fileNameQueryEntry->Text()[0]) settingsMsg.AddString("query", _fileNameQueryEntry->Text());

	// Save any additional strings....
	int qmLen = _queryMenu->CountItems();
	for (int qh=0; qh<qmLen; qh++) 
	{
	 const char * s; 
	 const BMessage * qmsg = _queryMenu->ItemAt(qh)->Message();
	 if ((qmsg)&&(qmsg->FindString("query", &s) == B_NO_ERROR)) settingsMsg.AddString("query", s);
	}

	int olLen = _onLoginStrings.GetNumItems();
	for (int ol=0; ol<olLen; ol++) settingsMsg.AddString("onlogin", _onLoginStrings.GetItemAt(ol)->Cstr());

	settingsMsg.AddString("server", _serverEntry->Text());
	settingsMsg.AddBool("fulluserqueries", _fullUserQueries->IsMarked());
	settingsMsg.AddBool("shortestfirst", _shortestUploadsFirst->IsMarked());
	settingsMsg.AddBool("autoclear", _autoClearCompletedDownloads->IsMarked());
	settingsMsg.AddBool("retainfilepaths", _retainFilePaths->IsMarked());
	settingsMsg.AddBool("loginonstartup", _loginOnStartup->IsMarked());
	settingsMsg.AddBool("autoupdateservers", _autoUpdateServers->IsMarked());
	
	settingsMsg.AddInt32("uploads", _maxSimultaneousUploadSessions);
	settingsMsg.AddInt32("downloads", _maxSimultaneousDownloadSessions);
	settingsMsg.AddInt32("uploadsperuser", _maxSimultaneousUploadSessionsPerUser);
	settingsMsg.AddInt32("downloadsperuser", _maxSimultaneousDownloadSessionsPerUser);
	settingsMsg.AddInt32("bandwidth", _uploadBandwidth);

	settingsMsg.AddBool("firewalled", _netClient->GetFirewalled());
	settingsMsg.AddBool("filesharingenabled", _sharingEnabled->IsMarked());
	settingsMsg.AddBool("filelogging", _toggleFileLogging->IsMarked());

	settingsMsg.AddString("watchpattern", _watchPattern());
	settingsMsg.AddString("autoprivpattern", _autoPrivPattern());

	for (int f=0; f<NUM_DESTINATIONS; f++)
	{
	 char filterSaveName[32];
	 sprintf(filterSaveName, "chatfilter%i", f);
	 for (int g=0; g<NUM_FILTERS; g++) settingsMsg.AddBool(filterSaveName, _filterItems[f][g]->IsMarked());
	}

	for (int i=0; i<_serverMenu->CountItems();	 i++) settingsMsg.AddString("serverlist",	_serverMenu->ItemAt(i)->Label());
	for (int u=0; u<_userNameMenu->CountItems();	u++) settingsMsg.AddString("usernamelist",	_userNameMenu->ItemAt(u)->Label());
	for (int s=0; s<_userStatusMenu->CountItems(); s++) settingsMsg.AddString("userstatuslist", _userStatusMenu->ItemAt(s)->Label());

	BRect windowpos = Frame();
	settingsMsg.AddRect("windowpos", windowpos);

	BMessage columnsSubMessage;
	String nextString;

	for (HashtableIterator<String, float> iter(_activeAttribs.GetIterator()); iter.HasData(); iter++) {
		float width = iter.GetValue();	// default width in case we can't find the column itself for some reason
		nextString = iter.GetKey();
		float colWidth = 0.0f;

		ShareColumn* sc;
		
		if (_columns.Get(nextString.Cstr(), sc) == B_NO_ERROR)
			colWidth = sc->Width();
		
		columnsSubMessage.AddFloat(nextString.Cstr(), (colWidth > 0.0f) ? colWidth : width);
	}
	
	settingsMsg.AddMessage("columns", &columnsSubMessage);

	SaveUserColumn(settingsMsg, STR_NAME,			_usersView->ColumnAt(0));
	SaveUserColumn(settingsMsg, STR_STATUS,		_usersView->ColumnAt(1));
	SaveUserColumn(settingsMsg, STR_ID,			_usersView->ColumnAt(2));
	SaveUserColumn(settingsMsg, STR_FILES,		 _usersView->ColumnAt(3));
	SaveUserColumn(settingsMsg, STR_CONNECTION_KEY, _usersView->ColumnAt(4));
	SaveUserColumn(settingsMsg, STR_LOAD,			_usersView->ColumnAt(5));
	SaveUserColumn(settingsMsg, STR_CLIENT,		_usersView->ColumnAt(6));

	int numColumns = _usersView->CountColumns();
	if (numColumns > 0)
	{
	 {
		int32 * userDisplayOrder = new int32[numColumns];
		_usersView->GetDisplayOrder(userDisplayOrder);
		for (int si=0; si<numColumns; si++) settingsMsg.AddInt32("usercolumnsorder", userDisplayOrder[si]);
		delete [] userDisplayOrder;
	 }
	 {
		int32 * sortKeys	= new int32[numColumns];
		CLVSortMode * sortModes = new CLVSortMode[numColumns];
		int32 numSortKeys = _usersView->GetSorting(sortKeys, sortModes);
		for (int si=0; si<numSortKeys; si++)
		{
			settingsMsg.AddInt32("usersortkey", sortKeys[si]);
			settingsMsg.AddInt32("usersortmode", (int32)sortModes[si]);
		}
		delete [] sortKeys;
		delete [] sortModes;
	 }
	}

	// Save any bans we have in effect
	for (HashtableIterator<uint32, uint64> iter(_bans.GetIterator()); iter.HasData(); iter++) {
		settingsMsg.AddInt32("banip", iter.GetKey());
	 	settingsMsg.AddInt64("banuntil", iter.GetValue());
	}

	// Save any aliases
	for (HashtableIterator<String, String> iter(_aliases.GetIterator()); iter.HasData(); iter++) {
		settingsMsg.AddString("aliasname", iter.GetKey().Cstr());
		settingsMsg.AddString("aliasvalue", iter.GetValue().Cstr());
	}
	
	// Save our colors
	for (int32 i = 0; i < NUM_COLORS; i++)
		SaveColorToMessage("colors", GetColor(i, -1), settingsMsg);

	settingsMsg.AddBool("customcolors", GetCustomColorsEnabled());
}


void 
ShareWindow::
SavePrivateWindowInfo(const BMessage & msg)
{
	uint32 index;
	if (msg.FindInt32("index", (int32*)&index) == B_NO_ERROR)
	{
	 BMessage blank;
	 while(_privateChatInfos.GetNumItems() <= index) _privateChatInfos.AddTail(blank);
	 _privateChatInfos[index] = msg;
	}
}


void
ShareWindow::ClearUsers()
{
	TRACE_BESHAREWINDOW(("ShareWindow::ClearUsers begin\n"));
	ClearResults();			// no users means no files available
	TRACE_BESHAREWINDOW(("ShareWindow::ClearUsers 1\n"));
	_usersView->MakeEmpty();	// for efficiency

	RemoteUserItem* next;
	TRACE_BESHAREWINDOW(("ShareWindow::ClearUsers before loop\n"));
	for (HashtableIterator<const char*, RemoteUserItem*> iter(_users.GetIterator()); iter.HasData(); iter++) {
		next = iter.GetValue();
		TRACE_BESHAREWINDOW(("Remove user %s\n", next->GetUserString().Cstr()));
		delete next;
	}	
	
	TRACE_BESHAREWINDOW(("ShareWindow::ClearUsers after loop\n"));
	_users.Clear();
	
	BMessage msg(PrivateChatWindow::PRIVATE_WINDOW_REMOVE_USER);
	TRACE_BESHAREWINDOW(("ShareWindow::ClearUsers before send message to priv\n"));
	SendToPrivateChatWindows(msg, NULL);
	TRACE_BESHAREWINDOW(("ShareWindow::ClearUsers end\n"));
}
/*
#if NOT_NEEDED_I_THINK
// Hash function that just uses the pointer value as a hash value
static uint32 RemoteUserItemPointerHash(RemoteUserItem * const & ptr);
static uint32 RemoteUserItemPointerHash(RemoteUserItem * const & ptr)
{
	return (uint32)ptr;
}
#endif
*/

BMenu*
ShareWindow::MakeLimitSubmenu(const BMessage& settingsMsg, uint32 code, const char* label, const char* fieldName, uint32& var)
{
	BMenu * qMenu = new BMenu(label);
	qMenu->SetRadioMode(true);

	int32 limit = var;
	if (settingsMsg.FindInt32(fieldName, &limit) == B_NO_ERROR) var = limit;

	uint32 limits[] = {1, 2, 3, 4, 5, 10, 20, NO_FILE_LIMIT};
	for (uint32 i=0; i<ARRAYITEMS(limits); i++)
	{
	 char temp[80];
	 if (i < ARRAYITEMS(limits)-1) sprintf(temp, "%lu", limits[i]);
							else strncpy(temp, str(STR_NO_LIMIT), sizeof(temp));
	 BMessage * msg = new BMessage(code);
	 msg->AddInt32("num", limits[i]);
	 BMenuItem * item = new BMenuItem(temp, msg);
	 if (limits[i] == (uint32) var) item->SetMarked(true);
	 qMenu->AddItem(item);
	}
	return qMenu;
}

void
ShareWindow ::
AddBandwidthOption(BMenu * bMenu, const char * label, int32 bps)
{
	BMessage * msg = new BMessage(SHAREWINDOW_COMMAND_SET_ADVERTISED_BANDWIDTH);
	msg->AddInt32("bps", bps);
	msg->AddString("label", label);
	BMenuItem * item = new BMenuItem(label, msg);
	if (bps == (int) _uploadBandwidth) 
	{
	 item->SetMarked(true);
	 _netClient->SetUploadBandwidth(label, bps);
	}
	bMenu->AddItem(item);
}


uint64
ShareWindow::IPBanTimeLeft(uint32 ip)
{
	uint64 now = real_time_clock_usecs();

	// First, purge any IP bans that have timed out
	for (HashtableIterator<uint32, uint64> iter(_bans.GetIterator()); iter.HasData(); iter++) {
		uint32 ip = iter.GetKey();
		uint64 time = iter.GetValue();
		if (time <= now)
			_bans.Remove(ip);
	}
	
	uint64 * banTime = _bans.Get(ip);
	return banTime ? ((*banTime == (uint64)-1) ? (uint64)-1 : *banTime-now) : 0;
}


uint32
ShareWindow ::
ParseRemoteIP(const char * hn) const
{
	uint32 rip = 0;
	StringTokenizer tok(hn, ".");
	for (int i=3; i>=0; i--)
	{
	 const char * nextQuad = tok.GetNextToken();
	 if (nextQuad) rip |= (((uint32)atoi(nextQuad)) << i*8);
	}
	return rip;
}


void 
ShareWindow::TransferCallbackRejected(const char * from, uint64 timeLeft)
{
	TRACE_BESHAREWINDOW(("ShareWindow::TransferCallbackRejected begin\n"));
	uint32 numItems = _transferList->CountItems();
	for (uint32 j=0; j<numItems; j++) {
		ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(j);
		if ((strcmp(next->GetRemoteSessionID(), from) == 0)
			&& (next->IsUploadSession() == false)
			&& (next->IsConnected() == false))
				next->TransferCallbackRejected(timeLeft);
	}
	TRACE_BESHAREWINDOW(("ShareWindow::TransferCallbackRejected end\n"));
}


void
ShareWindow::ConnectBackRequestReceived(const char * targetSessionID, uint16 port, const MessageRef & optBase)
{
	TRACE_BESHAREWINDOW(("ShareWindow::ConnectBackRequestReceived begin\n"));
	if (_sharingEnabled->IsMarked()) {
		RemoteUserItem * target;
		if ((_users.Get(targetSessionID, target) == B_NO_ERROR)&&(target->GetFirewalled() == false)) {
			// Check the IP address to make sure it's not banned
			uint32 rip = ParseRemoteIP(target->GetHostName());

			uint64 banTimeLeft = IPBanTimeLeft(rip);
			if (banTimeLeft > 0) {
				MessageRef banRef = MakeBannedMessage(banTimeLeft, optBase);
				if ((_netClient)
					&& (banRef())
					&& (banRef()->AddString(PR_NAME_SESSION, "") == B_NO_ERROR)
					&& (banRef()->AddString(PR_NAME_KEYS, String("/*/")+targetSessionID) == B_NO_ERROR)) 
						_netClient->SendMessageToSessions(banRef, true);
			} else {
				ShareFileTransfer* xfer = new ShareFileTransfer(_shareDir, _netClient->GetLocalSessionID(), target->GetInstallID(), 0, _maxUploadRate);
				AddHandler(xfer);
			
				if (xfer->InitConnectSession(target->GetHostName(), port, rip, target->GetSessionID()) == B_NO_ERROR) {
					_transferList->AddItem(xfer);
				} else {
					RemoveHandler(xfer);
					delete xfer;
				}
				
				DequeueTransferSessions();
			}
		}
	}
	TRACE_BESHAREWINDOW(("ShareWindow::ConnectBackRequestReceived end\n"));
}


void 
ShareWindow::RequestDownloads(const BMessage& filelistMsg, const BDirectory& downloadDir, BPoint *droppoint)
{
	TRACE_BESHAREWINDOW(("ShareWindow::RequestDownloads begin\n"));
	// First, collate the files by remote user.	We do this so that we only have to
	// make one TCP connection to each remote host, and download all files that are
	// to come from him one at a time over that.	It's more efficient than connecting
	// to him several times in parallel.
	Hashtable<RemoteUserItem*, ShareFileTransfer*> newTransferSessions;
	RemoteFileItem* item;
	for (int32 i = 0; (filelistMsg.FindPointer("item", i, (void **)&item) == B_NO_ERROR); i++) {
		if (_resultsView->HasItem(item)) {
			RemoteUserItem* owner = item->GetOwner();
			if ((owner->GetHostName()[0])&&((owner->GetPort() > 0) || (owner->GetFirewalled()))) {
				
				ShareFileTransfer* xfer;
				if (newTransferSessions.Get(owner, xfer) == B_ERROR) {
					xfer = new ShareFileTransfer(downloadDir, _netClient->GetLocalSessionID(), owner->GetInstallID(), owner->GetSupportsPartialHash() ? NUM_PARTIAL_HASH_BYTES : 0, _maxDownloadRate);
			 		AddHandler(xfer);
					newTransferSessions.Put(owner, xfer);
				}
				
				xfer->AddRequestedFileName(item->GetFileName(), 0LL, _retainFilePaths->IsMarked() ? item->GetPath() : "", droppoint);
				
				if (droppoint)
					droppoint->y += 50;
			} else {
				TRACE_BESHAREWINDOW(("ShareWindow::RequestDownloads Can't download\n"));
				String errStr(str(STR_CANT_DOWNLOAD_FROM_USER));
				errStr += owner->GetDisplayHandle();
				errStr += str(STR_COMMA_NO_CONNECTION_INFORMATION_AVAILABLE);
				LogMessage(LOG_ERROR_MESSAGE, errStr());
			}
		}
	}

	// Set up the ShareFileTransfers to await their incoming connections from the remote owners...
	ShareFileTransfer* nextXfer;
	for (HashtableIterator<RemoteUserItem*, ShareFileTransfer*> iter(newTransferSessions.GetIterator()); iter.HasData(); iter++) {
		nextXfer = iter.GetValue();
		if (SetupNewDownload(iter.GetKey(), nextXfer, false) == B_NO_ERROR) {
			_transferList->AddItem(nextXfer);
			_transferList->ScrollTo(0, 999999.0f);	// scroll to the bottom
		} else {
			RemoveHandler(nextXfer);
			delete nextXfer;	// he's toast
		}
	}
	DequeueTransferSessions();
	TRACE_BESHAREWINDOW(("ShareWindow::RequestDownloads end\n"));
}


status_t
ShareWindow::SetupNewDownload(const RemoteUserItem* user, ShareFileTransfer* xfer, bool forceRemoteIsFirewalled)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetupNewDownload begin\n"));
	if ((user->GetFirewalled()) || (forceRemoteIsFirewalled)) {
		if (_netClient->GetFirewalled()) {
			String errStr(str(STR_CANT_DOWNLOAD_FILES_FROM));
			errStr += user->GetUserString();
			errStr += str(STR_BECAUSE_BOTH_OF_US_ARE_BEHIND_FIREWALLS);
			LogMessage(LOG_ERROR_MESSAGE, errStr());
		} else {
			if (xfer->InitAcceptSession(user->GetSessionID()) == B_NO_ERROR)
				return B_NO_ERROR;
			else {
				String errStr(str(STR_FILE_DOWNLOAD_ACCEPT_SESSION_FOR));
				errStr += user->GetUserString();
				errStr += str(STR_FAILED_TO_INITIALIZE);
				LogMessage(LOG_ERROR_MESSAGE, errStr());
			}
		}
	} else {
		// He's not firewalled so we can connect to him directly.
		if (xfer->InitConnectSession(user->GetHostName(), user->GetPort(), 0, user->GetSessionID()) == B_NO_ERROR)
			return B_NO_ERROR;
		else {
			String errStr(str(STR_FILE_DOWNLOAD_SESSION_TO));
			errStr += user->GetUserString();
			errStr += str(STR_FAILED_TO_INITIALIZE);
			LogMessage(LOG_ERROR_MESSAGE, errStr());
		}
	}

	TRACE_BESHAREWINDOW(("ShareWindow::SetupNewDownload end\n"));
	return B_ERROR;
}


void
ShareWindow::SendConnectBackRequestMessage(const char * sessionID, uint16 port)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SendConnectBackRequestMessage begin\n"));
	_netClient->SendConnectBackRequestMessage(sessionID, port);
	TRACE_BESHAREWINDOW(("ShareWindow::SendConnectBackRequestMessage end\n"));
}


static int
SortShareFileTransfersBySize(ShareFileTransfer * const & s1, ShareFileTransfer * const & s2, void * cookie)
{
	const ShareNetClient * nc = (const ShareNetClient *) cookie;
	uint64 nb1 = s1->GetNumBytesLeftToUpload(nc);
	uint64 nb2 = s2->GetNumBytesLeftToUpload(nc);
	return muscleCompare((nb1>0)?nb1:((uint64)-1), (nb2>0)?nb2:((uint64)-1));	// empty gets prioritized last!
}

void
ShareWindow::DequeueTransferSessions(bool upload)
{
	TRACE_BESHAREWINDOW(("ShareWindow::DequeueTransferSessions begin with bool\n"));
	int numToStart = (upload ? _maxSimultaneousUploadSessions : _maxSimultaneousDownloadSessions) - CountActiveSessions(upload, NULL);
	if (numToStart > 0) {
		uint32 numItems = _transferList->CountItems();
		for (uint32 j = 0; j < numItems; j++) {
			ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(j);
			const char * nextSession = next->GetRemoteSessionID();
			if ((next->IsUploadSession() == upload) && (next->IsWaitingOnLocal())
				&& (CountActiveSessions(upload, nextSession[0] ? nextSession : NULL) < (upload ? _maxSimultaneousUploadSessionsPerUser : _maxSimultaneousDownloadSessionsPerUser))) {	
				next->BeginTransfer();
				if (--numToStart == 0)
					break;
			}
		}
	}
	UpdateDownloadButtonStatus();
	TRACE_BESHAREWINDOW(("ShareWindow::DequeueTransferSessions end with bool\n"));
}


void
ShareWindow::DequeueTransferSessions()
{
	TRACE_BESHAREWINDOW(("ShareWindow::DequeueTransferSessions begin\n"));
	if ((_dequeueCount == 0) && (_shortestUploadsFirst->IsMarked())) {
		// First, make up a list of all current non-aborted uploads
		Hashtable<ShareFileTransfer *, bool> origList;
		uint32 numXfers = _transferList->CountItems();
		for (uint32 i = 0; i < numXfers; i++) {
			ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(i);
			if ((next->IsUploadSession())
				&& (next->IsFinished() == false)
				&& (next->ErrorOccurred() == false))
					(void) origList.Put(next, true);
		}
		
		if (origList.GetNumItems() > 0) {
			// Then sort the list so that smallest transfers are first
			Hashtable<ShareFileTransfer *, bool> sortList = origList;
			#warning "Sort borde fixas?"
			//sortList.SortByKey(SortShareFileTransfersBySize, _netClient);

			bool sortOrderChanged = false;
			if (sortList.GetNumItems() == origList.GetNumItems()) { // paranoia
				HashtableIterator<ShareFileTransfer *, bool> origIter = origList.GetIterator();
				HashtableIterator<ShareFileTransfer *, bool> sortIter = sortList.GetIterator();
				ShareFileTransfer * orig, * sort;
				while(((orig = origIter.GetKey()) == B_NO_ERROR)
					&& ((sort = sortIter.GetKey()) == B_NO_ERROR)) {
					if (orig != sort) {
						sortOrderChanged = true; 
						break;
					}
				}
			}

			// Only bother the ListView if something is actually going to change
			if (sortOrderChanged) {
				_dequeueCount++;  // avoid infinite recursion from PauseAllUploads() and ResumeAllUploads()
				PauseAllUploads();
				{
					// Then update our xfer list so its upload-boxes are in the same order...
					HashtableIterator<ShareFileTransfer *, bool> sortListIter = sortList.GetIterator();
					for (uint32 j = 0; j < numXfers; j++) {
						ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(j);
						if (sortList.ContainsKey(next)) {
							ShareFileTransfer * replaceWith;
							if ((replaceWith = sortListIter.GetKey()) == B_NO_ERROR) 
								_transferList->ReplaceItem(j, replaceWith);
							else
								break;  // nothing left to replace!
						}
					}
				}
				ResumeAllUploads();
				_dequeueCount--;
			}
		}
	}

	DequeueTransferSessions(true);
	DequeueTransferSessions(false);
	_netClient->SetUploadStats(CountUploadSessions(), _maxSimultaneousUploadSessions, false);
	TRACE_BESHAREWINDOW(("ShareWindow::DequeueTransferSessions end\n"));
}


uint32
ShareWindow::CountActiveSessions(bool upload, const char * optForSessionID) const
{
	uint32 numActive = 0;
	for (int i=_transferList->CountItems()-1; i>=0; i--)
	{
	 ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(i);
	 if ((next->IsUploadSession() == upload)&&(next->IsActive())&&((optForSessionID == NULL)||(strcmp(optForSessionID, next->GetRemoteSessionID()) == 0))) numActive++;
	}
	return numActive;
}


// Returns the total number of upload sessions 
uint32
ShareWindow::CountUploadSessions() const
{
	uint32 numActive = 0;
	for (int i=_transferList->CountItems()-1; i>=0; i--) {
		ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(i);
		if (next->IsUploadSession())
			numActive++;
	}
	return numActive;
}


void
ShareWindow::SharesScanComplete()
{
	uint32 numItems = _transferList->CountItems();
	for (uint32 j=0; j<numItems; j++)
		((ShareFileTransfer *) _transferList->ItemAt(j))->SharesScanComplete();
}


void
ShareWindow::OpenTrackerFolder(const BDirectory & dir)
{
	BEntry entry(&dir, ".", true);
	entry_ref ref;
	if (entry.GetRef(&ref) == B_NO_ERROR) {
		BMessage msg(B_REFS_RECEIVED);
		msg.AddRef("refs", &ref);
		BMessenger("application/x-vnd.Be-TRAK").SendMessage(&msg);
	}
}


void ShareWindow::SaveAttributesPreset(BMessage & saveMsg)
{
	saveMsg.MakeEmpty();

	saveMsg.what = 1;	// signal that we have a valid config

	int numColumns = _resultsView->CountColumns();
	if (numColumns > 0)
	{
	 {
		int32 * displayOrder = new int32[numColumns];
		_resultsView->GetDisplayOrder(displayOrder);
		for (int di=1; di<numColumns; di++)
		{
			ShareColumn * col = (ShareColumn *) _resultsView->ColumnAt(displayOrder[di]);
			saveMsg.AddString("attrib", col->GetAttributeName());
			saveMsg.AddFloat("width", col->Width());
		}
		delete [] displayOrder;
	 }
	 {
		int32 * sortKeys = new int32[numColumns];
		CLVSortMode * sortModes = new CLVSortMode[numColumns];
		int32 numSortKeys = _resultsView->GetSorting(sortKeys, sortModes);
		for (int ds=0; ds<numSortKeys; ds++)
		{
			saveMsg.AddInt32("sortkey", sortKeys[ds]);
			saveMsg.AddInt32("sortmode", (int32)sortModes[ds]);
		}
		delete [] sortKeys;
		delete [] sortModes;
	 }
	}
}

void ShareWindow::RestoreAttributesPreset(const BMessage & restoreMsg)
{
	// Clear all existing columns....
	for (int32 i=_resultsView->CountColumns()-1; i>=1; i--)
	{
	 BMessage remMsg(SHAREWINDOW_COMMAND_TOGGLE_COLUMN);
	 remMsg.AddString("attrib", ((ShareColumn*)_resultsView->ColumnAt(i))->GetAttributeName());
	 PostMessage(&remMsg);
	}

	// And then add the saved ones
	const char * addColName;
	for (int32 j=0; restoreMsg.FindString("attrib", j, &addColName) == B_NO_ERROR; j++)
	{
	 float width;
	 if (restoreMsg.FindFloat("width", j, &width) != B_NO_ERROR) width = 70.0f;
	 
	 BMessage addMsg(SHAREWINDOW_COMMAND_TOGGLE_COLUMN);
	 addMsg.AddString("attrib", addColName);
	 addMsg.AddFloat("width", width);
	 PostMessage(&addMsg);
	}

	// After all new columns are added, then restore the saved sorting prefs
	BMessage sortMsg(restoreMsg);
	sortMsg.what = SHAREWINDOW_COMMAND_RESTORE_SORTING;
	PostMessage(&sortMsg);
}

MessageRef ShareWindow::MakeBannedMessage(uint64 time, const MessageRef & optBase) const
{
	MessageRef ret = optBase;
	if (ret()) ret()->what = ShareFileTransfer::TRANSFER_COMMAND_REJECTED;
		else ret = GetMessageFromPool(ShareFileTransfer::TRANSFER_COMMAND_REJECTED);

	if (ret()) ret()->AddInt64("timeleft", time);
	return ret;
}

void
ShareWindow::RemoveLRUItem(BMenu* menu, const BMessage& msg)
{
	int32 idx;
	
	if ((msg.FindInt32("index", &idx) == B_NO_ERROR) && (idx >= 0) && (idx < menu->CountItems())) 
		delete menu->RemoveItem(idx);
}


void
ShareWindow::MessageReceived(BMessage* msg)
{
	switch(msg->what)
	{
		case B_MOVE_TARGET:
	 	{
		entry_ref dirref;
		if (msg->FindRef("directory", &dirref) == B_NO_ERROR)
		{
			BDirectory directory(&dirref);
			BEntry entry;
			directory.GetEntry(&entry);
			BPath path;
			entry.GetPath(&path);

			const char *filename;
			if (msg->FindString("name", &filename) == B_NO_ERROR)
			{
			 BEntry fileentry(&directory, filename);
			 if (fileentry.Exists())
			 {
				// Determine where the file was dropped, by reading the attribute
				// that indicates its position. NOTE: this code does not support
				// big-endian bfs mounted on x86, or little-endian bfs mounted on PPC
				struct {
					char uninteresting_data[12];
					BPoint point;
				} pinfo;

				if (BNode(&fileentry).ReadAttr(B_HOST_IS_LENDIAN?"_trk/pinfo_le":"_trk/pinfo", B_RAW_TYPE, 0, &pinfo, sizeof(pinfo)) != sizeof(pinfo)) pinfo.point.Set(-1,-1);
				
				// remove the entry, which was created for us by the receiver
				fileentry.Remove();
				BMessage downloads;
				if (msg->FindMessage("be:originator-data", &downloads) == B_NO_ERROR) RequestDownloads(downloads, directory, (pinfo.point.x >= 0.0)?&pinfo.point:NULL);
			 }
			}
		}
	 }
	 break;		

	 case SHAREWINDOW_COMMAND_BAN_USER:
	 {
		uint64 now = real_time_clock_usecs();
		uint64 dur;
		if (msg->FindInt64("duration", (int64*) &dur) != B_NO_ERROR) dur = 0;
		const char * durstr;
		if (msg->FindString("durstr", &durstr) == B_NO_ERROR)
		{
			uint32 ip;
			for (int i=0; (msg->FindInt32("ip", i, (int32*) &ip) == B_NO_ERROR); i++)
			{
			 char buf[256];
			 char ipbuf[16]; Inet_NtoA(ip, ipbuf);
			 sprintf(buf, str(STR_USER_AT_IP_PS_BANNED_FOR), ipbuf); 
			 String temp(buf);
			 temp += ' ';
			 temp += durstr;
			 LogMessage(LOG_INFORMATION_MESSAGE, temp());
			 _bans.Put(ip, (dur > 0) ? (now + dur) : ((uint64)-1));
			}
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_FILE_SHARING_ENABLED:
		_sharingEnabled->SetMarked(!_sharingEnabled->IsMarked());
		_netClient->SetFileSharingEnabled(_sharingEnabled->IsMarked());
	 break;

	 case SHAREWINDOW_COMMAND_SET_AUTO_AWAY:
	 {
		int32 aa;
		if (msg->FindInt32("autoaway", &aa) == B_NO_ERROR) _idleTimeoutMinutes = aa;
	 }
	 break;

	 case SHAREWINDOW_COMMAND_SET_COMPRESSION_LEVEL:
	 {
		int32 compLevel;
		if (msg->FindInt32("complevel", &compLevel) == B_NO_ERROR) 
		{
			_compressionLevel = compLevel;
			_netClient->UpdateEncoding();
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_UNIDLE:
	 {
		if (strcmp(_userStatusEntry->Text(), (_oneTimeAwayStatus.Length() > 0) ? _oneTimeAwayStatus() : _awayStatus()) == 0)
		{
			_oneTimeAwayStatus = "";
			String revertTo((_revertToStatus.Length() > 0) ? _revertToStatus() : FACTORY_DEFAULT_USER_STATUS);
			_revertToStatus = "";
			_userStatusEntry->SetText(revertTo());
			BMessage csMsg(SHAREWINDOW_COMMAND_USER_CHANGED_STATUS);
			MessageReceived(&csMsg);	// do this synchronously!
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_USER_SELECTED_USER_NAME:
	 {
		if (modifiers() & B_CONTROL_KEY) RemoveLRUItem(_userNameMenu, *msg);
		else
		{
			const char * username;
			if (msg->FindString("username", &username) == B_NO_ERROR) 
			{
			 if (strcmp(username, _userNameEntry->Text()))
			 {
				_userNameEntry->SetText(username);
				PostMessage(SHAREWINDOW_COMMAND_USER_CHANGED_NAME);
			 }
			}
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_USER_SELECTED_USER_STATUS:
	 {
		if (modifiers() & B_CONTROL_KEY) RemoveLRUItem(_userStatusMenu, *msg);
		else
		{
			const char * userstatus;
			if (msg->FindString("userstatus", &userstatus) == B_NO_ERROR) 
			{
			 if (strcmp(userstatus, _userStatusEntry->Text()))
			 {
				_userStatusEntry->SetText(userstatus);
				PostMessage(SHAREWINDOW_COMMAND_USER_CHANGED_STATUS);
			 }
			}
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_SWITCH_TO_PAGE:
	 {
		int32 page;
		if (msg->FindInt32("page", &page) == B_NO_ERROR) 
		{
			SwitchToPage(page);
			UpdateTitleBar();	
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_SET_PAGE_SIZE:
	 {
		int32 l;
		if (msg->FindInt32("pagesize", &l) == B_NO_ERROR) _pageSize = l;
	 }
	 break;
 
	 case SHAREWINDOW_COMMAND_TOGGLE_FILE_LOGGING:
	 {
		bool newState = !_toggleFileLogging->IsMarked();
		if (newState == false) 
		{
			LogMessage(LOG_INFORMATION_MESSAGE, str(STR_LOGGING_DISABLED));
			CloseLogFile();
		}
		_toggleFileLogging->SetMarked(newState);
		if (newState) LogMessage(LOG_INFORMATION_MESSAGE, str(STR_LOGGING_ENABLED));
	 }
	 break;

	 case SHAREWINDOW_COMMAND_PRINT_STARTUP_MESSAGES:
		if (_acceptThread.GetPort() > 0)
		{
			char temp[150];
			sprintf(temp, str(STR_BESHARE_IS_LISTENING_ON_PORT_PERCENTU), _acceptThread.GetPort());
			LogMessage(LOG_INFORMATION_MESSAGE, temp);
		}
		else LogMessage(LOG_ERROR_MESSAGE, str(STR_COULDNT_START_FILE_SHARING_THREAD));

		GenerateSettingsMessage(_stateMessage);	// also save our starting config for later comparisons
	 break;

	 case SHAREWINDOW_COMMAND_PREVIOUS_PAGE:
		SwitchToPage(((int)_currentPage)-1);
		UpdateTitleBar();
	 break;

	 case SHAREWINDOW_COMMAND_NEXT_PAGE:
		SwitchToPage(_currentPage+1);
		UpdateTitleBar();
	 break;

	 case SHAREWINDOW_COMMAND_QUERY_IN_PROGRESS_ANIM:
		DrawQueryInProgress(_queryInProgressRunner != NULL);
	 break;

	 case SHAREWINDOW_COMMAND_SAVE_ATTRIBUTE_PRESET:
	 {
		uint32 which;
		if ((msg->FindInt32("which", (int32*)&which) == B_NO_ERROR)&&(which < ARRAYITEMS(_attribPresets)))
		{
			BMessage & saveMsg = _attribPresets[which];
			SaveAttributesPreset(saveMsg);
			_restorePresets[which]->SetEnabled(true);	// since there is now something to restore in this slot...
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_RESTORE_ATTRIBUTE_PRESET:
	 {
		uint32 which;
		if ((msg->FindInt32("which", (int32*) &which) == B_NO_ERROR)&&(which < ARRAYITEMS(_attribPresets))) RestoreAttributesPreset(_attribPresets[which]);
		else 
		{
			BMessage settingsMsg;
			if (msg->FindMessage("settings", &settingsMsg) == B_NO_ERROR) RestoreAttributesPreset(settingsMsg);
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_RESTORE_SORTING:
	 {
		int32 numColumns = _resultsView->CountColumns();
		int32 * sortKeys = new int32[numColumns];
		CLVSortMode * sortModes = new CLVSortMode[numColumns];
		int32 numSortKeys = 0;
		for (int i=0; ((i<numColumns)&&(msg->FindInt32("sortkey", i, &sortKeys[i]) == B_NO_ERROR)); i++)
		{
			int32 temp;
			sortModes[i] = (msg->FindInt32("sortmode", i, &temp) == B_NO_ERROR) ? (CLVSortMode)temp : NoSort;
			numSortKeys++;
		}
		if (numSortKeys > 0) _resultsView->SetSorting(numSortKeys, sortKeys, sortModes);
		delete [] sortKeys;
		delete [] sortModes;
	 }
	 break;

	 case PrivateChatWindow::PRIVATE_WINDOW_CLOSED:
	 {
		PrivateChatWindow * w;
		if ((msg->FindPointer("which", (void **) &w) == B_NO_ERROR)&&(_privateChatWindows.Remove(w) == B_NO_ERROR))
		{
			BMessage stateMsg;
			if (msg->FindMessage("state", &stateMsg) == B_NO_ERROR) SavePrivateWindowInfo(stateMsg);
		}
	 }
	 break;

	 case PrivateChatWindow::PRIVATE_WINDOW_USER_TEXT_CHANGED:
	 {
		PrivateChatWindow * w;
		const char * target;
		if ((msg->FindPointer("which", (void **) &w) == B_NO_ERROR)&&
			(msg->FindString("users", &target) == B_NO_ERROR)&&
			(_privateChatWindows.ContainsKey(w))) 
		{
			_privateChatWindows.Put(w, target);
			UpdatePrivateWindowUserList(w, target);
		}
	 }
	 break;

	case SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW:
	{
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW begin\n"));
		const char * target;
		if (msg->FindString("users", &target) != B_NO_ERROR)
			target = _lastPrivateMessageTarget();

		uint32 idx = _privateChatWindows.GetNumItems();
		const BMessage * archive = (idx < _privateChatInfos.GetNumItems()) ? _privateChatInfos.GetItemAt(idx) : NULL;
		const BMessage blank;
		
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW 1\n"));
		
		PrivateChatWindow * pcw = new PrivateChatWindow(_toggleFileLogging->IsMarked(), archive?*archive:blank, idx, this, (strlen(target) > 0) ? target : NULL);
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW 2\n"));
		pcw->ReadyToRun();
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW 3\n"));
		_privateChatWindows.Put(pcw, target);
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW 4\n"));
		UpdatePrivateWindowUserList(pcw, target);

		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW 5\n"));
		// tell the ReflowingTextView how to send us querychange messages when "beshare://" is clicked
		BMessage qMsg(SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY);
		qMsg.AddBool("activate", true);
		pcw->SetCommandURLTarget(BMessenger(this), qMsg, BMessage(SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW));
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW 6\n"));
		UpdatePrivateChatWindowsColors();
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW 7\n"));
		pcw->Show();
		TRACE_BESHAREWINDOW(("ShareWindow::MessageReceived SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW end\n"));
	}
	break;		

	 case SHAREWINDOW_COMMAND_CHECK_FOR_MORIBUND_CONNECTIONS:
	 {
		if (_idleSendPending)
		{		
			SendChatText(_onIdleString, NULL);
			_idleSendPending = false;
		}

		bigtime_t now = system_time();
		for (int i=_transferList->CountItems()-1; i>=0; i--)
		{
			ShareFileTransfer * next = (ShareFileTransfer *)_transferList->ItemAt(i);
			bigtime_t nextT = next->LastTransferTime();
			if ((next->IsUploadSession())&&(nextT > 0LL))
			{
			 bigtime_t diff = now - nextT;
			 if (diff > MORIBUND_TIMEOUT_SECONDS*1000000LL)
			 {
				printf("Connection %i is moribund, aborting!\n", i);
				next->AbortSession(true);
			 }
			} 
		}

		// Check for auto-away every minute, too
		if ((_idleTimeoutMinutes > 0)&&(now > _lastInteractionAt + (_idleTimeoutMinutes * 60 * 1000000))) MakeAway();

		// If we haven't any server activity recent
		_netClient->CheckServer();

		// Let's also see if our settings have changed.	If they have, we'll save to disk now (in case we crash before we quit)
		BMessage temp;
		GenerateSettingsMessage(temp);	// _settingMsg is saved to disk by the application object later (we are only holding a reference to it)
		if (AreMessagesEqual(temp, _stateMessage) == false)
		{
			((ShareApplication*)be_app)->SaveSettings(temp);
			_stateMessage = temp;
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_LAUNCH_TRANSFER_ITEM:
	 {
		int32 nextIndex;
		for (int i=0; ((nextIndex = _transferList->CurrentSelection(i)) >= 0); i++) ((ShareFileTransfer *)_transferList->ItemAt(nextIndex))->LaunchCurrentItem();
	 }
	 break;

	 case SHAREWINDOW_COMMAND_SELECT_LANGUAGE:
	 {
		int32 l;
		if (msg->FindInt32("language", &l) == B_NO_ERROR)
		{
			_language = l;
			_languageSet = true;
			char temp[200];
			sprintf(temp, str(STR_LANGUAGE_SELECTED), GetLanguageName(_language, false));
			LogMessage(LOG_INFORMATION_MESSAGE, temp);
		}
	 }
	 break;
 
	 case SHAREWINDOW_COMMAND_OPEN_SHARED_FOLDER:
		OpenTrackerFolder(_shareDir);
	 break;

	 case SHAREWINDOW_COMMAND_OPEN_LOGS_FOLDER:
		OpenTrackerFolder(GetLogsDir());
	 break;

	 case SHAREWINDOW_COMMAND_OPEN_DOWNLOADS_FOLDER:
		OpenTrackerFolder(_downloadsDir);
	 break;

	 case SHAREWINDOW_COMMAND_SELECT_USER:
	 {
		String strng = _fileNameQueryEntry->Text();
		int atIndex = strng.IndexOf('@');
		if (_fullUserQueries->IsMarked()) strng = "*";
									else if (atIndex >= 0) strng = strng.Substring(0, atIndex)();	// only the file part, pleez
		strng += '@';

		bool filesThere = false;
		int32 nextIndex;
		for (int i=0; ((nextIndex = _usersView->CurrentSelection(i)) >= 0); i++) 
		{
			RemoteUserItem * next = (RemoteUserItem *)_usersView->ItemAt(nextIndex);
			if (i > 0) strng += ',';
			strng += next->GetSessionID(); 
			if ((GetFirewalled() == false)||(next->GetFirewalled() == false)) filesThere = true;
		}
		if (filesThere)
		{
			_fileNameQueryEntry->SetText(strng());
			SetQueryEnabled(false);
			SetQueryEnabled(true, false);
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_RESET_LAYOUT: 
		ResetLayout();
	 break;

	 case SHAREWINDOW_COMMAND_SET_ADVERTISED_BANDWIDTH:
	 {
		const char * label;
		if ((msg->FindString("label", &label) == B_NO_ERROR)&&(msg->FindInt32("bps", (int32 *) &_uploadBandwidth) == B_NO_ERROR)) _netClient->SetUploadBandwidth(label, _uploadBandwidth);
	 }
	 break;

	 case SHAREWINDOW_COMMAND_SET_UPLOAD_LIMIT:
		(void) msg->FindInt32("num", (int32 *) &_maxSimultaneousUploadSessions);
		DequeueTransferSessions();
	 break;

	 case SHAREWINDOW_COMMAND_SET_DOWNLOAD_LIMIT:
		(void) msg->FindInt32("num", (int32 *) &_maxSimultaneousDownloadSessions);
		DequeueTransferSessions();
	 break;

	 case SHAREWINDOW_COMMAND_SET_UPLOAD_PER_USER_LIMIT:
		(void) msg->FindInt32("num", (int32 *) &_maxSimultaneousUploadSessionsPerUser);
		DequeueTransferSessions();
	 break;

	 case SHAREWINDOW_COMMAND_SET_DOWNLOAD_PER_USER_LIMIT:
		(void) msg->FindInt32("num", (int32 *) &_maxSimultaneousDownloadSessionsPerUser);
		DequeueTransferSessions();
	 break;

	 case SHAREWINDOW_COMMAND_CLEAR_CHAT_LOG:
		ClearChatLog();
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_FIREWALLED:
		_netClient->SetFirewalled(!_netClient->GetFirewalled());
		if (_queryEnabled)	// force query refresh so that only non-firewalled files are visible
		{
			SetQueryEnabled(false, false);
			SetQueryEnabled(true, false);
		}
		for (int i=_usersView->CountItems()-1; i>=0; i--) 
		{
			RemoteUserItem * rui = (RemoteUserItem *)_usersView->ItemAt(i);
			rui->SetNumSharedFiles(rui->GetNumSharedFiles());	// force update to set/unset parens
		}
		UpdateConnectStatus(false);
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_CHAT_FILTER:
	 {
		BMenuItem * mi;
		if (msg->FindPointer("source", (void **) &mi) == B_NO_ERROR) mi->SetMarked(!mi->IsMarked());
	 }
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_FULL_USER_QUERIES:
		_fullUserQueries->SetMarked(!_fullUserQueries->IsMarked());
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_SHORTEST_UPLOADS_FIRST:
		_shortestUploadsFirst->SetMarked(!_shortestUploadsFirst->IsMarked());
		DequeueTransferSessions();
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_AUTOCLEAR_COMPLETED_DOWNLOADS:
		_autoClearCompletedDownloads->SetMarked(!_autoClearCompletedDownloads->IsMarked());
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_RETAIN_FILE_PATHS:
		_retainFilePaths->SetMarked(!_retainFilePaths->IsMarked());
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_LOGIN_ON_STARTUP:
		_loginOnStartup->SetMarked(!_loginOnStartup->IsMarked());
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_AUTOUPDATE_SERVER_LIST:
		_autoUpdateServers->SetMarked(!_autoUpdateServers->IsMarked());
	 break;

	 // Called by our BMessageTransceiverThreads when they have generated new events for us to handle.
	 case MUSCLE_THREAD_SIGNAL:
	 {
		// Check for any new messages from our HTTP thread
		uint32 code;
		MessageRef next;
		while(_checkServerListThread.GetNextEventFromInternalThread(code, &next) >= 0)
		{
			switch(code)
			{
			 case MTT_EVENT_INCOMING_MESSAGE:
				if (next())
				{
					String nextString;
					for (int i=0; next()->FindString(PR_NAME_TEXT_LINE, i, nextString) == B_NO_ERROR; i++)
					{
					 int hashIndex = nextString.IndexOf('#');
					 if (hashIndex >= 0) nextString = nextString.Substring(0, hashIndex);

					 if (nextString.StartsWith("beshare_"))
					 {
						StringTokenizer tok(nextString()+8, "=");
						const char * param = tok.GetNextToken();
						if (param)
						{ 
							const char * val = tok.GetRemainderOfString();
							String valStr(val?val:"");
							UpdaterCommandReceived(String(param).Trim()(), valStr.Trim()());
						}
					 }
					}
				}
			 break;

			 // We get this when we successfully connect to Minox's HTTP server
			 case MTT_EVENT_SESSION_CONNECTED:
			 {
				MessageRef pmsg = GetMessageFromPool();
				if (pmsg())
				{ 
					pmsg()->AddString(PR_NAME_TEXT_LINE, "GET /servers.txt HTTP/1.1\nUser-Agent: BeShare/"VERSION_STRING"\nHost: "AUTO_UPDATER_SERVER"\n\n");
					_checkServerListThread.SendMessageToSessions(pmsg);
				}
			 }
			 break;

			 // We get this when the HTTP server closes the session... here we can clean up
			 case MTT_EVENT_SESSION_DETACHED:
				_checkServerListThread.ShutdownInternalThread();
			 break;
			}
		}

		// Check for any new incoming TCP connections from our accept thread
		while(_acceptThread.GetNextReplyFromInternalThread(next) >= 0)
		{
			if (next())
			{
			 switch(next()->what)
			 {
				case AST_EVENT_NEW_SOCKET_ACCEPTED:
				{
					RefCountableRef tag;
					if (next()->FindTag(AST_NAME_SOCKET, tag) == B_NO_ERROR)
					{
					 ConstSocketRef sref(tag, false);
					 uint32 remoteIP;
					 if ((sref()) && (_sharingEnabled->IsMarked()) && ((remoteIP = GetPeerIPAddress(sref, true)) > 0))
					 {
						uint64 banTime = IPBanTimeLeft(remoteIP);
						if (banTime > 0)
						{
							TCPSocketDataIO sockIO(sref, false);	// this will close the socket for us when it goes
							MessageRef banRef = MakeBannedMessage(banTime, MessageRef());
							if (banRef())
							{
							 // Tell the poor bastard he's banned, before we hang up on him
							 MessageIOGateway gw;
							 gw.SetDataIO(DataIORef(&sockIO, false));
							 gw.AddOutgoingMessage(banRef);
							 int32 bytesWritten = 0;
							 while(1)
							 {
								int32 bw = gw.DoOutput();	// we assume that everything will be output in one try; if not, too bad!
								bytesWritten += bw;
								if (bw <= 0) break;
							 }
							 sockIO.FlushOutput();
							}
						}
						else
						{
							ShareFileTransfer * newSession = new ShareFileTransfer(_shareDir, _netClient->GetLocalSessionID(), 0, 0, _maxUploadRate);
							AddHandler(newSession);
							if (newSession->InitSocketUploadSession(sref, remoteIP, CountActiveSessions(true, NULL) >= _maxSimultaneousUploadSessions) == B_NO_ERROR) _transferList->AddItem(newSession);
							else
							{
							 LogMessage(LOG_ERROR_MESSAGE, str(STR_COULDNT_START_SHAREFILETRANSFER_SESSION));
							 RemoveHandler(newSession);
							 delete newSession;
							}
							UpdateDownloadButtonStatus();
							DequeueTransferSessions();
						}
					 }
					 //else CloseSocket(sref);
					}
				}
				break;
			 }
			}
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_REMOVE_SESSION:
	 {
		ShareFileTransfer * who;
		if (msg->FindPointer("who", (void **) &who) == B_NO_ERROR)
		{
			RemoveHandler(who);
			_transferList->RemoveItem(who);
			delete who;
			DequeueTransferSessions();
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_CLEAR_FINISHED_DOWNLOADS:
	 {
		for (int i=_transferList->CountItems()-1; i>=0; i--)
		{
			ShareFileTransfer * next = (ShareFileTransfer *)_transferList->ItemAt(i);
			if (next->IsFinished())
			{
			 RemoveHandler(next);
			 _transferList->RemoveItem(i);
			 delete next;	
			}
		}
		UpdateDownloadButtonStatus();
	 }
	 break;

	 case SHAREWINDOW_COMMAND_RESULT_SELECTION_CHANGED:
		UpdateDownloadButtonStatus();
	 break;

	 case SHAREWINDOW_COMMAND_CANCEL_DOWNLOADS:
	 {
		Queue<ShareFileTransfer *> killList;
		int32 nextIndex;
		for (int i=0; ((nextIndex = _transferList->CurrentSelection(i)) >= 0); i++) 
		{
			ShareFileTransfer * next = (ShareFileTransfer *)_transferList->ItemAt(nextIndex);
			killList.AddTail(next);
		}
		for (int j=killList.GetNumItems()-1; j>=0; j--)
		{
			RemoveHandler(killList[j]);
			_transferList->RemoveItem(killList[j]);
			delete killList[j];
		}
		DequeueTransferSessions();
		UpdateDownloadButtonStatus();
	 }
	 break;

		case SHAREWINDOW_COMMAND_BEGIN_DOWNLOADS:
		{
			BMessage filelistMsg;
			int32 nextIndex;
			
			for (int32 i = 0; ((nextIndex =_resultsView->CurrentSelection(i)) >= 0); i++)
				filelistMsg.AddPointer("item", _resultsView->ItemAt(nextIndex));
		
			RequestDownloads(filelistMsg, _downloadsDir, NULL);
			_resultsView->DeselectAll();
			UpdateDownloadButtonStatus();
	 }
	 break;
 
	 case SHAREWINDOW_COMMAND_USER_CHANGED_NAME:
		SetLocalUserName(String(_userNameEntry->Text()).Trim()());
	 break;

	 case SHAREWINDOW_COMMAND_USER_CHANGED_STATUS:
	 {
		bool a;
		if ((msg->FindBool("auto", &a) != B_NO_ERROR)||(a == false)) 
		{
			_lastInteractionAt = system_time();
			_revertToStatus = _oneTimeAwayStatus = "";
		}
		SetLocalUserStatus(String(_userStatusEntry->Text()).Trim()());
	 }
	 break;

		case SHAREWINDOW_COMMAND_DISCONNECT_FROM_SERVER:
			if (_autoReconnectRunner)
				LogMessage(LOG_INFORMATION_MESSAGE, str(STR_AUTO_RECONNECT_SEQUENCE_TERMINATED));
			
			ResetAutoReconnectState(true);	// user intervened, so reset count
			UpdateConnectStatus(false);	 // make sure disconnect button goes disabled
			_netClient->DisconnectFromServer();
	 	break;

	 case SHAREWINDOW_COMMAND_ABOUT:
	 {
		char temp[200];
		sprintf(temp, "BeShare v%s (MUSCLE %s)\n%s\nJeremy Friesner (jaf@lcsaudio.com)\nFredrik Modéen ([firstname]@[lastname].se)\n\n%s Vitaliy Mikitchenko", VERSION_STRING, MUSCLE_VERSION_STRING, str(STR_BY), str(STR_COLOR_PREFS_BY));
		const char * url = NULL;
		switch((new BAlert("About BeShare", temp, "BeBits Page", "BeShare Page", "Okay"))->Go())
		{
			case 0: url = BESHARE_BEBITS_URL;	break;
			case 1: url = BESHARE_HOMEPAGE_URL; break;
//			case 2: url = BESHARE_SOURCE_URL; break;
		}

		if (url) be_roster->Launch("text/html", 1, (char**) &url);
	 }
	 break;

	case SHAREWINDOW_COMMAND_RECONNECT_TO_SERVER:
		TRACE_BESHAREWINDOW(("Here\n"));
		ResetAutoReconnectState(true);	// user intervened, so reset count
		ReconnectToServer();
	break;

	 case SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY:
	 {
		if (modifiers() & B_CONTROL_KEY) RemoveLRUItem(_queryMenu, *msg);
		else
		{
			const char * s = _fileNameQueryEntry->Text();
			if (msg->FindString("query", &s) == B_NO_ERROR)
			{
			 String q(s);
			 q = q.Trim();
			 _fileNameQueryEntry->SetText(q());
			}

			bool activate;
			if ((_queryEnabled)||((msg->FindBool("activate", &activate) == B_NO_ERROR)&&(activate)))
			{
			 // force the query to be re-sent
			 SetQueryEnabled(false);
			 SetQueryEnabled(true);
			}
		}
	 }
	 break;
 
	 case SHAREWINDOW_COMMAND_USER_CHANGED_SERVER:	// user entered new server text
		UpdateConnectStatus(false);
	 break;

	 case SHAREWINDOW_COMMAND_USER_SELECTED_SERVER:	// user selected server from pop-up
	 {
		if (modifiers() & B_CONTROL_KEY) RemoveLRUItem(_serverMenu, *msg);
		else
		{
			const char * server;
			if (msg->FindString("server", &server) == B_NO_ERROR) SetServer(server);
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_ENABLE_QUERY:
		_fileNameQueryEntry->MakeFocus(false);
		SetQueryEnabled(true);
	 break;

	 case SHAREWINDOW_COMMAND_DISABLE_QUERY:
		SetQueryEnabled(false);
	 break;

	 case SHAREWINDOW_COMMAND_TOGGLE_COLUMN:
	 {
		const char * attrName;
		if (msg->FindString("attrib", &attrName) == B_NO_ERROR)
		{
			BMenuItem * mi;
			if (_attribMenuItems.Get(attrName, mi) == B_NO_ERROR)
			{
			 mi->SetMarked(!mi->IsMarked());
			 ShareColumn * sc;
			 if (_columns.Get(attrName, sc) == B_NO_ERROR)
			 {
				float newWidth;
				if (msg->FindFloat("width", &newWidth) == B_NO_ERROR) sc->SetWidth(newWidth);

				bool isVisible = (_resultsView->IndexOfColumn(sc) >= 0);
				if ((isVisible)&&(mi->IsMarked() == false)) 
				{
					_activeAttribs.Remove(sc->GetAttributeName());
					_resultsView->RemoveColumn(sc);
				}
				else if ((isVisible == false)&&(mi->IsMarked())) 
				{
					float w = DEFAULT_COLUMN_WIDTH;
					if (_activeAttribs.Get(sc->GetAttributeName(), w) == B_NO_ERROR) sc->SetWidth(w);
																		else _activeAttribs.Put(sc->GetAttributeName(), sc->Width());
					_resultsView->AddColumn(sc);
				}
			 }
			}
		}
	 }
	 break;

	 case SHAREWINDOW_COMMAND_AUTO_RECONNECT:
		if ((_isConnecting == false)&&(_isConnected == false)) DoAutoReconnect();
													 else ResetAutoReconnectState(false);
	 break;

	 case SHAREWINDOW_COMMAND_SHOW_COLOR_PICKER:
		if ((_colorPicker)&&(_colorPicker->Lock() == false)) _colorPicker = NULL;
		if (_colorPicker)
		{
			_colorPicker->Show();
			_colorPicker->Activate();
			_colorPicker->Unlock();
		}
		else
		{
			_colorPicker = new ColorPicker(this);
			_colorPicker->Show();
		}
	 break;
	 
	 default:
		ChatWindow :: MessageReceived(msg);
	 break;
	}
}

void ShareWindow::UpdateColors()
{
	ChatWindow::UpdateColors();

	UpdateTextViewColors(_serverEntry->TextView());
	UpdateTextViewColors(_userNameEntry->TextView());
	UpdateTextViewColors(_userStatusEntry->TextView());
	UpdateTextViewColors(_fileNameQueryEntry->TextView());

	UpdateColumnListViewColors(_usersView);
	UpdateColumnListViewColors(_resultsView);

	UpdatePrivateChatWindowsColors();
	RefreshTransfersFor(NULL);
}


void
ShareWindow::UpdatePrivateChatWindowsColors()
{
	TRACE_BESHAREWINDOW(("ShareWindow::UpdatePrivateChatWindowsColors begin\n"));
	BMessage updateAllColors(CHATWINDOW_COMMAND_COLOR_CHANGED);
	for (int i=0; i<NUM_COLORS; i++) {
		const rgb_color & col = GetColor(i);
		updateAllColors.AddInt32("color", i);
		SaveColorToMessage("rgb", col, updateAllColors);
	}
	
	PrivateChatWindow* priv;

	for (HashtableIterator<PrivateChatWindow *, String> iter(_privateChatWindows.GetIterator()); iter.HasData(); iter++) {
		priv = iter.GetKey();
		priv->PostMessage(&updateAllColors);
	}
	TRACE_BESHAREWINDOW(("ShareWindow::UpdatePrivateChatWindowsColors begin\n"));
}


void ShareWindow::UpdaterCommandReceived(const char * key, const char * value)
{
	if (value[0])
	{
	 if (strcmp(key, "version") == 0)
	 {
		if (strcmp(value, VERSION_STRING))
		{
			// try a numeric comparison...
			float myVersion = atof(VERSION_STRING);
			float newVersion = atof(value);

			if ((myVersion == 0.0f)||(newVersion == 0.0f)||(newVersion > myVersion))
			{
			 // avoid potential buffer overflow in sprintf()
			 String temp(value);
			 if (temp.Length() > 30) temp = temp.Substring(0, 30);

			 char buf[256];
			 sprintf(buf, str(STR_BESHARE_UPGRADE_NOTICE), temp(), BESHARE_BEBITS_URL);
			 LogMessage(LOG_INFORMATION_MESSAGE, buf);
			}
		}
	 }
	 else if (strcmp(key, "addserver")	== 0) AddServerItem(value, false, -1);
	 else if (strcmp(key, "removeserver") == 0) RemoveServerItem(value, false);
	}
}


void ShareWindow::MakeAway()
{
	_idle = true;
	String away = (_oneTimeAwayStatus.Length() > 0) ? _oneTimeAwayStatus : _awayStatus;
	if (strcmp(_userStatusEntry->Text(), away()) != 0)
	{
	 _revertToStatus = _userStatusEntry->Text();
	 _userStatusEntry->SetText(away());
	 BMessage sMsg(SHAREWINDOW_COMMAND_USER_CHANGED_STATUS);
	 sMsg.AddBool("auto", true);	// so as not reset the _lastInteractionTime
	 PostMessage(&sMsg);
	}
}


void
ShareWindow::LogMessage(LogMessageType type, const char * text, const char * optSessionID, const rgb_color * optTextColor, bool isPersonal, ChatWindow * optEchoTo)
{
	// Ignore messages that match our ignore pattern.
	if ((_ignorePattern.Length() > 0)&&(optSessionID)) {
		RemoteUserItem * user;
		
		if ((_users.Get(optSessionID, user) == B_NO_ERROR) && (MatchesUserFilter(user, _ignorePattern())))
			return;
	}

	const rgb_color watchColor = GetColor(COLOR_WATCH);
	
	if ((optSessionID)&&((optEchoTo == NULL)||(optEchoTo == this))&&(type == LOG_REMOTE_USER_CHAT_MESSAGE)) {
		RemoteUserItem * user;
		
		if (_users.Get(optSessionID, user) == B_NO_ERROR) {
			
			if (isPersonal) {
				BMessage toPriv(LOG_REMOTE_USER_CHAT_MESSAGE);
				toPriv.AddString("text", text);
				toPriv.AddString("sid", optSessionID);

				PrivateChatWindow * nextWin;
				for (HashtableIterator<PrivateChatWindow*, String> iter(_privateChatWindows.GetIterator()); iter.HasData(); iter++) {						
						if ((MatchesUserFilter(user, iter.GetValue().Cstr())) && (nextWin->PostMessage(&toPriv) == B_NO_ERROR))
							_messageWasSentToPrivateChatWindow = true;
				}

				if ((_messageWasSentToPrivateChatWindow == false)&&(_autoPrivPattern.Length() > 0)) {
					// check to see if this message should trigger the automatic creation of a private chat window.
					if (MatchesUserFilter(user, _autoPrivPattern())) {
						DoBeep(SYSTEM_SOUND_PRIVATE_MESSAGE_RECEIVED);
						BMessage pcmsg(SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW);
						pcmsg.AddString("users", optSessionID);
						MessageReceived(&pcmsg);
						LogMessage(type, text, optSessionID, optTextColor, isPersonal, optEchoTo);
						return;
					}
				}
			} else if (MatchesUserFilter(user, _watchPattern())) {
				optTextColor = &watchColor;
				DoBeep(SYSTEM_SOUND_WATCHED_USER_SPEAKS);
			}
		}
	}

	ChatWindow::LogMessage(type, text, optSessionID, optTextColor, isPersonal, optEchoTo);
	_messageWasSentToPrivateChatWindow = false;
}


bool
ShareWindow::OkayToLog(LogMessageType type, LogDestinationType dest, bool isPrivate) const
{
	if (((dest == DESTINATION_LOG_FILE)&&(_toggleFileLogging->IsMarked() == false)) ||
		((dest == DESTINATION_DISPLAY)&&(_messageWasSentToPrivateChatWindow))) return false;

	int whichFilter = -1;
	switch(type)
	{
	 case LOG_INFORMATION_MESSAGE:	 whichFilter = FILTER_INFO_MESSAGES;	break;
	 case LOG_WARNING_MESSAGE:		 whichFilter = FILTER_WARNING_MESSAGES; break; 
	 case LOG_ERROR_MESSAGE:			whichFilter = FILTER_ERROR_MESSAGES;	break;
	 case LOG_LOCAL_USER_CHAT_MESSAGE:
	 case LOG_REMOTE_USER_CHAT_MESSAGE: whichFilter = isPrivate ? FILTER_PRIVATE_MESSAGES : FILTER_CHAT; break;
	 case LOG_USER_EVENT_MESSAGE:		whichFilter = FILTER_USER_EVENTS;	 break;
	 case LOG_UPLOAD_EVENT_MESSAGE:	 whichFilter = FILTER_UPLOADS;		 break;
	}

	return (whichFilter == -1) ? true : _filterItems[dest][whichFilter]->IsMarked();
}

 
void
ShareWindow ::
UpdateLRUMenu(BMenu * menu, const char * lookfor, uint32 what, const char * fieldName, int maxSize, bool caseSensitive, uint32 maxLabelLen)
{
	// Put the query into the query list, or move it to the top
	for (int i=menu->CountItems()-1; i>=0; i--)
	{
	 const char * label;
	 const BMessage * msg = menu->ItemAt(i)->Message();
	 if ((msg)&&(msg->FindString(fieldName, &label) == B_NO_ERROR)&&((caseSensitive ? strcmp(label, lookfor) : strcasecmp(label, lookfor)) == 0))
	 {
		// move this item to the top of the menu
		// switch this item with the first item in the menu
		if (i > 0) menu->AddItem(menu->RemoveItem(i), 0);
		return;
	 }
	}

	// add item to end of menu
	BMessage * msg = new BMessage(what);
	msg->AddString(fieldName, lookfor);
	String lookForStr(lookfor);
	if (lookForStr.Length() > maxLabelLen) lookForStr = lookForStr.Substring(0, maxLabelLen) + B_UTF8_ELLIPSIS;
	menu->AddItem(new BMenuItem(lookForStr(), msg), 0);

	// Don't let the menu get too long though
	while(menu->CountItems() > maxSize) delete menu->RemoveItem(maxSize);
}


void
ShareWindow::SetQueryEnabled(bool e, bool putInQueryMenu)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetQueryEnabled begin\n"));
	if (e != _queryEnabled) {
		_queryEnabled = e;
	
		if (_queryEnabled) {
			TRACE_BESHAREWINDOW(("ShareWindow::SetQueryEnabled _queryEnabled = true\n"));
			// If there is an '@' sign, split the string into separate filename and username queries
			String fileExp(_fileNameQueryEntry->Text());
			String userExp;	// default == empty == "*"

			fileExp = fileExp.Trim();

			if (putInQueryMenu)
				UpdateLRUMenu(_queryMenu, fileExp(), SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY, "query", 20, false, 32);

			int32 atIndex = fileExp.IndexOf('@');

			if (atIndex >= 0) {
				TRACE_BESHAREWINDOW(("ShareWindow::SetQueryEnabled atIndex >= 0\n"));
				if ((uint32) atIndex < fileExp.Length()) {
					userExp = fileExp.Substring(atIndex+1);	// in case they entered a session ID

					// Since the user probably entered a user name instead of a
					// session ID, let's go down our user list and find any matching
					// names, and add their IDs to the search.
					if (HasRegexTokens(userExp()) == false) {
						// Only do this if there is at least one non-numeric digit in the string, though
						bool nonNumericFound = false;
						const char * check = userExp();
						while(*check) {
							if ((*check != ',') && ((*check < '0') || (*check > '9'))) {
								nonNumericFound = true;
								break;
							}
							check++;
						}
		
						if (nonNumericFound)
							userExp = userExp.Prepend("*").Append("*"); 
					}
				
					MakeRegexCaseInsensitive(userExp);
					StringMatcher match(userExp());
				
					RemoteUserItem* next;
					for (HashtableIterator<const char *, RemoteUserItem *> iter(_users.GetIterator()); iter.HasData(); iter++) {
						next = iter.GetValue();
						if (match.Match(next->GetDisplayHandle())) {
							userExp += ",";
							userExp += next->GetSessionID();
						}
					}
				}

				if (atIndex > 0)
					fileExp = fileExp.Substring(0, atIndex);
				else
					fileExp = "";	// i.e. "*"
			}

			// If there are regexp chars in the filename, use it verbatim;
			// otherwise add *'s around the edges to make it a substring search.
			if ((HasRegexTokens(fileExp()) == false) && ((userExp.Length() > 0) || (fileExp.Length() > 0)))
				fileExp = fileExp.Prepend("*").Append("*");

			// Don't allow slashes in the queries as that would screw things up
			fileExp.Replace('/', '?');
			userExp.Replace('/', '?');

			MakeRegexCaseInsensitive(fileExp);

			ClearResults();
			_netClient->StartQuery(userExp.Length() > 0 ? userExp() : "*", fileExp());
		} else
			_netClient->StopQuery();

		UpdateQueryEnabledStatus();
	}
	TRACE_BESHAREWINDOW(("ShareWindow::SetQueryEnabled end\n"));
}


void
ShareWindow::UpdateQueryEnabledStatus()
{
	_enableQueryButton->SetEnabled((_isConnected)&(!_queryEnabled));
	_disableQueryButton->SetEnabled((_isConnected)&(_queryEnabled));
}

void
ShareWindow ::
UpdateConnectStatus(bool titleToo)
{
	const char * sname = _serverEntry->Text();

	bool c = ((_isConnected) || (_isConnecting));
	_connectMenuItem->SetEnabled(!c);
	
	if (sname[0] == '\0')
		sname = "???";
	
	_disconnectMenuItem->SetEnabled((c) || (_autoReconnectRunner != NULL));

	_firewalled->SetMarked(_netClient->GetFirewalled());

	if (titleToo)
		UpdateTitleBar();

	char buf[200];
	strcpy(buf, str(STR_CONNECT_TO));
	strncat(buf, sname, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	_connectMenuItem->SetLabel(buf);

	UpdateDownloadButtonStatus();
}


void
ShareWindow::UpdateTitleBar()
{
	TRACE_BESHAREWINDOW(("ShareWindow::UpdateTitleBar begin\n"));
	String title("BeShare");
	const String & custom = GetCustomWindowTitle();
	if (custom.Length() > 0)
		title = custom;
	else {
		if ((_isConnecting) && (_isConnected == false)) {
			title += str(STR_CONNECTING_TO);
			title += _connectedTo;
	 	} else if (_isConnected) {
			const char * sid = _netClient->GetLocalSessionID();
			if (sid[0] != '\0') {
				title += " - ";
				title += str(STR_USER_NUMBER);
				title += sid;
				title += ' ';
			}
			
			title += '(';
			title += _userNameEntry->Text();
			title += ')';
			
			title += str(STR_CONNECTED_TO);
			title += _connectedTo;
			
			uint32 count = _netClient->GetSharedFileCount();
			if (count > 0) {
				title += ' ';
				char buf[256];
				sprintf(buf, str(STR_SHARING_PERCENTI_LOCAL_FILES), (int) _netClient->GetSharedFileCount());
				title += buf; 
			}
	 	} else {
	 		uint32 totalNumFiles = 0, pageBase = 0;
			
			for (int i=_resultsPages.GetNumItems()-1; i>=0; i--) {
				uint32 pageSize = _resultsPages[i]->GetNumItems();
				
				if (i < (int)_currentPage)
					pageBase += pageSize;
				totalNumFiles += pageSize;
			}
			if (totalNumFiles > 0) {
				uint32 numFiles = _resultsView->CountItems();
				char buf[256];
			
				if (totalNumFiles > numFiles)
					sprintf(buf, str(STR_PERCENTLU_DASH_PERCENTLU_OF_PERCENTLU_RESULTS_SHOWN), pageBase+1, pageBase+numFiles, totalNumFiles);
				else 
					sprintf(buf, str(STR_PERCENTLU_RESULTS_SHOWN), numFiles);
			
				title += buf;
				GetByteSizeString(_bytesShown, buf);
				title += buf;
				title += ')';
			}
		}
	}

	SetTitle(title());

	UpdatePagingButtons();
	TRACE_BESHAREWINDOW(("ShareWindow::UpdateTitleBar end\n"));
}


void
ShareWindow::UpdateDownloadButtonStatus()
{
	TRACE_BESHAREWINDOW(("ShareWindow::UpdateDownloadButtonStatus begin\n"));
	_requestDownloadsButton->SetEnabled((_isConnected)&&(_resultsView->CurrentSelection() >= 0));

	bool deadTransfersPresent = false;
	for (int i=_transferList->CountItems()-1; i>=0; i--) {
		ShareFileTransfer* next = (ShareFileTransfer *)_transferList->ItemAt(i);
		if (next->IsFinished()) {
			deadTransfersPresent = true;
			break;
		}
	}
	_clearFinishedDownloadsButton->SetEnabled(deadTransfersPresent);
	_cancelTransfersButton->SetEnabled(_transferList->CurrentSelection() >= 0);
	TRACE_BESHAREWINDOW(("ShareWindow::UpdateDownloadButtonStatus end\n"));
}


bool
ShareWindow::QuitRequested()
{
	if (_enableQuitRequester) {
		int numInProgress = 0;
		
		for (int i=_transferList->CountItems()-1; i>=0; i--) {
			ShareFileTransfer* next = (ShareFileTransfer*)_transferList->ItemAt(i);
			
			if (next->IsFinished() == false)
				numInProgress++;
		}
	 
		if (numInProgress > 0) {
			char temp[512];
			sprintf(temp, str(STR_TRANSFERS_IN_PROGRESS_ARE_YOU_SURE_YOU_WANT_TO_QUIT), numInProgress);
		
			if ((new BAlert("BeShare", temp, str(STR_QUIT), str(STR_DONT_QUIT)))->Go())
				return false;
		}
	
		if(_isConnected)
			_netClient->DisconnectFromServer();
		
		be_app->PostMessage(B_QUIT_REQUESTED);
	}
	
	if (_idle) {
		_idle = false;
		BMessage msg(SHAREWINDOW_COMMAND_UNIDLE);
		MessageReceived(&msg);	// important to do this synchronously, so we can't just PostMessage()
	}
	return true;
}


void 
ShareWindow ::
SendOnLogins()
{
	// execute the onLogin script, if any
	int numLines = _onLoginStrings.GetNumItems();
	for (int i=0; i<numLines; i++) SendChatText(_onLoginStrings[i], NULL);
}

void
ShareWindow ::
RemoveServerItem(const char * serverName, bool quiet)
{
	for (int i=_serverMenu->CountItems()-1; i>=0; i--) 
	{
	 BMenuItem * item = _serverMenu->ItemAt(i);
	 if (strcasecmp(item->Label(), serverName) == 0) 
	 {
		if (quiet == false)
		{ 
			String serverLabel(serverName);
			if (serverLabel.Length() > 90) serverLabel = serverLabel.Substring(0,90);
			char buf[256];
			sprintf(buf, str(STR_REMOVED_SERVER), serverLabel());
			LogMessage(LOG_INFORMATION_MESSAGE, buf);
		}
		_serverMenu->RemoveItem(i);
		delete item;
		return;
	 }
	}
}


// The methods below are called by our ShareNetClient at the appropriate times.
void 
ShareWindow::SetConnectStatus(bool isConnecting, bool isConnected)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetConnectStatus begin\n"));
	if ((!_isConnected) && (isConnected)) {
		TRACE_BESHAREWINDOW(("ShareWindow::SetConnectStatus if first\n"));
		LogMessage(LOG_INFORMATION_MESSAGE, str(STR_CONNECTION_ESTABLISHED));

		AddServerItem(_connectedTo(), false, 0);

		_netClient->SetUploadStats(CountUploadSessions(), _maxSimultaneousUploadSessions, true);

		if (_queryOnConnect.Length() > 0) {
			SetQuery(_queryOnConnect());
			_queryOnConnect = "";	// we only want to do this once!
		}
	} else if ((!_isConnecting) && (isConnecting))
		LogMessage(LOG_INFORMATION_MESSAGE, str(STR_CONNECTING_TO_SERVER_DOTDOTDOT));
	else if ((isConnecting == false) && (isConnected == false)) {
		if (_isConnected)
			LogMessage(LOG_ERROR_MESSAGE, str(STR_YOU_ARE_NO_LONGER_CONNECTED_TO_THE_MUSCLE_SERVER));
		else if (_isConnecting)
			LogMessage(LOG_ERROR_MESSAGE, str(STR_CONNECTION_TO_SERVER_FAILED));
	}
	
	TRACE_BESHAREWINDOW(("ShareWindow::SetConnectStatus if middle\n"));
	
	_isConnecting = isConnecting;
	_isConnected	= isConnected;

	// If we're not connected anymore, make sure the display is clear
	if (_isConnected == false) {
		TRACE_BESHAREWINDOW(("ShareWindow::SetConnectStatus if middle 1\n"));
		ClearUsers();
		TRACE_BESHAREWINDOW(("ShareWindow::SetConnectStatus if middle 2\n"));
		SetQueryEnabled(false);
	}
	TRACE_BESHAREWINDOW(("ShareWindow::SetConnectStatus if last\n"));
	UpdateConnectStatus(true);
	UpdateQueryEnabledStatus();
	TRACE_BESHAREWINDOW(("ShareWindow::SetConnectStatus end\n"));
}


void 
ShareWindow::PutUser(const char* sessionID, const char* userName, const char* hostName, int port, bool* isBot, uint64 installID, const char* client, bool* supportsPartialHash)
{
	TRACE_BESHAREWINDOW(("ShareWindow::PutUser begin\n"));
	bool addName = true;
	RemoteUserItem * user;
	if (_users.Get(sessionID, user) == B_NO_ERROR) {
		if ((userName == NULL)
			|| (strcmp(userName, user->GetDisplayHandle()) == 0))
			addName = false;	// no change needed!
		else {
			BMessage removeOld(PrivateChatWindow::PRIVATE_WINDOW_REMOVE_USER);
			removeOld.AddString("id", user->GetSessionID());
			SendToPrivateChatWindows(removeOld, NULL);
		}
	} else {
		user = new RemoteUserItem(this, sessionID);
		TRACE_BESHAREWINDOW(("ShareWindow::PutUser Users %s\n", user->GetSessionID()));
		_users.Put(user->GetSessionID(), user);
		_usersView->AddItem(user);
		_usersView->SortItems();
	}

	bool wasReadyForRestart = ((user->GetInstallID() > 0)&&((user->GetPort() > 0)||(user->GetFirewalled())));
	
	if (userName)
		user->SetHandle(userName, SubstituteLabelledURLs(userName).Trim()());
	
	if (hostName)
		user->SetHostName(hostName);
	
	if (port >= 0)
		user->SetPort(port);
	
	if (isBot) user->SetIsBot(*isBot);

	if ((installID > 0) && (user->GetInstallID() == 0))
		user->SetInstallID(installID);

	if (client)
		user->SetClient(client, SubstituteLabelledURLs(client).Trim()());

	if (supportsPartialHash)
		user->SetSupportsPartialHash(*supportsPartialHash);

	bool isReadyForRestart = ((user->GetInstallID() > 0) && ((user->GetPort() > 0) || (user->GetFirewalled())));

	if ((wasReadyForRestart == false) && (isReadyForRestart))
		RestartDownloadsFor(user);

	if (addName) {
		BMessage msg(PrivateChatWindow::PRIVATE_WINDOW_ADD_USER);
		msg.AddString("id", user->GetSessionID());
		msg.AddString("name", user->GetDisplayHandle());
		SendToPrivateChatWindows(msg, user);
	}
	TRACE_BESHAREWINDOW(("ShareWindow::PutUser end\n"));
}


// Goes through the list of transfers, and any transfers that are non-finished downloads from
// the same installID as (user), we'll restart at (user)'s new IP address.
void 
ShareWindow ::
RestartDownloadsFor(const RemoteUserItem * user)
{
	// Save any active, pending, or errored-out downloads; maybe we can continue them later.
	for (int i=_transferList->CountItems()-1; i>=0; i--)
	{
	 ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(i);
	 uint64 nrid = next->GetRemoteInstallID();
	 if (((nrid > 0)&&(nrid == user->GetInstallID()))&&
		 (next->IsUploadSession() == false)&&
		 (next->IsConnected() == false)&&
		 (next->IsConnecting() == false)&&
		 (next->ErrorOccurred())&&
		 (next->SetLocalSessionID(_netClient->GetLocalSessionID()) == B_NO_ERROR)&&
		 (SetupNewDownload(user, next, next->IsAcceptSession()) == B_NO_ERROR)) next->RestartSession();
	}
	DequeueTransferSessions();
}

void
ShareWindow::SetUserBandwidth(const char* sessionID, const char* label, uint32 bps)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserBandwidth begin\n"));
	PutUser(sessionID, NULL, NULL, -1, NULL, 0, NULL, NULL);	// make sure the RemoteUserItem is present!
	RemoteUserItem * user;
	
	if (_users.Get(sessionID, user) == B_NO_ERROR)
		user->SetBandwidth(label, bps);
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserBandwidth end\n"));
}


void
ShareWindow::SetUserStatus(const char* sessionID, const char* status)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserStatus begin\n"));
	PutUser(sessionID, NULL, NULL, -1, NULL, 0, NULL, NULL);	// make sure the RemoteUserItem is present!
	
	RemoteUserItem* user;
	if (_users.Get(sessionID, user) == B_NO_ERROR)
		user->SetStatus(status, SubstituteLabelledURLs(status).Trim()());

	TRACE_BESHAREWINDOW(("ShareWindow::SetUserStatus end\n"));
}


void
ShareWindow::SetUserUploadStats(const char* sessionID, uint32 cur, uint32 max)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserUploadStats begin\n"));
	PutUser(sessionID, NULL, NULL, -1, NULL, 0, NULL, NULL);	// make sure the RemoteUserItem is present!
	
	RemoteUserItem* user;
	if (_users.Get(sessionID, user) == B_NO_ERROR)
		user->SetUploadStats(cur, max);
	
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserUploadStats end\n"));
}

void
ShareWindow::SetUserIsFirewalled(const char * sessionID, bool fw)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserIsFirewalled begin\n"));
	PutUser(sessionID, NULL, NULL, -1, NULL, 0, NULL, NULL);	// make sure the RemoteUserItem is present!
	
	RemoteUserItem* user;
	if (_users.Get(sessionID, user) == B_NO_ERROR)
		user->SetFirewalled(fw);
	
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserIsFirewalled end\n"));
}


void
ShareWindow::SetUserFileCount(const char * sessionID, int32 fc)
{
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserFileCount begin\n"));
	PutUser(sessionID, NULL, NULL, -1, NULL, 0, NULL, NULL);	// make sure the RemoteUserItem is present!
	
	RemoteUserItem* user;
	if (_users.Get(sessionID, user) == B_NO_ERROR)
		user->SetNumSharedFiles(fc);
	
	TRACE_BESHAREWINDOW(("ShareWindow::SetUserFileCount end\n"));
}


void 
ShareWindow ::
RemoveUser(const char * sessionID)
{
	RemoteUserItem * user;
	if (_users.Remove(sessionID, user) == B_NO_ERROR) 
	{
	 // Any downloads who are awaiting callbacks from this user might as well forget it now
	 // he can't get the message and call us back if he's left the server!
	 for (int i=_transferList->CountItems()-1; i>=0; i--)
	 {
		ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(i);
		if ((next->IsAccepting())&&(strcmp(next->GetRemoteSessionID(), sessionID) == 0)) next->AbortSession(true, true);
	 }
	
	 _usersView->RemoveItem(user);
	 BMessage msg(PrivateChatWindow::PRIVATE_WINDOW_REMOVE_USER);
	 msg.AddString("id", user->GetSessionID());
	 SendToPrivateChatWindows(msg, NULL);
	 delete user;
	}
}


void
ShareWindow::SendToPrivateChatWindows(BMessage & msg, const RemoteUserItem * matchesItem) 
{
	TRACE_BESHAREWINDOW(("ShareWindow::SendToPrivateChatWindows begin\n"));
	PrivateChatWindow * next;
	for (HashtableIterator<PrivateChatWindow*, String> iter(_privateChatWindows.GetIterator()); iter.HasData(); iter++) {
		next = iter.GetKey();
		
		String filter = iter.GetValue();	// don't inline this!
		if ((matchesItem == NULL)
			|| (MatchesUserFilter(matchesItem, filter.Cstr())))
			next->PostMessage(&msg);
	}	
	TRACE_BESHAREWINDOW(("ShareWindow::SendToPrivateChatWindows end\n"));
}


void 
ShareWindow::PutResult(const char* sessionID, const char* fileName, bool isFirewalled, const MessageRef& fileInfo)
{
	TRACE_BESHAREWINDOW(("ShareWindow::PutResult begin\n"));
	PutUser(sessionID, NULL, NULL, -1, NULL, 0, NULL, NULL);	// make sure the RemoteUserItem is present!
	
	RemoteUserItem* user;
	if (_users.Get(sessionID, user) == B_NO_ERROR) {
		user->SetFirewalled(isFirewalled);
		user->PutFile(fileName, fileInfo);
	}
	
	TRACE_BESHAREWINDOW(("ShareWindow::PutResult end\n"));
}


void 
ShareWindow::RemoveResult(const char* sessionID, const char* fileName)
{
	RemoteUserItem* user;
	
	if (_users.Get(sessionID, user) == B_NO_ERROR)
		user->RemoveFile(fileName);
}

void 
ShareWindow::ClearResults()
{
	TRACE_BESHAREWINDOW(("ShareWindow::ClearResults begin\n"));
	_resultsView->MakeEmpty();	// for efficiency
	
	TRACE_BESHAREWINDOW(("ShareWindow::ClearResults before loop\n"));
	for (int i = _resultsPages.GetNumItems()-1; i >= 0; i--)
		delete _resultsPages[i];
	
	_resultsPages.Clear();
	SwitchToPage(0);
	 
	TRACE_BESHAREWINDOW(("ShareWindow::ClearResults before loop 2\n"));

	RemoteUserItem * next;
	for (HashtableIterator<const char *, RemoteUserItem *> iter(_users.GetIterator()); iter.HasData(); iter++) {
		//(void) watch_node(&iter.GetKey(), B_STOP_WATCHING, this);
		next = iter.GetValue();
		if (next != NULL)
			next->ClearFiles();
	}
	TRACE_BESHAREWINDOW(("ShareWindow::ClearResults after loop 2\n"));	
	_bytesShown = 0LL;
	UpdateTitleBar();
	TRACE_BESHAREWINDOW(("ShareWindow::ClearResults end\n"));
}


void
ShareWindow::FileTransferConnected(ShareFileTransfer * who)
{
	TRACE_BESHAREWINDOW(("ShareWindow::FileTransferConnected begin\n"));
	_idleSendPending = false;
	RefreshTransferItem(who);
	UpdateDownloadButtonStatus();
	TRACE_BESHAREWINDOW(("ShareWindow::FileTransferConnected end\n"));
}


void
ShareWindow::BeginBatchFileResultUpdate()
{
	// empty
}


void
ShareWindow ::
EndBatchFileResultUpdate()
{
	int tempSize = _tempAddList.CountItems();
	if (tempSize > 0)
	{
	 BList addToDisplay;

	 uint32 addPage = 0;
	 for (int i=0; i<tempSize; i++)
	 {
		if (addPage >= _resultsPages.GetNumItems()) _resultsPages.AddTail(new Hashtable<RemoteFileItem *, bool>);
		Hashtable<RemoteFileItem *, bool> * table = _resultsPages[addPage];
		if (table->GetNumItems() >= _pageSize)
		{
			addPage++;	// allocate a new page on the next loop-through
			i--;		// then do this same item again
		}
		else
		{
			RemoteFileItem * nextItem = (RemoteFileItem *) _tempAddList.ItemAt(i);
			table->Put(nextItem, nextItem);
			if (addPage == _currentPage) addToDisplay.AddItem(nextItem);
		}
	 }

	 AddResultsItemList(addToDisplay);
	 UpdateTitleBar();
	 _tempAddList.MakeEmpty();
	}
}


void
ShareWindow ::
AddResultsItemList(const BList & list)
{
	if (list.CountItems() > 0)
	{
	 DisableUpdates();
		_resultsView->AddList((BList *)&list);
		SortResults();
	 EnableUpdates();
	}
}

void
ShareWindow::FileTransferDisconnected(ShareFileTransfer * who)
{
	TRACE_BESHAREWINDOW(("ShareWindow::FileTransferDisconnected begin\n"));
	// Uploads we just immediately get rid of, downloads stick around till the user removes them.
	if ((who->IsUploadSession())
		|| ((_autoClearCompletedDownloads->IsMarked()) && (who->ErrorOccurred() == false))) {
		// Can't just delete it here because it is calling me! So I'll send myself a message to do it later
		BMessage msg(SHAREWINDOW_COMMAND_REMOVE_SESSION);
		msg.AddPointer("who", who);
		PostMessage(&msg);
	} else {
		DequeueTransferSessions();
		RefreshTransferItem(who);
	}

	UpdateDownloadButtonStatus();

	// Don't send the idle chat string quite yet -- it may be that someone connects again in the next few seconds
	// (i.e. someone who is downloading things one per box). Instead, I set a flag, and if nothing has happened 
	// by the next moribund-connection check, I'll send the /onidle string then.
	if ((_onIdleString.Length() > 0)
		&& (CountActiveSessions(true, NULL) == 0)
		&& (CountActiveSessions(false, NULL) == 0))
			_idleSendPending = true;
	
	TRACE_BESHAREWINDOW(("ShareWindow::FileTransferDisconnected end\n"));
}


const char * 
ShareWindow ::
GetFileCellText(const RemoteFileItem * item, int32 columnIndex) const
{
	return ((ShareColumn *)_resultsView->ColumnAt(columnIndex))->GetFileCellText(item);
}

const BBitmap *
ShareWindow ::
GetBitmap(const RemoteFileItem * item, int32 /*columnIndex*/) const
{
	const BBitmap * bmp = NULL;

	// For now, the only bitmap supported is the MIME type icon
	const char * mimeString;
	if (item->GetAttributes().FindString("beshare:Kind", &mimeString) == B_NO_ERROR)
	{
	 ShareMIMEInfo * mi;
	 if (_mimeInfos.Get(mimeString, mi) == B_NO_ERROR) bmp = mi->GetIcon();
	}
	return bmp ? bmp : &_defaultBitmap;
}

const BBitmap *
ShareWindow ::
GetBitmap(const char * mimeString)
{
	ShareMIMEInfo * mi = mimeString ? CacheMIMETypeInfo(mimeString) : NULL;
	const BBitmap * bmp = mi ? mi->GetIcon() : NULL;
	return bmp ? bmp : &_defaultBitmap;
}

ShareMIMEInfo *
ShareWindow ::
CacheMIMETypeInfo(const char * mimeString)
{
	ShareMIMEInfo * ret;
	if (_mimeInfos.Get(mimeString, ret) == B_ERROR)
	{
	 const char * label = mimeString;
	 char buf[B_MIME_TYPE_LENGTH];
	 BMimeType mt(mimeString);
	 if ((mt.InitCheck()==B_NO_ERROR)&&(mt.GetShortDescription(buf) == B_NO_ERROR)) label = buf;

	 ShareMIMEInfo * newInfo = new ShareMIMEInfo(label, mimeString);
	 _mimeInfos.Put(newInfo->GetMIMEString(), newInfo);
	 _emptyMimeInfos.Put(newInfo, true);	// because it's not in the menu yet (it goes there when we have something to put in it)
	 ret = newInfo;
	}
	return ret;
}


void
ShareWindow::AddFileItem(RemoteFileItem* item)
{
	TRACE_BESHAREWINDOW(("ShareWindow::AddFileItem begin\n"));
	MASSERT(item, "AddFileItem:	no item!?");
	_tempAddList.AddItem(item);

	const Message & attrs = item->GetAttributes();
	ShareMIMEInfo* optMimeInfo = NULL;
	const char* mimeString = NULL;
	if (attrs.FindString("beshare:Kind", &mimeString) == B_NO_ERROR)
		optMimeInfo = CacheMIMETypeInfo(mimeString);

	int64 s;
	if (attrs.FindInt64("beshare:File Size", &s) == B_NO_ERROR)
		_bytesShown += s;

	for (MessageFieldNameIterator iter = attrs.GetFieldNameIterator(); iter.HasData(); iter++) {
		CreateColumn(optMimeInfo, iter.GetFieldName().Cstr(), true);
	}
	
	TRACE_BESHAREWINDOW(("ShareWindow::AddFileItem end\n"));
}


void
ShareWindow::CreateColumn(ShareMIMEInfo* optMimeInfo, const char* attrName, bool remote)
{
	TRACE_BESHAREWINDOW(("ShareWindow::CreateColumn begin\n"));
	if (_columns.ContainsKey(attrName) == false) {
		const char * label = attrName;
		int type = ShareColumn::ATTR_MISC;

		if ((remote == false) && (attrName[0] == SPECIAL_COLUMN_CHAR)) {
			type = attrName[1]-'0';
			label += 2;
		} else {
		
			if (strncmp(attrName, "beshare:", 8) == 0) {
				// A BeShare var; handle these a bit differently	
				label += 8;
				
				if (strcmp(label, "Kind") == 0)
					label = str(STR_KIND);		// hacked-in language support :^P
				else if (strcmp(label, "File Size") == 0)
					label = str(STR_FILE_SIZE);
				else if (strcmp(label, "Modification Time") == 0)
					label = str(STR_MODIFICATION_TIME);
				else if (strcmp(label, "Path")==0)
					label = str(STR_PATH);
				
				optMimeInfo = NULL;	// these vars are not type specific
			} else {
				// It's a genuine attribute; try to find a better name for it
				if (optMimeInfo) {
					const char* desc = optMimeInfo->GetAttributeDescription(attrName);
			 		
			 		if (desc)
			 			label = desc;
				}
			}
		}

		ShareColumn* column = new ShareColumn(type, attrName, label, 80.0f);
		_columns.Put(column->GetAttributeName(), column);

		BMessage * msg = new BMessage(SHAREWINDOW_COMMAND_TOGGLE_COLUMN);
		msg->AddString("attrib", column->GetAttributeName());
		BMenuItem * mi = new BMenuItem(column->GetLabel(), msg, 0);

		if (optMimeInfo) {
			if (_emptyMimeInfos.ContainsKey(optMimeInfo)) {
				if (_firstUserDefinedAttribute) {
					_attribMenu->AddSeparatorItem();
					_firstUserDefinedAttribute = false;
				}
				_attribMenu->AddItem(optMimeInfo);	// only add it to our menu when it finally gets something to hold
				_emptyMimeInfos.Remove(optMimeInfo);	// now the BMenu is responsible for deleting it
			}
			optMimeInfo->AddItem(mi);
		} else
			_attribMenu->AddItem(mi);

		_attribMenuItems.Put(column->GetAttributeName(), mi);
		if (_activeAttribs.ContainsKey(attrName))
			PostMessage(msg);
	}
	TRACE_BESHAREWINDOW(("ShareWindow::CreateColumn end\n"));
}


void
ShareWindow::RemoveFileItem(RemoteFileItem * item)
{
	int64 s;
	if (item->GetAttributes().FindInt64("beshare:File Size", &s) == B_NO_ERROR) _bytesShown -= s;

	for (int i=_resultsPages.GetNumItems()-1; i>=0; i--)
	{
	 Hashtable<RemoteFileItem *, bool> * nextTable = _resultsPages[i];
	 if (nextTable->Remove(item) == B_NO_ERROR)
	 {
		if (i == (int)_currentPage) _resultsView->RemoveItem(item);
		if (nextTable->GetNumItems() == 0)
		{ 
			_resultsPages.RemoveItemAt(i);
			delete nextTable;
				if (i < (int)_currentPage) _currentPage--;
			else if (i == (int)_currentPage) SwitchToPage(((int)_currentPage)-1);
		}
		break;
	 }
	}
}

void
ShareWindow::SwitchToPage(int page)
{
	int numPages = _resultsPages.GetNumItems();
	if (page >= numPages) page = numPages-1;
	if (page < 0) page = 0;
	_currentPage = (uint32) page;

	_resultsView->MakeEmpty();

	if (numPages > 0)
	{
	 Hashtable<RemoteFileItem *, bool> * table = _resultsPages[page];
	 HashtableIterator<RemoteFileItem *, bool> iter = table->GetIterator();
	 RemoteFileItem * next;
	 BList tempList(table->GetNumItems());
	 while((next = iter.GetKey()) == B_NO_ERROR) tempList.AddItem(next);
	 AddResultsItemList(tempList);
	}
	UpdateTitleBar();
}

void
ShareWindow::RefreshTransferItem(ShareFileTransfer * item)
{
	TRACE_BESHAREWINDOW(("ShareWindow::RefreshTransferItem begin\n"));
	// Gotta call DrawItem() directly, since InvalidateItem() causes flicker
	item->DrawItem(_transferList, _transferList->ItemFrame(_transferList->IndexOf(item)), true);
	_transferList->Flush();
	TRACE_BESHAREWINDOW(("ShareWindow::RefreshTransferItem end\n"));
}


void
ShareWindow ::
RefreshFileItem(RemoteFileItem * item)
{
	for (int i=_resultsPages.GetNumItems()-1; i>=0; i--)
	{
	 if (_resultsPages[i]->ContainsKey(item))
	 {
		if (i == (int)_currentPage) _resultsView->InvalidateItem(_resultsView->IndexOf(item));
		break;
	 }
	}
}

void
ShareWindow ::
RefreshUserItem(RemoteUserItem * item)
{
	_usersView->InvalidateItem(_usersView->IndexOf(item));
	_usersView->SortItems();
}

void
ShareWindow ::
RefreshTransfersFor(RemoteUserItem * user)
{
	for (int i=_transferList->CountItems()-1; i>=0; i--)
	{
	 ShareFileTransfer * next = (ShareFileTransfer *) _transferList->ItemAt(i);
	 if ((user == NULL)||(strcmp(next->GetRemoteSessionID(), user->GetSessionID()) == 0)) 
	 {
		next->UpdateRemoteUserName();
		RefreshTransferItem(next);
	 }
	}
}


int
ShareWindow ::
CompareFunc(const CLVListItem* item1, const CLVListItem* item2, int32 sortKey)
{
	return ((RemoteFileItem *)item1)->Compare(((RemoteFileItem *)item2), sortKey);	
}

int ShareWindow ::
UserCompareFunc(const CLVListItem * i1, const CLVListItem * i2, int32 sortKey)
{
	return ((const RemoteUserItem *) i1)->Compare((const RemoteUserItem *)i2, sortKey);
}

int 
ShareWindow ::
Compare(const RemoteFileItem * rf1, const RemoteFileItem * rf2, int32 sortKey) const
{
	return ((const ShareColumn *)_resultsView->ColumnAt(sortKey))->Compare(rf1, rf2);
}


void 
ShareWindow ::
ResetLayout()
{
	_mainSplit->SetSwapped(false);
	_resultsTransferSplit->SetSwapped(false);
	_chatUsersSplit->SetSwapped(false);

	_mainSplit->SetAlignment(B_HORIZONTAL);
	_resultsTransferSplit->SetAlignment(B_VERTICAL);
	_chatUsersSplit->SetAlignment(B_VERTICAL);

	_resultsTransferSplit->SetBarPosition(BPoint(_resultsTransferSplit->Bounds().Width()*0.75f, _resultsTransferSplit->Bounds().Height()*0.75f));
	_chatUsersSplit->SetBarPosition(BPoint(_chatUsersSplit->Bounds().Width()*0.78f, _chatUsersSplit->Bounds().Height()*0.78f));

#ifdef SAVE_BEOS
	const float mainPos = 0.75f;
#else
	const float mainPos = 0.5f;
#endif
	_mainSplit->SetBarPosition(BPoint(_mainSplit->Bounds().Width()*0.5f,_mainSplit->Bounds().Height()*mainPos));
}


// Pattern matching for BGA's tab-completion
int 
ShareWindow::MatchUserName(const char * un, String & result, const char * optMatchFilter) const
{
	int matchCount = 0;
	RemoteUserItem* next;
	for (HashtableIterator<const char *, RemoteUserItem *> iter(_users.GetIterator()); iter.HasData(); iter++) {
		next = iter.GetValue();
		String userName(next->GetDisplayHandle());
		userName = userName.ToLowerCase().Trim();
		if (((optMatchFilter == NULL) || (MatchesUserFilter(next, optMatchFilter))) && (userName.StartsWith(un))) {
			matchCount++;
		
			if (matchCount == 1) {
				result = next->GetDisplayHandle();
			} else {
				// oops!	Several matches!	Chop any chars out of (result) that aren't in both names!
				String temp(result); 
				temp = temp.ToLowerCase();
				for (uint32 i=0; i<temp.Length(); i++) {
					// Gotta compare with case-insensitivity, while maintaining the correct case
					if (temp()[i] != userName()[i]) {
						result = result.Substring(0, i);
						break;
					}
				}
			}
		}
	}
	return matchCount;
}


status_t
ShareWindow ::
DoTabCompletion(const char * origText, String & returnCompletedText, const char * optMatchFilter) const
{
	// Do it all in lower case, for case insensitivity
	String text(origText);
	text = text.ToLowerCase();

	// Compile a list of pointers to beginnings-of-words in the user's chat string
	Queue<const char *> words;
	bool inSpace = true;
	const char * next = text();
	while(*next)
	{
	 if (inSpace)
	 {
		if ((*next != ' ')&&(*next != '\t'))
		{
			words.AddTail(next);
			inSpace = false;
		}
	 }
	 else if ((*next == ' ')||(*next == '\t')) inSpace = true;
 
	 next++;
	}
		
	// Now try matching, starting with the last word.
	// If no match is found, try the last two words, and so on.
	const char * startAt = NULL, * backupStartAt = NULL;
	String matchString, backupMatchString;
	for (int i=words.GetNumItems()-1; i>=0; i--)
	{
	 const char * matchAt = words[i];
	 String resultName;
	 int numMatches = MatchUserName(words[i], resultName, optMatchFilter); 
	 if (numMatches == 1)
	 {
		matchString = resultName;	// found a unique match!	We're done!
		startAt = matchAt;
		break;
	 }
	 else if (numMatches > 1)
	 {
		backupMatchString = resultName;	// found several matches; keep trying for a single
		backupStartAt = matchAt;		// but we'll use this if nothing else
	 }
	 matchString.Prepend(" ");
	}

	if (startAt == NULL)
	{
	 startAt = backupStartAt;
	 matchString = backupMatchString;
	 if (startAt) DoBeep(SYSTEM_SOUND_AUTOCOMPLETE_FAILURE);	// remind the user that this isn't a full match
	}
	if (startAt)
	{
	 returnCompletedText = origText;
	 returnCompletedText = returnCompletedText.Substring(0, startAt-text());
	 returnCompletedText += matchString;
	 return B_NO_ERROR;
	}
	return B_ERROR;
}


bool
ShareWindow::GetFirewalled() const
{
	return _netClient->GetFirewalled();
}


status_t
ShareWindow::ParseUserTargets(const char * text, Hashtable<RemoteUserItem *, String> & sendTo, String & setTargetStr, String & setRestOfString)
{
	StringTokenizer wholeStringTok(text, " ");
	String restOfString2(wholeStringTok.GetRemainderOfString());	// store this for later full-name matching
	restOfString2.Replace(CLUMP_CHAR, ' ');
	const char * w2 = wholeStringTok.GetNextToken();
	
	if (w2) {
		setTargetStr = w2;
		setTargetStr.Replace(CLUMP_CHAR, ' ');
		w2 = setTargetStr();

		setRestOfString = wholeStringTok.GetRemainderOfString();

		// Compile setTargetStr into a list of comma-separated clauses...
		StringTokenizer tok(w2, ",");
		Queue<String> clauses;
		const char * next;
		while((next = tok.GetNextToken()) != NULL)
			clauses.AddTail(String(next).Trim());

		// Now, for each clause, we first want to see if it is a session ID.
		// session ID has priority over other ID methods, as it disallows 'imposters'.
		for (int i=clauses.GetNumItems()-1; i>=0; i--) {
			RemoteUserItem * user;
			if (_users.Get(clauses[i](), user) == B_NO_ERROR) {
				sendTo.Put(user, setRestOfString);
				clauses.RemoveItemAt(i);
			}
		}

		// Any clauses still left over, we will try to match against the user names.
		for (int j=clauses.GetNumItems()-1; j>=0; j--) {
			String tstr(clauses[j]);
			tstr.Trim();
			MakeRegexCaseInsensitive(tstr);
			StringMatcher sm(tstr());

			bool foundMatches = false;
			RemoteUserItem * user;
			
			for (HashtableIterator<const char *, RemoteUserItem *> iter(_users.GetIterator()); iter.HasData(); iter++) {
				user = iter.GetValue();
				String userName = String(user->GetDisplayHandle()).Trim();
			
				if ((userName.Length() > 0)&&(sm.Match(userName()))) {
					sendTo.Put(user, setRestOfString);
					foundMatches = true;
				}
			}

			if (foundMatches)
				clauses.RemoveItemAt(j);
		}
	 
	 	// If we *still* haven't found any matches, try a full-string match.
	 	// This way, we can support tab-completed names with spaces.
		if (sendTo.GetNumItems() == 0) {
			RemoteUserItem * user;
			for (HashtableIterator<const char *, RemoteUserItem *> iter(_users.GetIterator()); iter.HasData(); iter++) {
					user = iter.GetValue();
					String userName = String(user->GetDisplayHandle()).Trim();
				if ((userName.Length() > 0)&&(restOfString2.StartsWith(userName))&&(restOfString2.Substring(userName.Length()).StartsWith(" "))) {
					// Match this name!
					sendTo.Put(user, restOfString2.Substring(strlen(user->GetDisplayHandle())).Trim());
					setTargetStr = user->GetDisplayHandle();
				}
			}
		}
		return B_NO_ERROR;
	} else 
		return B_ERROR;
}


void
ShareWindow::SendOutMessageOrPing(const String & text, ChatWindow * optEchoTo, bool isPing)
{
	String targetStr, restOfString;
	Hashtable<RemoteUserItem *, String> sendTo;
	if (ParseUserTargets(text()+(isPing ? 6 : 5), sendTo, targetStr, restOfString) == B_NO_ERROR) {
		if (sendTo.GetNumItems() > 0) { 
			String pinging;
			RemoteUserItem * user;
			bool first = true;
			bool showAllTargets = (optEchoTo ? optEchoTo : this)->ShowMessageTargets();
			
			for (HashtableIterator<RemoteUserItem *, String> iter(sendTo.GetIterator()); iter.HasData(); iter++) {
				user = iter.GetKey();
				const char * sendText = iter.GetValue().Cstr();
				const char * sid = user->GetSessionID();

				if (isPing) {
					_netClient->SendPing(sid);
				
					if (pinging.Length() > 0)
						pinging += ", ";

					pinging += sid;
				} else
					_netClient->SendChatMessage(user->GetSessionID(), sendText);

				if ((isPing == false) && ((showAllTargets) || (first)))
					LogMessage(LOG_LOCAL_USER_CHAT_MESSAGE, sendText, user->GetSessionID(), NULL, (isPing==false), optEchoTo);
				
				first = false;
			}
			
			if (isPing) {
				pinging = pinging.Prepend(str(STR_SENT_PING_REQUEST_TO));
				LogMessage(LOG_INFORMATION_MESSAGE, pinging(), NULL, NULL, false, optEchoTo);
			} else
				_lastPrivateMessageTarget = targetStr;
		} else { 
			String temp(str(STR_UNKNOWN_USER));
			temp += targetStr;
			
			if (isPing == false) {
				temp += str(STR_MESSAGE);
				temp += restOfString;
				temp += str(STR_NOT_SENT);
			}
			
			LogMessage(LOG_ERROR_MESSAGE, temp(), NULL, NULL, false, optEchoTo);
		}
	} else
		LogMessage(LOG_ERROR_MESSAGE, str(STR_NO_TARGET_USER_SPECIFIED_IN_MSG), NULL, NULL, false, optEchoTo);
}


void 
ShareWindow ::
LogPattern(const char * preamble, const String & pattern, ChatWindow * optEchoTo)
{
	String iStr(preamble);
	if (pattern.Length() > 0) iStr += pattern;
	else
	{
	 iStr += "(";
	 iStr += str(STR_DISABLED);
	 iStr += ")";
	}
	LogMessage(LOG_INFORMATION_MESSAGE, iStr(), NULL, NULL, false, optEchoTo);
}

void 
ShareWindow ::
LogRateLimit(const char * preamble, uint32 limit, ChatWindow * optEchoTo)
{
	String iStr(preamble);
	if (limit > 0)
	{
	 char buf[64]; sprintf(buf, " %lu ", limit);
	 iStr += buf;
	 iStr += str(STR_TOKEN_BYTES_PER_SECOND);
	}
	else 
	{
	 iStr += " (";
	 iStr += str(STR_NO_LIMIT);
	 iStr += ")";
	}
	LogMessage(LOG_INFORMATION_MESSAGE, iStr(), NULL, NULL, false, optEchoTo);
}

String 
ShareWindow::
GetQualifiedSharedFileName(const String & name) const
{
	if (_netClient->GetLocalSessionID()[0])
	{
	 entry_ref er = _netClient->FindSharedFile(name());
	 if (BEntry(&er).Exists())
	 {
		String ret(name);
		ret += "@";
		ret += _netClient->GetLocalSessionID();
		return ret;
	 }
	}
	return name;
}

status_t
ShareWindow::
ExpandAlias(const String & text, String & retStr) const
{
	return _aliases.Get(text, retStr);
}

void
ShareWindow ::
SendChatText(const String & t, ChatWindow * optEchoTo)
{
	const String * text = &t;	// point to the string we will use; in the common case it's the passed-in one.
	String altText;

	String lowerText = text->ToLowerCase();
	if (lowerText.StartsWith("/action ")) 
	{
	 altText = text->Substring(8).Prepend("/me ");
	 text = &altText;	// oops, we'll use the alternate string instead
	 lowerText = text->ToLowerCase();
	}

		if (lowerText.StartsWith("/msg ")) SendOutMessageOrPing(*text, optEchoTo, false);
	else if (lowerText.StartsWith("/ping ")) SendOutMessageOrPing(*text, optEchoTo, true);
	else if (lowerText.StartsWith("/priv"))
	{
	 BMessage msg(SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW);
	 if (text->Length() > 6) msg.AddString("users", &(text->Cstr())[6]);
	 PostMessage(&msg);
	}
	else if (lowerText.StartsWith("/fontsize"))
	{
	 SetFontSize(lowerText);
	}
	else if (lowerText.StartsWith("/font"))
	{
	 SetFont(lowerText, true);
	}
	// Zaranthos - uptime code from YNOP.	Coding help from YNOP and yuktar
	else if (lowerText.StartsWith("/uptime"))
	{
	 system_info info;
	 get_system_info(&info);
	//microseconds snce January 1st, 1970
	 bigtime_t micro = system_time();
	 bigtime_t milli = micro/1000;
	 bigtime_t sec = milli/1000;
	 bigtime_t min = sec/60;
	 bigtime_t hours = min/60;
	 bigtime_t days = hours/24;
	char str[100]; //the time string
	for (int i=0; i<100; i++) str[i]=0; //clear the string
	if (days) sprintf(str,"%Ld day%s,",days,days!=1?"s":""); //add days if >0
	if (hours%24) sprintf(str,"%s %Ld hour%s,",str,hours%24,(hours%24)!=1?"s":""); //add hours if >0
	if (min%60) sprintf(str,"%s %Ld minute%s,",str, min%60, (min%60)!=1?"s":""); //add minutes if >0
	sprintf(str,"%s %Ld second%s",str,sec%60,(sec%60)!=1?"s":""); //add seconds
 //	printf("%s\n",str);fflush(stdout); //send to stdout for debugging
	String newText="BeOS System Uptime: ";
	int i=0; while(str[i]!='\0') newText+=str[i++];
	lowerText= newText.ToLowerCase();
//	const char * txt = newText,Cstr()+(((lowerText.StartsWith("/me ")==false)&&(lowerText[0]=='/'))?1:0);
	 const char * txt = newText.Cstr()+0; // This doesn't do anything
	 _netClient->SendChatMessage("*", txt);	// if started with double slash, remove escape
	 LogMessage(LOG_LOCAL_USER_CHAT_MESSAGE, txt, NULL, NULL, false, optEchoTo);
	// end Zaranthos except for uptime entry at the end of the help section.
	}
	else if (lowerText.StartsWith("/nick "))
	{
	 _userNameEntry->SetText(text->Cstr()+6);
	 PostMessage(SHAREWINDOW_COMMAND_USER_CHANGED_NAME);
	}
	else if (lowerText.StartsWith("/screenshot"))
	{
	 DoScreenShot(text->Substring(11).Trim(), optEchoTo);
	}
	else if (lowerText.StartsWith("/status "))
	{
	 _userStatusEntry->SetText(text->Cstr()+8);
	 PostMessage(SHAREWINDOW_COMMAND_USER_CHANGED_STATUS);
	}
	else if (lowerText.Equals("/clear"))
	{
	 PostMessage(SHAREWINDOW_COMMAND_CLEAR_CHAT_LOG);
	}
	else if (lowerText.StartsWith("/quit"))
	{
	 PostMessage(B_QUIT_REQUESTED);
	}
	else if ((lowerText.StartsWith("/start"))||(lowerText.StartsWith("/query")))
	{
	 if (text->Length() > 7) _fileNameQueryEntry->SetText(text->Cstr()+7);
	 BMessage setMsg(SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY); // in case we have a query going
	 setMsg.AddBool("activate", true);						 // in case we don't
	 PostMessage(&setMsg); 
	}
	else if (lowerText.StartsWith("/stop"))
	{
	 PostMessage(SHAREWINDOW_COMMAND_DISABLE_QUERY);
	}
	else if (lowerText.StartsWith("/disconnect"))
	{
	 PostMessage(SHAREWINDOW_COMMAND_DISCONNECT_FROM_SERVER);
	}
	else if (lowerText.StartsWith("/color"))
	{
	 SetCustomColorsEnabled(!GetCustomColorsEnabled());
	 UpdateColors();
	}
	else if (lowerText.StartsWith("/connect"))
	{
	 if (text->Length() > 9) _serverEntry->SetText(text->Cstr()+9);
	 PostMessage(SHAREWINDOW_COMMAND_RECONNECT_TO_SERVER);
	}
	else if (lowerText.StartsWith("/ignore"))
	{
	 if (text->Length() > 8) 
	 {
		_ignorePattern = text->Substring(8).Trim();
		String s(str(STR_IGNORE_PATTERN_SET_TO));
		s += _ignorePattern;
		LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
	 }
	 else 
	 {
		_ignorePattern = "";
		LogMessage(LOG_INFORMATION_MESSAGE, str(STR_IGNORE_PATTERN_REMOVED), NULL, NULL, false, optEchoTo);
	 }
	}
	else if (lowerText.StartsWith("/watch"))
	{
	 if (text->Length() > 7) 
	 {
		_watchPattern = text->Substring(7).Trim();
		String s(str(STR_WATCH_PATTERN_SET_TO));
		s += _watchPattern;
		LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
	 }
	 else 
	 {
		_watchPattern = "";
		LogMessage(LOG_INFORMATION_MESSAGE, str(STR_WATCH_PATTERN_REMOVED), NULL, NULL, false, optEchoTo);
	 }
	}
	else if (lowerText.StartsWith("/autopriv"))
	{
	 if (text->Length() > 10) 
	 {
		_autoPrivPattern = text->Substring(10).Trim();
		String s(str(STR_AUTOPRIV_PATTERN_SET_TO));
		s += _autoPrivPattern;
		LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
	 }
	 else 
	 {
		_autoPrivPattern = "";
		LogMessage(LOG_INFORMATION_MESSAGE, str(STR_AUTOPRIV_PATTERN_REMOVED), NULL, NULL, false, optEchoTo);
	 }
	}
	else if (lowerText.StartsWith("/awaymsg"))
	{
	 _oneTimeAwayStatus = "";
	 if (text->Length() > 9) _awayStatus = text->Substring(9).Trim();
	 String s(str(STR_AWAY_MESSAGE_SET_TO));
	 s += _awayStatus;
	 LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
	}
	else if ((lowerText.Equals("/away"))||(lowerText.StartsWith("/away "))) 
	{
	 if (text->Length() > 6) _oneTimeAwayStatus = text->Substring(6).Trim();
	 MakeAway();
	}
	else if (lowerText.StartsWith("/shell "))
	{
	 String command = text->Substring(7).Trim();

	 String s(str(STR_EXECUTING_SHELL_COMMAND));
	 s += " [";
	 s += command;
	 s += "]";
	 LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);

	 command += " &";	// let's not lock up the GUI waiting for it...
	 system(command());
	}
	else if (lowerText.StartsWith("/onidle"))
	{
	 _onIdleString = lowerText.Substring(7).Trim();

	 String s(str(STR_IDLE_COMMAND_SET_TO)); 
	 s += " [";
	 s += _onIdleString();
	 s += "]";
	 LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
	}
	else if (lowerText.StartsWith("/onlogin "))
	{
	 String ol = lowerText.Substring(9).Trim();
	 _onLoginStrings.AddTail(ol);

	 String report(str(STR_ADDED_STARTUP_COMMAND)); 
	 report += ol;
	 LogMessage(LOG_INFORMATION_MESSAGE, report(), NULL, NULL, false, optEchoTo);
	}
	else if (lowerText.Equals("/clearonlogin"))
	{
	 _onLoginStrings.Clear();
	 LogMessage(LOG_INFORMATION_MESSAGE, str(STR_ONLOGIN_COMMANDS_CLEARED), NULL, NULL, false, optEchoTo);
	}
	else if (lowerText.Equals("/serverinfo"))
	{ 
	 _showServerStatus = true;
	 _netClient->SendGetParamsMessage();	// request server status.
	 LogMessage(LOG_INFORMATION_MESSAGE, str(STR_SERVER_STATUS_REQUESTED), NULL, NULL, false, optEchoTo);
	}
	else if (lowerText.Equals("/unban"))
	{
	 char temp[256];
	 sprintf(temp, str(STR_REMOVING_PLU_UPLOAD_BANS), _bans.GetNumItems());
	 _bans.Clear();
	 LogMessage(LOG_INFORMATION_MESSAGE, temp, NULL, NULL, false, optEchoTo);
	}
	else if (lowerText.StartsWith("/unalias "))
	{
	 String which = lowerText.Substring(9).Trim();
	 if (_aliases.Remove(which) == B_NO_ERROR)
	 {
		String s(str(STR_REMOVED_ALIAS));
		s += " ";
		s += which;
		LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
	 }
	}
	else if (lowerText.StartsWith("/alias"))
	{
	 String arg = text->Substring(6).Trim();
	 if (arg.Length() > 0)
	 {
		StringTokenizer tok(arg());
		const char * key = tok.GetNextToken();
		const char * value = tok.GetRemainderOfString();
		if (key)
		{
			if (value)
			{
			 String v(value);
			 v = v.Trim();
			 _aliases.Put(key, v);
			 String s(str(STR_SET_ALIAS));
			 s += ' ';
			 s += key;
			 s += " = ";
			 s += v;
			 LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
			}
			else
			{
			 String * value = _aliases.Get(key);
			 if (value)
			 {
				String s("	");
				s += key;
				s += " = ";
				s += *value;
				LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
			 }
			}
		}
	 }
	 else
	 {
		String nextKey, nextValue;
	for (HashtableIterator<String, String> iter(_aliases.GetIterator()); iter.HasData(); iter++) {
			nextKey = iter.GetKey();
			nextValue = iter.GetValue();
			String s("	");
			s += nextKey;
			s += " = ";
			s += nextValue;
			LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
		}
	 }
	}
	else if (lowerText.StartsWith("/title"))
	{
	 StringTokenizer tok(text->Cstr());
	 (void) tok.GetNextToken();
	 const char * arg = tok.GetRemainderOfString();

	 BMessage updateTitle(CHATWINDOW_COMMAND_SET_CUSTOM_TITLE);
	 if (arg) updateTitle.AddString("title", arg);
	 (optEchoTo ? optEchoTo : this)->PostMessage(&updateTitle);

	 String s(str(STR_CUSTOM_WINDOW_TITLE_IS_NOW));
	 s += ": ";
	 s += arg ? arg : str(STR_DISABLED); 
	 LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
	}
	else if (lowerText.StartsWith("/setulrate")) SetBandwidthLimit(true, lowerText, optEchoTo);
	else if (lowerText.StartsWith("/setdlrate")) SetBandwidthLimit(false, lowerText, optEchoTo);
	else if (lowerText.Equals("/help"))
	{
	 LogMessage(LOG_INFORMATION_MESSAGE, str(STR_AVAILABLE_IRC_STYLE_COMMANDS), NULL, NULL, false, optEchoTo);
	 LogHelp("action",	 STR_TOKEN_ACTION,				STR_DO_SOMETHING,				optEchoTo);
	 LogHelp("alias",	 STR_TOKEN_NAME_AND_VALUE,		 STR_CREATE_AN_ALIAS,			 optEchoTo);
	 LogHelp("autopriv",	STR_TOKEN_NAMES_OR_SESSION_IDS,	STR_SPECIFY_AUTOPRIV_USERS,		optEchoTo);
	 LogHelp("away",		STR_TOKEN_MESSAGE_STRING,		 STR_FORCE_AWAY_STATE,			optEchoTo);
	 LogHelp("awaymsg",	STR_TOKEN_MESSAGE_STRING,		 STR_CHANGE_THE_AUTO_AWAY_MESSAGE, optEchoTo);
	 LogHelp("clear",	 -1,							 STR_CLEAR_THE_CHAT_LOG,			optEchoTo);
	 LogHelp("clearonlogin",	-1,							STR_CLEAR_STARTUP_COMMANDS,		optEchoTo);
	 LogHelp("color",	 -1,							 STR_TOGGLE_CUSTOM_COLORS,		optEchoTo);
	 LogHelp("connect",	STR_TOKEN_SERVER_NAME,			STR_CONNECT_TO_A_SERVER,		 optEchoTo);
	 LogHelp("disconnect", -1,							 STR_DISCONNECT_FROM_THE_SERVER,	optEchoTo);
	 LogHelp("font",		STR_TOKEN_FONT,					STR_SET_FONT,					optEchoTo);
	 LogHelp("fontsize",	STR_TOKEN_FONT_SIZE,			 STR_SET_FONT_SIZE,				optEchoTo);
	 LogHelp("help",		-1,							 STR_SHOW_THIS_HELP_TEXT,		 optEchoTo);
	 LogHelp("ignore",	 STR_TOKEN_NAMES_OR_SESSION_IDS,	STR_SPECIFY_USERS_TO_IGNORE,	 optEchoTo);
	 LogHelp("info",		-1,							 STR_SHOW_MISCELLANEOUS_INFO,	 optEchoTo);
	 LogHelp("me",		STR_TOKEN_ACTION,				STR_SYNONYM_FOR_ACTION,			optEchoTo);
	 LogHelp("msg",		STR_TOKEN_NAME_OR_SESSION_ID_TEXT, STR_SEND_A_PRIVATE_MESSAGE,		optEchoTo);
	 LogHelp("nick",		STR_TOKEN_NAME,					STR_CHANGE_YOUR_USER_NAME,		optEchoTo);
	 LogHelp("onidle",	 STR_TOKEN_COMMAND,				STR_SET_COMMAND_FOR_WHEN_TRANSFERS_CEASE, optEchoTo);
	 LogHelp("onlogin",	STR_TOKEN_COMMAND,				STR_ADD_STARTUP_COMMAND,		 optEchoTo);
	 LogHelp("priv",		STR_TOKEN_NAMES_OR_SESSION_IDS,	STR_OPEN_PRIVATE_CHAT_WINDOW,	 optEchoTo);
	 LogHelp("ping",		STR_TOKEN_NAMES_OR_SESSION_IDS,	STR_PING_OTHER_CLIENTS,			optEchoTo);
	 LogHelp("quit",		-1,							 STR_QUIT_BESHARE,				optEchoTo);	
	 LogHelp("screenshot", -1,							 STR_SHARE_SCREENSHOT,			optEchoTo);
	 LogHelp("serverinfo", -1,							 STR_REQUEST_SERVER_STATUS,		optEchoTo);
	 LogHelp("setdlrate",	STR_TOKEN_BYTES_PER_SECOND,		STR_SET_MAX_DOWNLOAD_RATE,		optEchoTo);
	 LogHelp("setulrate",	STR_TOKEN_BYTES_PER_SECOND,		STR_SET_MAX_UPLOAD_RATE,		 optEchoTo);
	 LogHelp("shell",	 STR_TOKEN_SHELL_COMMAND,			STR_EXECUTE_SHELL_COMMAND,		optEchoTo);
	 LogHelp("start",	 STR_TOKEN_QUERY_STRING,			STR_START_A_NEW_QUERY,			optEchoTo);
	 LogHelp("status",	 STR_STATUS,					 STR_SET_USER_STATUS_STRING,		optEchoTo);
	 LogHelp("stop",		-1,							 STR_STOP_THE_CURRENT_QUERY,		optEchoTo);
	 LogHelp("title",	 STR_TOKEN_NAME,					STR_SET_CUSTOM_WINDOW_TITLE,	 optEchoTo);
	 LogHelp("unalias",	STR_TOKEN_NAME,					STR_REMOVE_AN_ALIAS,			 optEchoTo);
	 LogHelp("unban",	 -1,							 STR_REMOVE_ALL_UPLOAD_BANS,		optEchoTo);
	 LogHelp("watch",	 STR_TOKEN_NAMES_OR_SESSION_IDS,	STR_SPECIFY_USERS_TO_WATCH,		optEchoTo);
	 LogHelp("uptime",	 -1,							 STR_UPTIME,					 optEchoTo);
	}
	else if (lowerText.Equals("/info"))
	{
	 LogPattern(str(STR_CURRENT_IGNORE_PATTERN_IS),	_ignorePattern,	optEchoTo);
	 LogPattern(str(STR_CURRENT_WATCH_PATTERN_IS),	_watchPattern,	optEchoTo);
	 LogPattern(str(STR_CURRENT_AUTOPRIV_PATTERN_IS), _autoPrivPattern, optEchoTo);
	 LogRateLimit(str(STR_MAX_DOWNLOAD_RATE_IS),	 _maxDownloadRate, optEchoTo);
	 LogRateLimit(str(STR_MAX_UPLOAD_RATE_IS),		_maxUploadRate,	optEchoTo);

	 char tbuf1[32]; GetByteSizeString(_totalBytesDownloaded, tbuf1);
	 char tbuf2[32]; GetByteSizeString(_totalBytesUploaded,	tbuf2);
	 char buf[128]; sprintf(buf, str(STR_TRANSFER_REPORT), tbuf1, tbuf2);
	 LogMessage(LOG_INFORMATION_MESSAGE, buf, NULL, NULL, false, optEchoTo);
	}
	else if ((lowerText.StartsWith("/me") == false)&&(lowerText.StartsWith("/")&&(!lowerText.StartsWith("//"))))	// double slash means escape the starting slash
	{
	 String err(str(STR_ERROR_UNKNOWN_COMMAND));
	 err += " \"";
	 StringTokenizer tok(text->Cstr());
	 err += tok.GetNextToken();
	 err += "\".	";
	 err += str(STR_TYPE_HELP_FOR_LIST_OF_AVAILABLE_COMMANDS);
	 LogMessage(LOG_ERROR_MESSAGE, err(), NULL, NULL, false, optEchoTo);
	}
	else if (lowerText.Length() > 0)
	{
	 const char * txt = text->Cstr()+(((lowerText.StartsWith("/me")==false)&&(lowerText[0]=='/'))?1:0);
	 _netClient->SendChatMessage("*", txt);	// if started with double slash, remove escape
	 LogMessage(LOG_LOCAL_USER_CHAT_MESSAGE, txt, NULL, NULL, false, optEchoTo);
	}
}

void ShareWindow::SetBandwidthLimit(bool upload, const String & lowerText, ChatWindow * optEchoTo)
{
	StringTokenizer tok(lowerText());
	(void) tok();	// throw away the keyword

	const char * arg = tok();
	uint32 limit = arg ? atoi(arg) : 0;
	if (upload)
	{
	 _maxUploadRate = limit;	
	 LogRateLimit(str(STR_MAX_UPLOAD_RATE_IS), _maxUploadRate, optEchoTo);
	}
	else
	{
	 _maxDownloadRate = limit;	
	 LogRateLimit(str(STR_MAX_DOWNLOAD_RATE_IS), _maxDownloadRate, optEchoTo);
	}
}

// Returns true iff (user) matches the user filter string(filter)
// Not used in SendChatText() for security/privacy reasons (we need an _ordered_ grep there!)
bool
ShareWindow::MatchesUserFilter(const RemoteUserItem * user, const char * filter) const
{
	StringTokenizer idTok(filter, ","); // identifiers may be separated by commas (but not spaces, as those may be parts of the users' names!)
	const char * n;
	while((n = idTok.GetNextToken()) != NULL)
	{
	 String next(n);
	 next = next.Trim();

	 // Is this item our user's session ID?
	 if (strcmp(user->GetSessionID(), next()) == 0) return true;
	 else
	 {
		// Does this item (interpreted as a regex) match our user's name?
		MakeRegexCaseInsensitive(next);
		StringMatcher sm(next());
		String userName = String(SubstituteLabelledURLs(user->GetDisplayHandle())).Trim();
		if ((userName.Length() > 0)&&(sm.Match(userName()))) return true;
	 }
	}
	return false;
}

const char *
ShareWindow ::
GetUserNameBySessionID(const char * sessionID) const
{
	RemoteUserItem * user;
	return (_users.Get(sessionID, user) == B_NO_ERROR) ? user->GetVerbatimHandle() : NULL;
}		

void ShareWindow::GetUserNameForSession(const char * sessionID, String & retUserName) const
{
	const char * ret = GetUserNameBySessionID(sessionID);
	retUserName = ret ? ret : str(STR_UNKNOWN);
}

void ShareWindow::GetLocalUserName(String & retLocalUserName) const
{
	String ret(_netClient->GetLocalUserName());
	retLocalUserName = ret;
}

void ShareWindow::GetLocalSessionID(String & retLocalSessionID) const
{
	String ret(_netClient->GetLocalSessionID());
	retLocalSessionID = ret;
}


void
ShareWindow::UpdatePrivateWindowUserList(PrivateChatWindow * w, const char * target)
{
	TRACE_BESHAREWINDOW(("ShareWindow::UpdatePrivateWindowUserList begin\n"));
	// Resend the user list to the window, so it can update its user list
	w->PostMessage(PrivateChatWindow::PRIVATE_WINDOW_REMOVE_USER);	// clear old users
	RemoteUserItem * user;

	for (HashtableIterator<const char *, RemoteUserItem *> iter(_users.GetIterator()); iter.HasData(); iter++) {
		user = iter.GetValue();
		if (MatchesUserFilter(user, target)) {
			BMessage msg(PrivateChatWindow::PRIVATE_WINDOW_ADD_USER);
			msg.AddString("id", user->GetSessionID());
			msg.AddString("name", user->GetDisplayHandle());
			w->PostMessage(&msg);
		}
	}
	TRACE_BESHAREWINDOW(("ShareWindow::UpdatePrivateWindowUserList end\n"));
}

void
ShareWindow ::
SetQueryInProgress(bool qp)
{
	if (qp != (_queryInProgressRunner != NULL))
	{
	 if (qp)
	 {
		BMessenger toMe(this);
		_queryInProgressRunner = new BMessageRunner(toMe, new BMessage(SHAREWINDOW_COMMAND_QUERY_IN_PROGRESS_ANIM), 100000LL); //	10fps
	 }
	 else 
	 {
		delete _queryInProgressRunner;
		_queryInProgressRunner = NULL;
		DrawQueryInProgress(false);
	 }
	}
}

void
ShareWindow ::
SortResults()
{
	_resultsView->SortItems();
}

void
ShareWindow ::
DrawQueryInProgress(bool inProgress)
{
	BView * clv = _resultsView->GetColumnLabelView();

	BRect radarBounds(clv->Bounds());
	radarBounds.right = 19.0f;
	radarBounds.InsetBy(2,2);

	BPoint center((radarBounds.left+radarBounds.right)/2.0f, ((radarBounds.top+radarBounds.bottom)/2.0f)-1.0f);
	BPoint radius(center.x-radarBounds.left, center.y-radarBounds.top);

	// draw dee leetle radar screen, doop de doop de doo
	if (inProgress)
	{
	 if (_lastInProgress != inProgress)
	 {
		clv->SetHighColor(0,0,0);
		clv->FillEllipse(center, radius.x, radius.y, B_SOLID_HIGH);
		clv->SetHighColor(255,255,255);
		clv->StrokeEllipse(center, radius.x, radius.y, B_SOLID_HIGH);
	 }
	 rgb_color color = {0, 0, 0, 255};	// BeBackgroundGrey;
	 const float diff = 30.0f;
	 const float total = 180.0f;
	 radius.x -= 1.0f;	// don't overdraw the outline!
	 radius.y -= 1.0f;
	 for (float a=_radarSweep; a>_radarSweep-total; a-=diff)
	 {
		clv->SetHighColor(color);
		clv->FillArc(center, radius.x, radius.y, a, diff);
		color.green = (uint8)(((float)color.green*0.7f)+(255.0f*0.3f));
	 }
	 _radarSweep -= diff/2.0f;
	}
	else 
	{
	 clv->FillEllipse(center, radius.x, radius.y, B_SOLID_LOW);
	 _radarSweep = 0.0f;
	}

	_lastInProgress = inProgress;
}


void 
ShareWindow::UpdatePagingButtons()
{
	uint32 numPages = _resultsPages.GetNumItems();
	_prevPageButton->SetEnabled((numPages > 1)&&(_currentPage > 0));
	_nextPageButton->SetEnabled((numPages > 1)&&(_currentPage < numPages-1));
}

void
ShareWindow::DispatchMessage(BMessage * msg, BHandler * handler)
{
	switch(msg->what)
	{
	 case B_MOUSE_DOWN:
	 {
			 if ((handler == _resultsView)&&(_resultsView->IsFocus() == false)) _resultsView->MakeFocus();
		else if ((handler == _usersView)&&(_usersView->IsFocus() == false)) _usersView->MakeFocus();
	 }
	 break;

	 case B_KEY_DOWN:
	 {
		int8 c;
		int32 modifiers;
		if ((msg->FindInt32("modifiers", &modifiers) == B_NO_ERROR)&&
			(msg->FindInt8("byte", &c)			 == B_NO_ERROR))
		{
			switch(c)
			{
			 case B_ENTER: 
				if ((_isConnected)&&(handler == _fileNameQueryEntry->TextView())) PostMessage(SHAREWINDOW_COMMAND_ENABLE_QUERY); 
			 break;
				
			 case B_UP_ARROW: case B_DOWN_ARROW:
				if (modifiers & B_COMMAND_KEY) 
				{
					_transferList->MoveSelectedItems((c == B_UP_ARROW) ? -1 : 1);
					msg = NULL;
				}
			 break;
			}
		}
	 }
	 break;
	}
	if (msg) ChatWindow::DispatchMessage(msg, handler);
}

void
ShareWindow::UserChatted()
{
	// Watch for selected UI events to see when the user is back
	_lastInteractionAt = system_time();
	_autoReconnectAttemptCount = 0;	// if user is here, don't make him wait for a reconnect
	if (_idle)
	{
	 _idle = false;
	 PostMessage(SHAREWINDOW_COMMAND_UNIDLE);
	}
}


void
ShareWindow::LogStat(int statName, const char * statValue)
{
	String temp(str(statName));
	temp += ":	";
	temp += statValue;
	LogMessage(LOG_INFORMATION_MESSAGE, temp());
}
	

String
ShareWindow::MakeTimeElapsedString(int64 t) const
{
	int64 seconds = t / 1000000;
	int64 minutes = seconds / 60;	seconds = seconds % 60;
	int64 hours	= minutes / 60;	minutes = minutes % 60;
	int64 days	= hours	/ 24;	hours	= hours	% 24;
	int64 weeks	= days	/	7;	days	= days	% 7;

	char temp[256];
	String s;

	if (weeks > 0)
	{
	 sprintf(temp, "%Li %s, ", weeks, str(STR_WEEKS));
	 s += temp;
	}

	if ((weeks > 0)||(days > 0))
	{
	 sprintf(temp, "%Li %s, ", days, str(STR_DAYS));
	 s += temp;
	}

	sprintf(temp, "%Li:%02Li:%02Li", hours, minutes, seconds);
	s += temp;

	return s;
}

void
ShareWindow::ServerParametersReceived(const Message & params)
{
	if (_showServerStatus)
	{
	 _showServerStatus = false;
	 LogMessage(LOG_INFORMATION_MESSAGE, str(STR_SERVER_STATUS));

	 const char * serverVersion;
	 if (params.FindString(PR_NAME_SERVER_VERSION, &serverVersion) == B_NO_ERROR) LogStat(STR_SERVER_VERSION, serverVersion);

	 int64 serverUptime;
	 if (params.FindInt64(PR_NAME_SERVER_UPTIME, &serverUptime) == B_NO_ERROR) LogStat(STR_SERVER_UPTIME, MakeTimeElapsedString(serverUptime)());

	 const char * sessionRoot;
	 if (params.FindString(PR_NAME_SESSION_ROOT, &sessionRoot) == B_NO_ERROR) LogStat(STR_LOCAL_SESSION_ROOT, sessionRoot);

	 int64 memAvailable, memUsed;
	 if ((params.FindInt64(PR_NAME_SERVER_MEM_AVAILABLE, &memAvailable) == B_NO_ERROR)&&
		 (params.FindInt64(PR_NAME_SERVER_MEM_USED,	 &memUsed)	 == B_NO_ERROR))
	 {
		const float oneMeg = 1024.0f * 1024.0f;
		float memAvailableMB = ((float)memAvailable)/oneMeg;
		float memUsedMB	 = ((float)memUsed)	 /oneMeg;
		char temp[256];
		sprintf(temp, str(STR_MEMORY_USED_AVAILABLE), memUsedMB, memAvailableMB);
		LogStat(STR_SERVER_MEMORY_USAGE, temp);
	 }
	}
	UpdateTitleBar();
}

bool ShareWindow::AreMessagesEqual(const BMessage & m1, const BMessage & m2) const
{
	if (m1.what != m2.what) return false;
	if (m1.CountNames(B_ANY_TYPE) != m2.CountNames(B_ANY_TYPE)) return false;
	if (IsFieldSuperset(m1, m2) == false) return false;
	if (IsFieldSuperset(m2, m1) == false) return false;
	return true;
}


bool
ShareWindow::IsFieldSuperset(const BMessage & m1, const BMessage & m2) const
{
	char * name;
	type_code type1;
	int32 count1;
	for (int32 i = 0; (m1.GetInfo(B_ANY_TYPE, i, &name, &type1, &count1) == B_NO_ERROR); i++) {
		type_code type2;
		int32 count2;
		
		if ((m2.GetInfo(name, &type2, &count2) != B_NO_ERROR)
			|| (type2 != type1)||(count2 != count1))
			return false;

		for (int32 j = 0; j < count1; j++) {
			if (type1 == B_MESSAGE_TYPE) {
				BMessage s1, s2;
				if ((m1.FindMessage(name, j, &s1) != B_NO_ERROR)
					|| (m2.FindMessage(name, j, &s2) != B_NO_ERROR)
					|| (AreMessagesEqual(s1, s2) == false))
					return false;
			} else {
				const void * data1;
				const void * data2;
				ssize_t size1;
				ssize_t size2;
				if ((m1.FindData(name, type1, j, &data1, &size1) != B_NO_ERROR)
					|| (m2.FindData(name, type1, j, &data2, &size2) != B_NO_ERROR)
					|| (size1 != size2)
					|| (memcmp(data1, data2, size1) != 0))
					return false;
			}

		}
	}
	return true;
}				


void
ShareWindow::BeginAutoReconnect()
{
	if (_autoReconnectAttemptCount++ > 0)
	{
	 // for subsequent tries, we wait a while longer each time
	 ResetAutoReconnectState(false);	// make sure no runner is currently going
	 uint32 reconnectDelayMinutes = _autoReconnectAttemptCount-1;
	 char buf[128];
	 sprintf(buf, str(STR_WILL_ATTEMPT_AUTO_RECONNECT_IN_PLU_MINUTES), reconnectDelayMinutes);
	 LogMessage(LOG_INFORMATION_MESSAGE, buf);
	 _autoReconnectRunner = new BMessageRunner(BMessenger(this), new BMessage(SHAREWINDOW_COMMAND_AUTO_RECONNECT), reconnectDelayMinutes*60*1000000LL);
	 UpdateConnectStatus(false);	// so that the disconnect button will become enabled
	}
	else DoAutoReconnect();
}

void 
ShareWindow::DoAutoReconnect()
{
	ResetAutoReconnectState(false);	// once the connection is started, the runner is unnecessary
	LogMessage(LOG_INFORMATION_MESSAGE, str(STR_ATTEMPTING_AUTO_RECONNECT));
	ReconnectToServer();	// reconnect immediately
}


void
ShareWindow::ReconnectToServer()
{
	TRACE_BESHAREWINDOW(("ShareWindow::ReconnectToServer begin\n"));
	const char * server = _serverEntry->Text();

	if (server) {
		TRACE_BESHAREWINDOW(("ShareWindow::ReconnectToServer has server = %s\n", server));
		_connectedTo = server;	// save this for later, when we're connected
		StringTokenizer tok(server, " :");
		const char * host = tok.GetNextToken();

		if (host) {
			TRACE_BESHAREWINDOW(("ShareWindow::ReconnectToServer has host = %s\n", host));
			const char * portStr = tok.GetNextToken();
			int port = portStr ? atoi(portStr) : 0;
			
			if (port <= 0)
				port = 2960;

			_netClient->ConnectToServer(host, (uint16) (port ? port : 2960));
		}
	}

	UpdateConnectStatus(true);
	TRACE_BESHAREWINDOW(("ShareWindow::ReconnectToServer end\n"));
}


void 
ShareWindow::ResetAutoReconnectState(bool resetCountToo)
{
	TRACE_BESHAREWINDOW(("ShareWindow::ResetAutoReconnectState start\n"));
	
	delete _autoReconnectRunner;
	_autoReconnectRunner = NULL;
	
	if (resetCountToo)
		_autoReconnectAttemptCount = 0;
	TRACE_BESHAREWINDOW(("ShareWindow::ResetAutoReconnectState end\n"));
}

void 
ShareWindow::
PauseAllUploads()
{
	for (int32 i=_transferList->CountItems()-1; i>=0; i--)
	{
	 ShareFileTransfer * xfr = (ShareFileTransfer *) _transferList->ItemAt(i);
	 if (xfr->IsUploadSession())
	 {
		if (xfr->IsWaitingOnLocal() == false) xfr->RequeueTransfer();
		xfr->SetBeginTransferEnabled(false);
	 }
	}
	_transferList->Invalidate();
	DequeueTransferSessions();
}

void 
ShareWindow::
ResumeAllUploads()
{
	uint32 num = _transferList->CountItems();
	for (uint32 i=0; i<num; i++)
	{
	 ShareFileTransfer * xfr = (ShareFileTransfer *) _transferList->ItemAt(i);
	 if ((xfr->IsUploadSession())&&(xfr->IsWaitingOnLocal()))
	 {
		if (xfr->GetBeginTransferEnabled()) xfr->BeginTransfer();
									else xfr->SetBeginTransferEnabled(true);
	 }
	}
	_transferList->Invalidate();
	DequeueTransferSessions();
}

void 
ShareWindow::SetLocalUserName(const char * name)
{	 
	_userNameEntry->SetText(name);
		
	// See if the new name is in our user name list;	if not, add it to the beginning
	UpdateLRUMenu(_userNameMenu, name, SHAREWINDOW_COMMAND_USER_SELECTED_USER_NAME, "username", 20, true);

	_netClient->SetLocalUserName(name);
	String s(str(STR_YOUR_NAME_HAS_BEEN_CHANGED_TO));
	s += _netClient->GetLocalUserName();
	LogMessage(LOG_USER_EVENT_MESSAGE, s());
	_resultsView->MakeFocus();	// so that when the user presses a key, it drops to the _textEntry
}

void 
ShareWindow::SetLocalUserStatus(const char * status)
{
	_userStatusEntry->SetText(status);

	// See if the new status is in our user status list;	if not, add it to the beginning
	UpdateLRUMenu(_userStatusMenu, status, SHAREWINDOW_COMMAND_USER_SELECTED_USER_STATUS, "userstatus", 20, true);

	_netClient->SetLocalUserStatus(status);
	String s(str(STR_YOUR_STATUS_HAS_BEEN_CHANGED_TO));
	s += _netClient->GetLocalUserStatus();
	LogMessage(LOG_USER_EVENT_MESSAGE, s());
	_resultsView->MakeFocus();	// so that when the user presses a key, it drops to the _textEntry
}

void ShareWindow::SetServer(const char * server)
{
	bool reconnect = ((_isConnected == false)||(strcasecmp(server, _serverEntry->Text())));
	_serverEntry->SetText(server);
	if (reconnect) PostMessage(SHAREWINDOW_COMMAND_RECONNECT_TO_SERVER);
}

void ShareWindow::SetQuery(const char * query)
{
	_fileNameQueryEntry->SetText(query);
	SetQueryEnabled(false);	// force query resend
	if (strlen(query) > 0) SetQueryEnabled(true);
}

void ShareWindow::SendMessageToServer(const MessageRef & msg)
{
	_netClient->SendMessageToSessions(msg, true);
}

BBitmap * ShareWindow::GetDoubleBufferBitmap(uint32 width, uint32 height)
{
	// First make sure our background bitmap is large enough for this request...
	if ((_doubleBufferBitmap == NULL)||(_doubleBufferBitmap->Bounds().Width() < width)||(_doubleBufferBitmap->Bounds().Height() < height)||(_doubleBufferBitmap->ColorSpace() != BScreen(this).ColorSpace()))
	{
	 width	*= 2;	// leave room to grow too
	 height *= 2;

	 if (_doubleBufferBitmap)
	 {
		_doubleBufferBitmap->RemoveChild(_doubleBufferView);
		delete _doubleBufferBitmap;
	 }
	 _doubleBufferBitmap = new BBitmap(BRect(0,0,width,height), BScreen(this).ColorSpace(), true);
	 _doubleBufferView->ResizeTo(width, height);
	 _doubleBufferBitmap->AddChild(_doubleBufferView);
	}
	return _doubleBufferBitmap;
}

status_t ShareWindow::ShareScreenshot(const String & fileName)
{
	status_t ret = B_ERROR;
	BTranslatorRoster * roster = BTranslatorRoster::Default();
	if (roster)
	{
	 BBitmap * screenshot = NULL;
	 if (BScreen().GetBitmap(&screenshot) == B_NO_ERROR)
	 {
		if (screenshot->LockBits() == B_NO_ERROR)
		{
			BFile file(&_shareDir, fileName(), B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
			if (file.InitCheck() == B_NO_ERROR)
			{
			 BBitmap * convertedBitmap = NULL;
			 color_space cs = screenshot->ColorSpace();
			 if ((cs!=B_CMAP8)&&(cs!=B_RGB32))
			 {
				// For some reason, the PNG translator can't handle bit depths other than 8 or 32;
				// So we'll convert other bit depths into 32-bit depth format first.
				convertedBitmap = new BBitmap(screenshot->Bounds(), B_RGB32, true);
				if (convertedBitmap->Lock())
				{
					BView * drawView = new BView(screenshot->Bounds(), NULL, B_FOLLOW_NONE, B_WILL_DRAW);
					convertedBitmap->AddChild(drawView);
					drawView->DrawBitmap(screenshot);
					drawView->Sync();
					convertedBitmap->Unlock();
				}

				if (convertedBitmap->LockBits() != B_NO_ERROR)
				{
					delete convertedBitmap;
					convertedBitmap = NULL;
				}
			 }

			 BBitmapStream stream(convertedBitmap ? convertedBitmap : screenshot);
			 if (roster->Translate(&stream, NULL, NULL, &file, B_PNG_FORMAT) == B_NO_ERROR) 
			 {
				const char * mimeString = "image/png";
				(void) file.WriteAttr("BEOS:TYPE", B_MIME_TYPE, 0, mimeString, strlen(mimeString)+1);
				ret = B_NO_ERROR;
			 }
			 (void) stream.DetachBitmap(convertedBitmap ? &convertedBitmap : &screenshot);
			 if (convertedBitmap)
			 {
				convertedBitmap->UnlockBits();
				delete convertedBitmap;
			 }
			}
			screenshot->UnlockBits();
		}
		delete screenshot;
	 }
	}
	return ret;
}

void ShareWindow::DoScreenShot(const String & fn, ChatWindow * optEchoTo)
{
	String fileName = fn;

	if (fileName.Length() > 0)
	{
	 if (fileName.EndsWith(".png") == false) fileName += ".png";
	}
	else 
	{
	 // Generate a nice filename based on our name and the time
	 fileName = "beshare_screenshot-";
	 fileName += _netClient->GetLocalUserName();
	 fileName += '-';
	 time_t now = time(NULL);
	 char timeBuf[128];
	 ctime_r(&now, timeBuf);
	 fileName += timeBuf;
	 fileName.Replace(' ', '_');	// awkward
	 fileName.Replace('@', '_');	// illegal
	 fileName.Replace('/', '_');	// illegal
	 fileName = fileName.Trim()+".png";
	}

	if (ShareScreenshot(fileName) == B_NO_ERROR)
	{
	 String ad("/me ");
	 ad += str(STR_IS_NOW_SHARING_A_SCREENSHOT);
	 ad += ": beshare:";

	 String fn = fileName;
	 EscapeRegexTokens(fn);
	 fn.Replace(' ', '?');
	 fn.Replace('@', '?');
	 fn.Replace('/', '?');

	 ad += fn;
	 String sid = _netClient->GetLocalSessionID();
	 if ((IsConnected())&&(sid.Length() > 0)) 
	 {
		ad += "@";
		ad += sid;
	 }
	 if (optEchoTo)
	 {
		BMessage textMsg(CHATWINDOW_COMMAND_SEND_CHAT_TEXT);
		textMsg.AddString("text", ad());
		optEchoTo->PostMessage(&textMsg);
	 }
	 else SendChatText(ad, optEchoTo);
	}
	else LogMessage(LOG_ERROR_MESSAGE, str(STR_ERROR_SHARING_SCREENSHOT), NULL, NULL, false, optEchoTo);
}


void
ShareWindow::SetSplit(int which, int pos, bool isPercent, char dir)
{
	SplitPane * sp = NULL;
	switch(which)
	{
	 case 0:	sp = _mainSplit;			break;
	 case 1:	sp = _resultsTransferSplit; break;
	 case 2:	sp = _chatUsersSplit;		break;
	}
	if (sp)
	{
	 uint a = sp->GetAlignment();
	 switch(dir)
	 {
		case 'v': case 'V':	a = B_VERTICAL;	break;
		case 'h': case 'H':	a = B_HORIZONTAL; break;
	 }
	 sp->SetAlignment(a);

	 float extent = (a == B_VERTICAL) ? sp->Bounds().Width() : sp->Bounds().Height();
	 float newPos = (isPercent) ? extent*muscleClamp(((float)pos),0.0f,100.0f)/100.0f : muscleClamp((float)pos, 0.0f, extent);
	 newPos = muscleClamp(newPos, (a == B_VERTICAL) ? sp->GetMinSizeOne().x : sp->GetMinSizeOne().y, (a == B_VERTICAL) ? sp->Bounds().Width()-sp->GetMinSizeTwo().x : sp->Bounds().Height()-sp->GetMinSizeTwo().y);
	 BPoint oldPos = sp->GetBarPosition();
	 sp->SetBarPosition(BPoint((a==B_VERTICAL)?newPos:oldPos.x, (a==B_VERTICAL)?oldPos.y:newPos));
	}
}


void
ShareWindow::FrameResized(float w, float h)
{
	ChatWindow::FrameResized(w, h);
	
	// Show or hide some of the less-necessary top-view controls so that things
	// don't look too messy when the window has been made skinny

	bool queryShouldBeHidden = (_fileNameQueryEntry->Bounds().Width() < 5.0f);
	
	if (queryShouldBeHidden != _queryView->IsHidden()) {
		if (queryShouldBeHidden)
			_queryView->Hide();
		else
			_queryView->Show();
	}

	bool serverShouldBeHidden = (_statusView->Frame().left < 5.0f);
	
	if (serverShouldBeHidden != _serverMenuField->IsHidden()) {
		if (serverShouldBeHidden) {
			_serverMenuField->Hide();
			_serverEntry->Hide();
		} else {
			_serverMenuField->Show();
			_serverEntry->Show();
		}
	}
}

};	// end namespace beshare
