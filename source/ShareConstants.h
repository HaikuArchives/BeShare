#ifndef SHARE_CONSTANTS_H
#define SHARE_CONSTANTS_H

namespace beshare {

#define VERSION_STRING "2.35"

#define DEFAULT_LISTEN_PORT 7000
#define LISTEN_PORT_RANGE	50
#define CLUMP_CHAR			 '\1'

#define BESHARE_MIME_TYPE "application/x-vnd.Haiku-BeShare"
#define FACTORY_DEFAULT_USER_NAME		  "BeShareUser"
#define FACTORY_DEFAULT_USER_STATUS		"here"
#define FACTORY_DEFAULT_USER_AWAY_STATUS "away"

#define AUTO_UPDATER_SERVER	 "haikuarchives.github.io"
#define AUTO_UPDATER_URL	 "http://haikuarchives.github.io/BeShare/servers.txt"

#define BESHARE_SOURCE_URL		"https://github.com/HaikuArchives/BeShare"
#define BESHARE_HOMEPAGE_URL	BESHARE_SOURCE_URL

#define NO_FILE_LIMIT 999999

//Sett this to beta for now (easier to run 2 Beshare on one server)
#define BESHARE_SETTINGS_FILE_NAME "beshare_settings_beta"
#define BESHARE_USER_KEY_FILE_NAME "beshare_user_key_beta"
#define BESHARE_USER_FILE_LOCATION "BeShare User Data"

#define NUM_PARTIAL_HASH_BYTES (64*1024) // 64k seems like a good tradeoff between surety and speed

//Add those files you want to debug
#ifdef DEBUG_BESHARE
//#define DEBUG_BESHAREWINDOW
//#define DEBUG_REMOTEUSERITEM
//#define DEBUG_BESHARENETCLIENT
//#define DEBUG_SHAREFILETRANFER
//#define DEBUG_TRANSFERLISTVIEW
//#define DEBUG_CHATWINDOW
#endif

#ifdef DEBUG_BESHAREWINDOW
#define TRACE_BESHAREWINDOW(x) printf x
#else
#define TRACE_BESHAREWINDOW(x) /* nothing */
#endif

#ifdef DEBUG_TRANSFERLISTVIEW
#define TRACE_TRANSFERLISTVIEW(x) printf x
#else
#define TRACE_TRANSFERLISTVIEW(x) /* nothing */
#endif

// The ShareFileTransfer.cpp file
#ifdef DEBUG_SHAREFILETRANFER
#define TRACE_SHAREFILETRANFER(x) printf x
#else
#define TRACE_SHAREFILETRANFER(x) //nothing
#endif

// The ShareNetClient.cpp file
#ifdef DEBUG_BESHARENETCLIENT
#define TRACE_BESHARENETCLIENT(x) printf x
#else
#define TRACE_BESHARENETCLIENT(x) /* nothing */
#endif

// The RemoteUserItem.cpp file
#ifdef DEBUG_REMOTEUSERITEM
#define TRACE_REMOTEUSERITEM(x) printf x
#else
#define TRACE_REMOTEUSERITEM(x) /* nothing */
#endif

// The ChatWindow.cpp file
#ifdef DEBUG_CHATWINDOW
#define TRACE_CHATWINDOW(x) printf x
#else
#define TRACE_CHATWINDOW(x) /* nothing */
#endif

// types of message that are printed to the text view.
// Formatting and filtering are keyed to this type
enum LogMessageType
{
	LOG_INFORMATION_MESSAGE = 0,	// system info, etc
	LOG_WARNING_MESSAGE,
	LOG_ERROR_MESSAGE,
	LOG_LOCAL_USER_CHAT_MESSAGE,
	LOG_REMOTE_USER_CHAT_MESSAGE,
	LOG_USER_EVENT_MESSAGE,
	LOG_UPLOAD_EVENT_MESSAGE
};

// Available chat filters
enum LogFilterType {
	FILTER_TIMESTAMPS = 0,
	FILTER_USER_EVENTS,
	FILTER_UPLOADS,
	FILTER_CHAT,
	FILTER_PRIVATE_MESSAGES,
	FILTER_INFO_MESSAGES,
	FILTER_WARNING_MESSAGES,
	FILTER_ERROR_MESSAGES,
	FILTER_USER_IDS,
	NUM_FILTERS
};

// Destinations for the chat to be filtered to
enum LogDestinationType {
	DESTINATION_DISPLAY = 0,
	DESTINATION_LOG_FILE,
	NUM_DESTINATIONS
};

// Sound names for the prefs panel
#define SYSTEM_SOUND_USER_NAME_MENTIONED				"BeShare-Name Said"
#define SYSTEM_SOUND_PRIVATE_MESSAGE_RECEIVED			"BeShare-Private Msg"
#define SYSTEM_SOUND_AUTOCOMPLETE_FAILURE				"BeShare-NoComplete"
#define SYSTEM_SOUND_DOWNLOAD_FINISHED					"BeShare-DLFinished"
#define SYSTEM_SOUND_UPLOAD_STARTED						"BeShare-ULStarted"
#define SYSTEM_SOUND_UPLOAD_FINISHED					"BeShare-ULFinished"
#define SYSTEM_SOUND_WATCHED_USER_SPEAKS				"BeShare-WatchedUser"
#define SYSTEM_SOUND_PRIVATE_MESSAGE_WINDOW				"BeShare-PrivateWndw"
#define SYSTEM_SOUND_INACTIVE_CHAT_WINDOW_RECEIVED_TEXT	"BeShare-InactivChat"

#define LIMIT_BANDWIDTH_COMMAND 'lbcc'

};  // end namespace beshare

#endif
