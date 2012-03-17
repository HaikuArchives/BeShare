#ifndef SHARE_CONSTANTS_H
#define SHARE_CONSTANTS_H

#include "BeShareNameSpace.h"

namespace beshare {

#define VERSION_STRING "2.29 (MUSCLE 4.63)"

#define DEFAULT_LISTEN_PORT 7000
#define LISTEN_PORT_RANGE   50
#define CLUMP_CHAR          '\1' 

#define BESHARE_MIME_TYPE "application/x-vnd.Sugoi-BeShare"
#define FACTORY_DEFAULT_USER_NAME        "binky"
#define FACTORY_DEFAULT_USER_STATUS      "here"
#define FACTORY_DEFAULT_USER_AWAY_STATUS "away"

#define AUTO_UPDATER_SERVER "beshare.tycomsystems.com"
#define AUTO_UPDATER_URL    "http://" AUTO_UPDATER_SERVER "/servers.txt"

#define BESHARE_BEBITS_URL   "http://www.bebits.com/app/1330/"
#define BESHARE_HOMEPAGE_URL "http://www.lcscanada.com/beshare/"

#define NO_FILE_LIMIT 999999

#define NUM_PARTIAL_HASH_BYTES (64*1024) // 64k seems like a good tradeoff between surety and speed

// types of message that are printed to the text view.
// Formatting and filtering are keyed to this type
enum LogMessageType
{
   LOG_INFORMATION_MESSAGE = 0,   // system info, etc
   LOG_WARNING_MESSAGE,
   LOG_ERROR_MESSAGE,
   LOG_LOCAL_USER_CHAT_MESSAGE,
   LOG_REMOTE_USER_CHAT_MESSAGE,
   LOG_USER_EVENT_MESSAGE,
   LOG_UPLOAD_EVENT_MESSAGE,
   NUM_LOG_MESSAGE_TYPES
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
#define SYSTEM_SOUND_USER_NAME_MENTIONED                  "BeShare-Name Said"
#define SYSTEM_SOUND_PRIVATE_MESSAGE_RECEIVED             "BeShare-Private Msg"
#define SYSTEM_SOUND_AUTOCOMPLETE_FAILURE                 "BeShare-NoComplete"
#define SYSTEM_SOUND_DOWNLOAD_FINISHED                    "BeShare-DLFinished"
#define SYSTEM_SOUND_UPLOAD_STARTED                       "BeShare-ULStarted"
#define SYSTEM_SOUND_UPLOAD_FINISHED                      "BeShare-ULFinished"
#define SYSTEM_SOUND_WATCHED_USER_SPEAKS                  "BeShare-WatchedUser"
#define SYSTEM_SOUND_PRIVATE_MESSAGE_WINDOW               "BeShare-PrivateWndw"
#define SYSTEM_SOUND_INACTIVE_CHAT_WINDOW_RECEIVED_TEXT   "BeShare-InactivChat"

#define LIMIT_BANDWIDTH_COMMAND 'lbcc'

};  // end namespace beshare

#endif
