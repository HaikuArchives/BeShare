#ifndef SHARE_NET_CLIENT_H
#define SHARE_NET_CLIENT_H

#include <app/Handler.h>
#include <app/MessageRunner.h>
#include <storage/NodeMonitor.h>
#include <storage/Directory.h>
#include <storage/Entry.h>

#include "besupport/BThread.h"
#include "util/Hashtable.h"
#include "util/String.h"
#include "regex/StringMatcher.h"

#include "BeShareNameSpace.h"

#warning "to be added to muscle?"
namespace muscle {

template <>
class HashFunctor<node_ref>
{
public:
   uint32 operator () (const node_ref & key) const {return (uint32) (((uint64)key.device + key.node) % ((uint32)-1));}
};

};

namespace beshare {

/*
 * This class handles the TCP connection to the MUSCLE server, and
 * also maintains files in the shares (uploads) directory as well
 * as the query-results directory.
 */
class ShareNetClient : public BHandler 
{
public:
   /*
    * (shareDir) is the directory whose files we will make available to other users.
    * (localSharePort) is the port on the local machine that other BeShare clients
    * can connect to in order to download files from us.  It can be -1 if we
    * aren't connect-to-able.
    */
   ShareNetClient(const BDirectory & shareDir, int32 localSharePort);
   ~ShareNetClient();

   /* Closes any current connection to the MUSCLE server and starts a new one. */
   void ConnectToServer(const char * serverName, uint16 port);

   /* Closes any current ocnnection to the MUSCLE server. */
   void DisconnectFromServer();

   /* Removes any current query, clears the current result set, and starts a new query. */
   void StartQuery(const char * sessionExp, const char * fileExp);

   /* Stops the current query, but doesn't clear the query directory. */
   void StopQuery();

   /* Changes the user's handle */
   void SetLocalUserName(const char * newName);

   /* Changes the user's status */
   void SetLocalUserStatus(const char * newStatus);

   /* Changes the local user's advertised upload bandwidth */
   void SetUploadBandwidth(const char * label, uint32 bps);

   /* Accessors */
   const char * GetLocalUserName() const {return _localUserName();}
   const char * GetLocalUserStatus() const {return _localUserStatus();}
   const char * GetLocalHostName() const {return _localHostName();}
   const char * GetLocalSessionID() const {return _localSessionID();}

   /* Uploads a chat message to server, to the given sessionID 
    * (a sessionID of '*' will broadcast to all beshare sessions)
    */
   void SendChatMessage(const char * targetSessionID, const char * text);

   /* Sends a ping message to the specified client(s) */
   void SendPing(const char * targetSessionID);
 
   /* Sends a message to the specified session asking him to connect back to
    * us at the specified port so we can download some files from him.  
    */
   void SendConnectBackRequestMessage(const char * targetSessionID, uint16 port);

   /* Handles BMessages sent to us by the BMessageTransceiverThread object. */
   virtual void MessageReceived(BMessage *message); 

   /* Get/set for whether or not we are behind a firewall.  SetFirewall()
    * should not be called when we are connected to the server */
   void SetFirewalled(bool f);
   bool GetFirewalled() const {return _firewalled;}

   /* Returns an entry_ref for the shared file with the given file name.
    * If no such file exists, the returned entry_ref will not be valid. */
   entry_ref FindSharedFile(const char * fileName) const;

   /** Returns the number of files shared */
   uint32 GetSharedFileCount() const {return _nameToEntry.GetNumItems();}

   /** Called every so often... if there was no traffic recently,
    *  this method may send a packet to the server just to make
    *  sure it's still there.
    */
   void CheckServer();

   /** Called to send a PR_COMMAND_SETPARAMETERS message to the server */
   void SendGetParamsMessage();

   /** Change state from sharing mode to non sharing, or vice versa */
   void SetFileSharingEnabled(bool sa);

   /** Uploads to the server our latest stats about how many people
    *  are getting files from us (if they have changed) 
    */
   void SetUploadStats(uint32 numRequests, uint32 max, bool forceSend);

   /** Returns true iff we are scanning our shares folder for files asynchronously */
   bool IsScanningShares() const {return (_scanSharesThreadID >= 0);}

   /* Send a Message to the server, and update _lastTrafficAt.
    * If (highPriority) is true, the Message is enqueued right away;
    * otherwise, it is put into a "low priority" queue and enqueued
    * only when space is available.  This ensures that high-priority
    * Messages (like outgoing chat text) are not held up behind low
    * priority Messages (like uploading node values)
    */
   void SendMessageToSessions(const MessageRef & msgRef, bool highPriority);

   /** When this is called, we'll make sure our outgoing Messages
     * use the data-compression level specified in the ShareWindow.
     */
   void UpdateEncoding();

   /** Returns the size, in bytes, of the given shared file name... or -1 on an error. */
   off_t GetSharedFileSize(const String & name) const;

private:
   // Our server-side-directory node paths have one of the following forms:
   //   /<hostname>/<sessionid>/beshare/files/<filename>     (shared file info)
   //   /<hostname>/<sessionid>/beshare/name                 (user handle info)
   // The following enumeration gives meaningful names to the levels of the tree
   enum 
   {
      ROOT_DEPTH = 0,         // root node
      HOST_NAME_DEPTH,         
      SESSION_ID_DEPTH,
      BESHARE_HOME_DEPTH,     // used to separate our stuff from other (non-BeShare) data on the same server
      USER_NAME_DEPTH,        // user's handle node would be found here
      FILE_INFO_DEPTH         // user's shared file list is here
   };

   // 'what' codes that we use internally
   enum 
   {
      NET_CLIENT_CONNECTED_TO_SERVER = 0,
      NET_CLIENT_DISCONNECTED_FROM_SERVER,
      NET_CLIENT_NEW_CHAT_TEXT,
      NET_CLIENT_CONNECT_BACK_REQUEST,
      NET_CLIENT_CHECK_FILE_COUNT,
      NET_CLIENT_PING,
      NET_CLIENT_PONG,
      NET_CLIENT_SCAN_THREAD_REPORT
   }; 

   /* Called by MessageReceived, once per incoming Message */
   void MessageReceived(const MessageRef & pmsg);

   /* Sends an error message to the ShareWindow to have it print a message into the chat window */
   void LogError(const char * msg);

   /* Uploads info on the contents of our shareDir to the server, and starts node-monitoring
    * the share dir for changes.
    */
   void BeginSharingFiles();

   /* Returns a MessageRef that contains the shared data info for the given file. */
   status_t GetFileInfo(MessageRef & returnRef, const entry_ref & entryRef, const node_ref & nodeRef, const String & path, off_t & retFileSize);

   /* Convenience method; adds a data-node subscription to the server */
   void AddServerSubscription(const char * subString, bool quietly = false);

   /* Convenience method; removes one or more data-node subscriptions from the server. */
   void RemoveServerSubscription(const char * subStringRegExp);

   /* Convenience method: uploads the given value for the given node on the server */
   void SetDataNodeValue(const char * nodepath, const MessageRef & nodeValue);

   /* Called when a new entry is discovered in our shared dir */
   void NodeCreated(const node_ref & nodeRef, const entry_ref & entryRef);

   /* Called when an entry is removed from our shared dir */
   void NodeRemoved(const node_ref & nodeRef);

   /* Returns "/beshare/files" or "/beshare/fires" depending on whether we're firewalled. */
   const char * GetFilesNodePath() const;
 
   /* Recursive methods for adding/removing dirs from the watch set */
   status_t AddWatchedDirectory(const node_ref & dirNode, const BEntry & entry, thread_id * optWatchThreadID, const String & path, bool diffsOnly);
   void RemoveWatchedDirectory(const node_ref & dirNode);

   status_t GetDirectory(const node_ref & dirNode, const BEntry & entry, BDirectory & retDir) const;

   /* Removes all file nodes from the server, then rescans the local dir, uploading new ones 
    * @param code Either 0 for a 'normal scan', or one or both of the RESCAN_* constants for a diff-scan.
    */
   void ReuploadFiles(uint32 code);

   /* Uploads the Shared File Count to the server */
   void UploadSharedFileCount();

   /* Removes the node monitoring from all shared dirs */
   void UnwatchAllDirs();

   /* Remove all file nodes from the server and stop watching dirs */
   void RemoveFileSharing();

   String _localUserName;
   String _localUserStatus;

   BMessageTransceiverThread * _mtt;

   class CountedEntryRef
   {
   public:
      CountedEntryRef() : _count(0), _fileSize(0) {/* empty */}
      CountedEntryRef(entry_ref ref, off_t fileSize) : _ref(ref), _count(1), _fileSize(fileSize) {/* empty */}

      entry_ref _ref;
      int32 _count;
      off_t _fileSize;
   };

   void RefFilename(const char * fileName, const entry_ref & ref, off_t fileSize);
   void UnrefFilename(const char * fileName);

   void AbortScanSharesThread();
   void ScanSharesThread(bool doRemoves, bool doAdds);
   void NodeRemoveBatch(bool start);
   void BeginScanSharesBatch();
   void EndScanSharesBatch();

   void CheckSendMoreLowPriorityMessages();

   // This is a very silly way to send args... :^P
   static int32 ScanSharesThreadFunc(void * arg)   {((ShareNetClient*)arg)->ScanSharesThread(false, false); return 0;}
   static int32 CheckRemovesThreadFunc(void * arg) {((ShareNetClient*)arg)->ScanSharesThread(true,  false); return 0;}
   static int32 CheckAddsThreadFunc(void * arg)    {((ShareNetClient*)arg)->ScanSharesThread(false,  true); return 0;}
   static int32 CheckBothThreadFunc(void * arg)    {((ShareNetClient*)arg)->ScanSharesThread(true,   true); return 0;}

   Hashtable<String, CountedEntryRef> _nameToEntry;
   Hashtable<node_ref, entry_ref> _nodeToEntry;

   class WatchedDirData 
   {
   public:
      WatchedDirData() {/* empty */}
      WatchedDirData(const entry_ref & er, const String & path) : _entryRef(er), _path(path) {/* empty */}
      
      entry_ref _entryRef;
      String _path;
   };

   Hashtable<node_ref, WatchedDirData> _watchedDirs;

   BDirectory _shareDir;

   // These are set when we get a SETPARAMS reply message
   String _localHostName;
   String _localSessionID;

   // Cache current query criteria to validate results
   bool _queryActive;
   StringMatcher _sessionIDRegExp;
   StringMatcher _fileNameRegExp;

   int32 _localSharePort;

   bool _firewalled;
  
   String _bandwidthLabel;
   uint32 _bandwidth;

   int32 _pingCount;

   void StartFileCountRunner();

   uint32 _lastSentFileCount;
   BMessageRunner * _fileCountRunner;

   uint32 _lastSentUploadCount;
   uint32 _lastSentUploadMax;

   entry_ref _settingsDir;

   BLocker _dirsLock;
   volatile thread_id _scanSharesThreadID;
   uint32 _scanSharesBatchCount;

   bigtime_t _lastTrafficAt;

   bool _sendOnLogins;

   bigtime_t _lastLoginTime;

   enum { 
      RESCAN_REMOVES = 0x01,
      RESCAN_ADDS    = 0x02
   };

   bool _watchingVolumeMounts;
   uint32 _rescanRequestsPending;

   MessageRef _nodeRemoveMessage;
   uint32 _nodeRemoveBatchCount;

   Queue<MessageRef> _lowPriorityMessages;
   bool _sendingLowPriorityMessages;
   MessageRef _lowPriorityDrainedRef;
};

};  // end namespace beshare

#endif
