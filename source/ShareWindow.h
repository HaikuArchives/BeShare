#ifndef SHARE_WINDOW_H
#define SHARE_WINDOW_H

#include <app/Message.h>
#include <app/MessageFilter.h>
#include <interface/Bitmap.h>
#include <interface/Window.h>
#include <interface/TextControl.h>
#include <interface/TextView.h>
#include <interface/Menu.h>
#include <interface/MenuField.h>
#include <interface/MenuItem.h>
#include <storage/Directory.h>

#include "util/Queue.h"
#include "message/Message.h"
#include "besupport/BThread.h"

#include "PrefilledBitmap.h"
#include "CLVListItem.h"

#include "ShareConstants.h"
#include "ChatWindow.h"
#include "ShareNetClient.h"

class ColumnListView;
class CLVColumn;

namespace beshare {

class SplitPane;
class ShareFileTransfer;
class RemoteUserItem;
class RemoteFileItem;
class ShareColumn;
class ShareMIMEInfo;
class PrivateChatWindow;
class TransferListView;
class ColorPicker;

class ShareWindow : public ChatWindow
{
public:
   ShareWindow(uint64 installID, BMessage & settingsMsg, const char * connectServer);
   ~ShareWindow();

   virtual void FrameResized(float w, float h);
   virtual void MessageReceived(BMessage * msg);

   virtual bool QuitRequested();

   virtual void DispatchMessage(BMessage * msg, BHandler * handler);

   void SetConnectStatus(bool isConnecting, bool isConnected);

   void PutUser(const char * sessionID, const char * userName, const char * hostName, int userPort, bool * isBot, uint64 installID, const char * client, bool * supportsPartialHash);
   void RemoveUser(const char * sessionID);
   
   void PutResult(const char * sessionID, const char * fileName, bool isFirewalled, const MessageRef & fileInfo);
   void RemoveResult(const char * sessionID, const char * fileName);

   void FileTransferConnected(ShareFileTransfer * who);
   void FileTransferDisconnected(ShareFileTransfer * who);

   // Called by a ShareFileTransfer who wants his GUI updated.
   void RefreshTransferItem(ShareFileTransfer * item);

   // Utility methods called by ShareFileTransfers to do double buffering
   BBitmap * GetDoubleBufferBitmap(uint32 minWidth, uint32 minHeight);
   BView * GetDoubleBufferView() const {return _doubleBufferView;}

   // Updates the usernames for all transfers to/from (user)
   void RefreshTransfersFor(RemoteUserItem * user);

   // Returns a bitmap representing the icon of the given type, if any
   const BBitmap * GetBitmap(const char * mimeType);
 
   // Given a session ID, returns the user name for that ID, or NULL if the user is unknown
   const char * GetUserNameBySessionID(const char * sessionID) const;

   // When doing a lot of add/remove file items, it's best to
   // bracket your calls with these for, efficieny in updates the GUI.
   void BeginBatchFileResultUpdate();
   void EndBatchFileResultUpdate();

   // Called when someone wants us to connect to him because we're behind a firewall
   // and that's the only way to download files from us.
   void ConnectBackRequestReceived(const char * targetSessionID, uint16 port, const MessageRef & optBase);

   void SetUserUploadStats(const char * sessionID, uint32 cur, uint32 max);
   void SetUserStatus(const char * sessionID, const char * status);
   void SetUserBandwidth(const char * sessionID, const char * label, uint32 bps);
   void SetUserFileCount(const char * sessionID, int32 fileCount);
   void SetUserIsFirewalled(const char * sessionID, bool firewalled); 

   // Tells the net client to send a message to another BeShare client, asking it
   // to connect back to us.
   void SendConnectBackRequestMessage(const char * remoteSessionID, uint16 port);

   // Returns an entry_ref for the shared file with the given file name.
   entry_ref FindSharedFile(const char * fileName) const {return _netClient->FindSharedFile(fileName);}

   // See if any more transfers can be started
   void DequeueTransferSessions();

   // Returns true iff we're firewalled
   bool GetFirewalled() const;

   // Returns true iff we're connected to the server
   bool IsConnected() const {return _isConnected;}

   // Returns a tab-completed version of (origText) into (returnCompletedText), or returns B_ERROR.
   virtual status_t DoTabCompletion(const char * origText, String & returnCompletedText, const char * optMatchExpression) const;

   virtual void LogMessage(LogMessageType type, const char * text, const char * optSessionID=NULL, const rgb_color * optTextColor = NULL, bool isPersonal = false, ChatWindow * optEchoTo = NULL);

   // Our interface with the base class, mostly
   virtual void UpdateTitleBar();
   virtual void GetUserNameForSession(const char * sessionID, String & retUserName) const;
   virtual void GetLocalUserName(String & retLocalUserName) const;
   virtual void GetLocalSessionID(String & retLocalSessionID) const;
   virtual BView * GetChatView() const {return _chatView;}
   virtual String GetQualifiedSharedFileName(const String & name) const;
   virtual status_t ExpandAlias(const String & str, String & ret) const;
   virtual void SendChatText(const String & text, ChatWindow * optEchoTo);

   virtual bool ShowMessageTargets() const {return true;}
   virtual bool ShowTimestamps(LogDestinationType d) const {return _filterItems[d][FILTER_TIMESTAMPS]->IsMarked();} 
   virtual bool ShowUserIDs(LogDestinationType d) const {return _filterItems[d][FILTER_USER_IDS]->IsMarked();} 
   virtual bool OkayToLog(LogMessageType type, LogDestinationType dest, bool isPrivate) const;
   virtual void UpdateColors();

   void SetQueryInProgress(bool qp);

   int GetNumResultsPages() const {return _resultsPages.GetNumItems();}
   int GetCurrentResultsPage() const {return _currentPage;}

   void SetEnableQuitRequester(bool e) {_enableQuitRequester = e;}

   /* This opens a Tracker window for the specified directory */
   void OpenTrackerFolder(const BDirectory & dir);

   /* Send our startup script, if any */
   void SendOnLogins();

   /* Called when we got the PR_RESULT_PARAMETERS message back */
   void ServerParametersReceived(const Message & msg);

   /* Returns a nice human readable string indicating elapsed time (t). */
   String MakeTimeElapsedString(int64 t) const;

   /* Returns true iff the user wants file sharing to be enabled */
   bool GetFileSharingEnabled() const {return _sharingEnabled->IsMarked();}

   /* Returns true iff "Retain file paths" is enabled */
   bool GetRetainFilePaths() const {return _retainFilePaths->IsMarked();}

   /** Called when our net client recieves a callback-rejected Message */
   void TransferCallbackRejected(const char * from, uint64 timeLeft);

   /* Returns a random 64-bit number that stays constant for this installation
    * of BeShare, even across reboots, etc.
    */
   uint64 GetInstallID() const {return _installID;}

   /** Increases the value of our total-bytes-uploaded or total-bytes-downloaded counter */
   void AddToTransferCounts(bool isUpload, uint32 numBytes) {if (isUpload) _totalBytesUploaded += ((uint64)numBytes); else _totalBytesDownloaded += ((uint64)numBytes);}

   /** Called when the server disconnects us.  Will start the auto-reconnect process. */
   void BeginAutoReconnect();

   /** Returns true iff we are scanning our shares folder for files asynchronously */
   bool IsScanningShares() const {return _netClient ? _netClient->IsScanningShares() : false;}

   /** Called by our ShareNetClient when it's done scanning */
   void SharesScanComplete();

   // These methods are called by the application object when responding to inter-app messages
   void DoScreenShot(const String & fileName, ChatWindow * optEchoTo);
   void SetSplit(int which, int pos, bool isPercent, char dir);
   void PauseAllUploads();
   void ResumeAllUploads();
   void SetLocalUserName(const char * name);
   void SetLocalUserStatus(const char * status);
   void SetServer(const char * server);
   void SetQuery(const char * query);
   void SendMessageToServer(const MessageRef & msg);

   // Called by the drag&drop code to get the server's address
   String GetConnectedTo() const {return _connectedTo;}

   // Returns our current zlib-compression level setting (0-9)
   uint32 GetCompressionLevel() const {return _compressionLevel;}

   // 'what' codes that are used internally by this class
   enum 
   {
      SHAREWINDOW_COMMAND_RECONNECT_TO_SERVER = '$win',
      SHAREWINDOW_COMMAND_DISCONNECT_FROM_SERVER,
      SHAREWINDOW_COMMAND_ABOUT,
      SHAREWINDOW_COMMAND_SEND_CHAT_TEXT,
      SHAREWINDOW_COMMAND_CHANGE_FILE_NAME_QUERY,
      SHAREWINDOW_COMMAND_USER_CHANGED_NAME,
      SHAREWINDOW_COMMAND_USER_CHANGED_SERVER,
      SHAREWINDOW_COMMAND_ENABLE_QUERY,
      SHAREWINDOW_COMMAND_DISABLE_QUERY,
      SHAREWINDOW_COMMAND_TOGGLE_COLUMN,
      SHAREWINDOW_COMMAND_BEGIN_DOWNLOADS,
      SHAREWINDOW_COMMAND_CANCEL_DOWNLOADS,
      SHAREWINDOW_COMMAND_CLEAR_FINISHED_DOWNLOADS,
      SHAREWINDOW_COMMAND_RESULT_SELECTION_CHANGED,
      SHAREWINDOW_COMMAND_REMOVE_SESSION,
      SHAREWINDOW_COMMAND_NEW_PEER_SESSION,
      SHAREWINDOW_COMMAND_TOGGLE_FIREWALLED,
      SHAREWINDOW_COMMAND_CLEAR_CHAT_LOG,
      SHAREWINDOW_COMMAND_SET_UPLOAD_LIMIT,
      SHAREWINDOW_COMMAND_SET_ADVERTISED_BANDWIDTH,
      SHAREWINDOW_COMMAND_RESET_LAYOUT,
      SHAREWINDOW_COMMAND_SET_DOWNLOAD_LIMIT,
      SHAREWINDOW_COMMAND_SELECT_USER,
      SHAREWINDOW_COMMAND_OPEN_SHARED_FOLDER,
      SHAREWINDOW_COMMAND_OPEN_DOWNLOADS_FOLDER,
      SHAREWINDOW_COMMAND_TOGGLE_FULL_USER_QUERIES,
      SHAREWINDOW_COMMAND_USER_SELECTED_SERVER,
      SHAREWINDOW_COMMAND_SELECT_LANGUAGE,
      SHAREWINDOW_COMMAND_LAUNCH_TRANSFER_ITEM,
      SHAREWINDOW_COMMAND_CHECK_FOR_MORIBUND_CONNECTIONS,
      SHAREWINDOW_COMMAND_DEPRECATED_1,
      SHAREWINDOW_COMMAND_SET_UPLOAD_PER_USER_LIMIT,
      SHAREWINDOW_COMMAND_TOGGLE_AUTOCLEAR_COMPLETED_DOWNLOADS,
      SHAREWINDOW_COMMAND_SET_DOWNLOAD_PER_USER_LIMIT,
      SHAREWINDOW_COMMAND_TOGGLE_LOGIN_ON_STARTUP,
      SHAREWINDOW_COMMAND_OPEN_PRIVATE_CHAT_WINDOW,
      SHAREWINDOW_COMMAND_SAVE_ATTRIBUTE_PRESET,
      SHAREWINDOW_COMMAND_RESTORE_ATTRIBUTE_PRESET,
      SHAREWINDOW_COMMAND_QUERY_IN_PROGRESS_ANIM,
      SHAREWINDOW_COMMAND_NEXT_PAGE,
      SHAREWINDOW_COMMAND_PREVIOUS_PAGE,
      SHAREWINDOW_COMMAND_PRINT_STARTUP_MESSAGES,
      SHAREWINDOW_COMMAND_TOGGLE_CHAT_FILTER,
      SHAREWINDOW_COMMAND_TOGGLE_FILE_LOGGING,
      SHAREWINDOW_COMMAND_RESTORE_SORTING,
      SHAREWINDOW_COMMAND_SET_PAGE_SIZE,
      SHAREWINDOW_COMMAND_SWITCH_TO_PAGE,
      SHAREWINDOW_COMMAND_USER_SELECTED_USER_NAME,
      SHAREWINDOW_COMMAND_UNIDLE,
      SHAREWINDOW_COMMAND_SET_AUTO_AWAY,
      SHAREWINDOW_COMMAND_TOGGLE_FILE_SHARING_ENABLED,
      SHAREWINDOW_COMMAND_BAN_USER,
      SHAREWINDOW_COMMAND_OPEN_LOGS_FOLDER,
      SHAREWINDOW_COMMAND_USER_SELECTED_USER_STATUS,
      SHAREWINDOW_COMMAND_USER_CHANGED_STATUS,
      SHAREWINDOW_COMMAND_TOGGLE_RETAIN_FILE_PATHS,
      SHAREWINDOW_COMMAND_TOGGLE_AUTOUPDATE_SERVER_LIST,
      SHAREWINDOW_COMMAND_AUTO_RECONNECT,
      SHAREWINDOW_COMMAND_SHOW_COLOR_PICKER,
      SHAREWINDOW_COMMAND_SET_COMPRESSION_LEVEL,
      SHAREWINDOW_COMMAND_TOGGLE_SHORTEST_UPLOADS_FIRST
   };

protected:
   virtual void UserChatted();
   virtual const char * GetLogFileNamePrefix() const {return "BeShare";}

private:
   status_t SetupNewDownload(const RemoteUserItem * user, ShareFileTransfer * xfer, bool forceRemoteIsFirewalled);
   uint32 ParseRemoteIP(const char * addr) const;
   void RestartDownloadsFor(const RemoteUserItem * user);

   static int CompareFunc(const CLVListItem* item1, const CLVListItem* item2, int32 sort_key);
   static int UserCompareFunc(const CLVListItem* item1, const CLVListItem* item2, int32 sort_key);
   int Compare(const RemoteFileItem * rf1, const RemoteFileItem * rf2, int32 sort_key) const;

   friend class RemoteUserItem; 
   friend class RemoteFileItem;
   friend class ShareColumn;
   friend class ShareMIMEInfo;
   friend class TransferListView;

   void LogStat(int statName, const char * statValue);

   // called by RemoteUserItem
   void AddFileItem(RemoteFileItem * item);
   void RemoveFileItem(RemoteFileItem * item);
   void RefreshFileItem(RemoteFileItem * item);
   void RefreshUserItem(RemoteUserItem * item);

   const char * GetFileCellText(const RemoteFileItem * item, int32 columnIndex) const;
   const BBitmap * GetBitmap(const RemoteFileItem * file, int32 columnIndex) const;

   void UpdateConnectStatus(bool updateTitleToo);
   void UpdateQueryEnabledStatus();
   void SetQueryEnabled(bool enabled, bool putInQueryMenu = true);

   void ClearUsers();   // also clears results, as results are held by users
   void ClearResults();

   BMenu * MakeLimitSubmenu(const BMessage & settingsMsg, uint32 code, const char * label, const char * fieldName, uint32 & var);

   void AddBandwidthOption(BMenu * qMenu, const char * label, int32 bps);

   void CreateColumn(ShareMIMEInfo * optMIMEInfo, const char * columnName, bool remote);
   ShareMIMEInfo * CacheMIMETypeInfo(const char * mimeString);

   void UpdateDownloadButtonStatus();
   void RequestDownloads(const BMessage & filelistMsg, const BDirectory & downloadDir, BPoint *droppoint);
   void ReprioritizeUploads();

   status_t ShareScreenshot(const String & fileName);

   BDirectory _shareDir;
   BDirectory _downloadsDir;

   bool _queryEnabled;
   String _queryOnConnect;

   bool _isConnecting;
   bool _isConnected;
   String _connectedTo;

   BButton * _enableQueryButton;
   BButton * _disableQueryButton;

   BButton * _clearFinishedDownloadsButton;
   BButton * _requestDownloadsButton;
   BButton * _cancelTransfersButton;

   BMenu        * _queryMenu;
   BView        * _queryView;

   BMenu        * _serverMenu;
   BMenuField   * _serverMenuField;
   BTextControl * _serverEntry;

   BMenu        * _userNameMenu;
   BTextControl * _userNameEntry;

   BMenu        * _userStatusMenu;
   BTextControl * _userStatusEntry;

   BTextControl * _fileNameQueryEntry;

   BMenuBar * _menuBar;

   BMenuItem * _firewalled;
   BMenuItem * _sharingEnabled;

   BMenuItem * _filterItems[NUM_DESTINATIONS][NUM_FILTERS];
   BMenuItem * _toggleFileLogging;

   BMenuItem * _connectMenuItem;
   BMenuItem * _disconnectMenuItem;
   BMenuItem * _fullUserQueries;
   BMenuItem * _shortestUploadsFirst;
   BMenuItem * _autoClearCompletedDownloads;
   BMenuItem * _loginOnStartup;
   BMenuItem * _retainFilePaths;
   BMenuItem * _autoUpdateServers;
 
   BMenu * _attribMenu;

   BMenuItem * _colorItem;
               
   ColumnListView * _resultsView;
   ColumnListView * _usersView;

   Queue<Hashtable<RemoteFileItem *, bool> * > _resultsPages;
   uint32 _currentPage;

   int64 _bytesShown;

   TransferListView * _transferList;

   ShareNetClient * _netClient;

   PrefilledBitmap _defaultBitmap;

   Hashtable<const char *, RemoteUserItem *> _users;

   Hashtable<String, float> _activeAttribs;        /* attribs the user wants to see, and their widths */
   Hashtable<const char *, ShareColumn *> _columns;        /* columns that are available */
   Hashtable<const char *, BMenuItem *> _attribMenuItems;  /* toggles for available columns */
   Hashtable<const char *, ShareMIMEInfo *> _mimeInfos;    /* cached MIME information */

   Hashtable<ShareMIMEInfo *, bool> _emptyMimeInfos;       /* MIME infos that aren't in the BMenu yet */

   SplitPane * _resultsTransferSplit;
   SplitPane * _mainSplit;
   SplitPane * _chatUsersSplit;

   BView * _chatView;
   BView * _statusView;

   BList _tempAddList;    // batch up items to be added, for efficiency

   uint32 CountActiveSessions(bool upload, const char * optForSession) const;
   uint32 CountUploadSessions() const;

   uint32 _maxSimultaneousUploadSessions;    // the max # we allow (to avoid too much congestion)
   uint32 _maxSimultaneousUploadSessionsPerUser;  // the max # we allow per user (to avoid too much congestion)
   uint32 _maxSimultaneousDownloadSessions;  // the max # we allow (to avoid too much congestion)
   uint32 _maxSimultaneousDownloadSessionsPerUser;  // the max # we allow per user (to avoid too much congestion)
   uint32 _uploadBandwidth;

   BMessageRunner * _connectionReaper;   // Every so often, causes us to check for moribund connections & kill them

   void RestoreSplitPane(const BMessage & settingMsg, SplitPane * sp, const char * name) const;
   void SaveSplitPane(BMessage & settingsMsg, const SplitPane * sp, const char * name) const;

   void ResetLayout();

   void SortResults();

   void GenerateSettingsMessage(BMessage & msg);

   // If a session is queued and waiting, this method will start it up.
   void DequeueTransferSessions(bool upload);

   // Utility methods for saving/restoring userwindow column widths
   void AddUserColumn(const BMessage & settingsMsg, int labelID, float defaultWidthPercentage, const char * optForceLabel, uint32 extraFlags);
   void SaveUserColumn(BMessage & settingsMsg, int labelID, CLVColumn * col) const;

   void SavePrivateWindowInfo(const BMessage & msg);

   void AddServerItem(const char * serverName, bool quiet, int index);
   void RemoveServerItem(const char * serverName, bool quiet);
   void AddUserNameItem(const char * userName);
   void AddUserStatusItem(const char * userStatus);

   void SaveAttributesPreset(BMessage & saveMsg);
   void RestoreAttributesPreset(const BMessage & restoreMsg);

   void ReconnectToServer();
 
   void DoAutoReconnect();
   void ResetAutoReconnectState(bool resetCountToo);

   int MatchUserName(const char * un, String & result, const char * optMatchExpression) const;

   bool MatchesUserFilter(const RemoteUserItem * user, const char * filterString) const;

   BMenuItem * CreatePresetItem(int32 what, int32 which, bool enabled, bool shiftShortcut) const;

   status_t ParseUserTargets(const char * text, Hashtable<RemoteUserItem *, String> & table, String & setTargetStr, String & setRestOfStr);

   void UpdatePrivateWindowUserList(PrivateChatWindow * w, const char * target);
   void SendToPrivateChatWindows(BMessage & msg, const RemoteUserItem * user);
   void SendOutMessageOrPing(const String & text, ChatWindow * optEchoTo, bool isPing);

   void DrawQueryInProgress(bool inProgress);  // draws the little animation, ooh!
   void SwitchToPage(int page);   // sets the current page
   void AddResultsItemList(const BList & list);
   void MakeAway();

   void UpdateLRUMenu(BMenu * menu, const char * lookfor, uint32 what, const char * fieldName, int maxSize, bool caseSensitive, uint32 maxLabelLen = ((uint32)-1));
   void RemoveLRUItem(BMenu * menu, const BMessage & msg);

   void UpdatePagingButtons();

   uint64 IPBanTimeLeft(uint32 ip);  // returns number of microseconds left in the ban for ip, or 0 if not banned.
   void LogPattern(const char * preamble, const String & pattern, ChatWindow * optEchoTo);
   void LogRateLimit(const char * preamble, uint32 limit, ChatWindow * optEchoTo);

   bool AreMessagesEqual(const BMessage & m1, const BMessage & m2) const;
   bool IsFieldSuperset(const BMessage & m1, const BMessage & m2) const;

   void UpdaterCommandReceived(const char * key, const char * value);
   void SetBandwidthLimit(bool upload, const String & lowerText, ChatWindow * optEchoTo);

   void UpdatePrivateChatWindowsColors();
   MessageRef MakeBannedMessage(uint64 time, const MessageRef & optBase) const;

   BButton * _prevPageButton;
   BButton * _nextPageButton;

   uint32 _pageSize;

   int32 _language;
   bool _languageSet;  // false iff the language is a default setting and not to be saved

   BMessage _stateMessage;  // held to compare with our current state periodically

   String _lastPrivateMessageTarget;
   bool _messageWasSentToPrivateChatWindow;  // set when we want to log to a file but not to main display...
   Queue<BMessage> _privateChatInfos;
   Hashtable<PrivateChatWindow *, String> _privateChatWindows;
   BMessage _attribPresets[10];
   BMenuItem * _restorePresets[10];  // so we can enable them as necessary
   BMessageRunner * _queryInProgressRunner;
   String _ignorePattern;
   String _watchPattern;
   String _autoPrivPattern;

   float _radarSweep;
   bool _lastInProgress;

   bool _firstUserDefinedAttribute;
   bool _enableQuitRequester;

   bool _idle;
   uint32 _idleTimeoutMinutes;
   bigtime_t _lastInteractionAt;

   String _awayStatus;
   String _oneTimeAwayStatus;
   String _revertToStatus;

   Queue<String> _onLoginStrings;
   String _onIdleString;
   bool _idleSendPending;
 
   Hashtable<uint32, uint64> _bans;  // users who may not d/l from us
   Hashtable<String, String> _aliases;
  
   bool _showServerStatus;
   uint64 _installID;     // used to identify this user between sessions

   BAcceptSocketsThread _acceptThread;  // used to accept incoming connections from peers
   BMessageTransceiverThread _checkServerListThread;

   uint32 _autoReconnectAttemptCount;
   BMessageRunner * _autoReconnectRunner;

   uint32 _maxDownloadRate;
   uint32 _maxUploadRate;

   ColorPicker * _colorPicker;

   BBitmap * _doubleBufferBitmap;  // for the ShareFileTransfers to use
   BView * _doubleBufferView;

   uint64 _totalBytesUploaded;
   uint64 _totalBytesDownloaded;

   uint32 _compressionLevel;
   uint32 _dequeueCount;  // to prevent infinite recursion of DequeueTransferSessions()
};

};  // end namespace beshare

#endif
