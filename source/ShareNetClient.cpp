#include <unistd.h>
#include <netdb.h>
#include <kernel/fs_attr.h>
#include <storage/Mime.h>
#include <storage/Directory.h>
#include <storage/Entry.h>
#include <storage/File.h>
#include <storage/FindDirectory.h>
#include <storage/NodeInfo.h>
#include <storage/Path.h>
#include <support/Autolock.h>

#include "besupport/ConvertMessages.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/PathMatcher.h"
#include "iogateway/MessageIOGateway.h"
#include "util/NetworkUtilityFunctions.h"
#include "zlib/ZLibUtilityFunctions.h"

#include "ShareStrings.h"
#include "ShareConstants.h"
#include "ShareFileTransfer.h"
#include "ShareNetClient.h"
#include "ShareWindow.h"

namespace beshare {


ShareNetClient::ShareNetClient(const BDirectory & shareDir, int32 localPort) :
	_mtt(NULL), 
	_shareDir(shareDir),
	_queryActive(false),
	_localSharePort(localPort),
	_firewalled(false),
	_bandwidthLabel("?"),
	_bandwidth(0),
	_pingCount(0),
	_lastSentFileCount((uint32)-1L),
	_fileCountRunner(NULL),
	_lastSentUploadCount((uint32)-1L),
	_lastSentUploadMax((uint32)-1L),
	_scanSharesThreadID(-1),
	_scanSharesBatchCount(0),
	_lastTrafficAt(0),
	_sendOnLogins(false),
	_watchingVolumeMounts(false),
	_rescanRequestsPending(0),
	_nodeRemoveBatchCount(0),
	_sendingLowPriorityMessages(false)
{
	BSTRACE(("ShareNetClient::ShareNetClient begin\n"));
	BPath ucPath;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &ucPath) == B_NO_ERROR) {
		BEntry entry(ucPath.Path(), true);
		(void) entry.GetRef(&_settingsDir);
	}
}


ShareNetClient::~ShareNetClient()
{
	DisconnectFromServer();
}

void
ShareNetClient ::
StartFileCountRunner()
{
   if (_fileCountRunner == NULL)
   {
      UploadSharedFileCount();  // first one goes right away
      BMessenger toMe(this);
      _fileCountRunner = new BMessageRunner(toMe, new BMessage(NET_CLIENT_CHECK_FILE_COUNT), 1000000);   // 1 message/second
   }
}

void
ShareNetClient ::
UpdateEncoding()
{
   if (_mtt)
   {
      int32 encoding = MUSCLE_MESSAGE_ENCODING_DEFAULT + ((ShareWindow *)Looper())->GetCompressionLevel();
      _mtt->SetOutgoingMessageEncoding(encoding);
      MessageRef zlibRef = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
      if (zlibRef())
      {
         zlibRef()->AddInt32(PR_NAME_REPLY_ENCODING, encoding);
         _mtt->SendMessageToSessions(zlibRef);
      }
   }
}


void
ShareNetClient::ConnectToServer(const char* serverName, uint16 port)
{
	printf("ConnectToServer begin\n");
	DisconnectFromServer();  // clean out any old connection, as we don't want two at once.

	BMessenger toMe(this);
	_mtt = new BMessageTransceiverThread(toMe);
	if ((_mtt->StartInternalThread() == B_NO_ERROR)
		&& (_mtt->AddNewConnectSession(serverName, port) == B_NO_ERROR)) {
			printf("ConnectToServer true\n");
		UpdateEncoding();
			printf("ConnectToServer true next\n");
		((ShareWindow*)Looper())->SetConnectStatus(true, false);
		printf("ConnectToServer true last\n");
	} else {
		printf("ConnectToServer else\n");
		_mtt->ShutdownInternalThread();
		((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, str(STR_ERROR_COULDNT_CREATE_CONNECT_THREAD));
		delete _mtt;
		_mtt = NULL;
	}
	printf("ConnectToServer end\n");
}


void
ShareNetClient ::
DisconnectFromServer()
{
   AbortScanSharesThread();

   ((ShareWindow *)Looper())->SetQueryInProgress(false);

   delete _fileCountRunner;
   _fileCountRunner = NULL;
   _lastSentFileCount = (uint32)-1L;
   _lastLoginTime = 0;
   _localSessionID = "";

   if (_mtt)
   {
      _mtt->ShutdownInternalThread();
      delete _mtt;
      _mtt = NULL;

      // since we're no longer connected, there's no point in watching the shared dirs anymore
      UnwatchAllDirs();

      ((ShareWindow *)Looper())->SetConnectStatus(false, false);
   }
   else ((ShareWindow*)Looper())->UpdateTitleBar();

   // Anything that was waiting to be slow-sent is irrelevant now, since the connection is gone
   _lowPriorityMessages.Clear();
   _sendingLowPriorityMessages = false;
}


/* Should only be called when the share scan thread is known not to be running */
void
ShareNetClient::UnwatchAllDirs()
{
	for (HashtableIterator<node_ref, WatchedDirData> iter(_watchedDirs.GetIterator()); iter.HasData(); iter++) {
		(void) watch_node(&iter.GetKey(), B_STOP_WATCHING, this);
	}

	_watchedDirs.Clear();
	_nodeToEntry.Clear();
	_nameToEntry.Clear();
}


void
ShareNetClient::StartQuery(const char* sessionIDRegExp, const char* fileNameRegExp)
{
   if (_mtt)
   {
      StopQuery();          // get rid of the currently running query, if any

      // This path string tells muscled which files it should inform us about.
      // Since we may be basing part of our query on the session ID, we use
      // the full path string for this query.
      String temp = "SUBSCRIBE:/*/";
      temp += sessionIDRegExp;
      temp += "/beshare/";
      temp += _firewalled ? "files/" : "fi*/";  // If we're firewalled, we can only get non-firewalled files; else both types
      temp += fileNameRegExp;
      AddServerSubscription(temp());

      // Send a ping to the server.  When we get it back, we know the
      // initial scan of the query is done!
      MessageRef ping = GetMessageFromPool(PR_COMMAND_PING);
      if (ping())
      { 
         ping()->AddInt32("count", _pingCount++);
         SendMessageToSessions(ping, true);
      }
      ((ShareWindow *)Looper())->SetQueryInProgress(true);

      _queryActive = true;
      _sessionIDRegExp.SetPattern(sessionIDRegExp);
      _fileNameRegExp.SetPattern(fileNameRegExp);
   }
}


void
ShareNetClient ::
SendMessageToSessions(const MessageRef & msgRef, bool highPriority)
{
   _lastTrafficAt = system_time();
   if (_mtt) 
   {
      if (highPriority) _mtt->SendMessageToSessions(msgRef);
      else
      {
         _lowPriorityMessages.AddTail(msgRef);
         CheckSendMoreLowPriorityMessages();
      }
   }
}

void
ShareNetClient ::
CheckSendMoreLowPriorityMessages()
{
   if ((_sendingLowPriorityMessages == false)&&(_mtt))
   {
      const uint32 LOW_PRIORITY_CHUNK_SIZE = 4 * 1024;  // are 4k chunks a good balance?
      uint32 numBytesSent = 0;
      MessageRef temp;
      while((numBytesSent < LOW_PRIORITY_CHUNK_SIZE)&&(_lowPriorityMessages.RemoveHead(temp) == B_NO_ERROR))
      {
         numBytesSent += temp()->FlattenedSize();
         SendMessageToSessions(temp, true);
      }
      if (numBytesSent > 0) _sendingLowPriorityMessages = (_mtt->RequestOutputQueuesDrainedNotification(_lowPriorityDrainedRef) == B_NO_ERROR);
   }
}

void
ShareNetClient ::
CheckServer()
{
   if ((_mtt)&&(system_time() > _lastTrafficAt + 5*60*1000000LL)) SendMessageToSessions(GetMessageFromPool(PR_COMMAND_NOOP), true);
}


void
ShareNetClient ::
StopQuery()
{
   // Instead of having to remember what the exact subscription string
   // was, it's easier to just tell the muscle server to cancel all
   // subscriptions that have the substring 'beshare/fi' in them.
   if (_mtt) 
   {
      RemoveServerSubscription("SUBSCRIBE:*beshare/fi*");
      
      // This message will tell the server to cancel any update messages
      // that are still pending to be sent to us.  After all, we don't
      // care about them anymore!
      MessageRef cancel = GetMessageFromPool(PR_COMMAND_JETTISONRESULTS);
      if (cancel())
      {
         cancel()->AddString(PR_NAME_KEYS, "beshare/fi*/*");  // all queued-up file results should be cancelled
         SendMessageToSessions(cancel, true);
      }
      else WARN_OUT_OF_MEMORY;
   }
   _queryActive = false;
}

void 
ShareNetClient ::
UploadSharedFileCount()
{
   // Don't want to wait for the lock;  if it's locked, we'll get updated later anyway...
   if (_dirsLock.LockWithTimeout(0) == B_NO_ERROR)
   {
      uint32 fileCount = ((ShareWindow*)Looper())->GetFileSharingEnabled() ? _nameToEntry.GetNumItems() : 0;
      if (fileCount == _lastSentFileCount)
      {
         delete _fileCountRunner;
         _fileCountRunner = NULL; 
      }
      else if (_mtt)
      {
         MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
         if (uploadMsg())
         {
            MessageRef data = GetMessageFromPool();
            if (data())
            {
               data()->AddInt32("filecount", fileCount);
               uploadMsg()->AddMessage("beshare/filecount", data);
               SendMessageToSessions(uploadMsg, true);
               _lastSentFileCount = fileCount;
               ((ShareWindow*)Looper())->UpdateTitleBar();
            }
            else WARN_OUT_OF_MEMORY;
         }
         else WARN_OUT_OF_MEMORY;
      }
      _dirsLock.Unlock();
   }
}


void
ShareNetClient::
BeginScanSharesBatch()
{
   ++_scanSharesBatchCount;
}

void
ShareNetClient::
EndScanSharesBatch()
{
   if ((--_scanSharesBatchCount == 0)&&(_scanSharesThreadID < 0)) ((ShareWindow*)Looper())->SharesScanComplete();
}

void
ShareNetClient ::
MessageReceived(BMessage * msg)
{
   switch(msg->what)
   {
      case NET_CLIENT_SCAN_THREAD_REPORT:
      {
         BeginScanSharesBatch();
         _scanSharesThreadID = -1;  // clear it, it's done
         
         bool success;
         if (msg->FindBool("success", &success) == B_NO_ERROR)
         {
            if (success)
            {
               char buf[100];
               sprintf(buf, str(STR_SHARING_PERCENTI_LOCAL_FILES), _nameToEntry.GetNumItems());
               ((ShareWindow*)Looper())->LogMessage(LOG_INFORMATION_MESSAGE, buf);
            }
            else ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, str(STR_COULDNT_FIND_SHARED_SUBDIRECTORY_FILE_SHARING_DISABLED));
         }
         UploadSharedFileCount();

         if (_rescanRequestsPending)
         {
            uint32 r = _rescanRequestsPending;
            _rescanRequestsPending = 0;
            ReuploadFiles(r);
         }
         EndScanSharesBatch();
      }
      break;

      case NET_CLIENT_CHECK_FILE_COUNT:
         UploadSharedFileCount();
      break;

      case MUSCLE_THREAD_SIGNAL:
      {
         // Check for any new events from our network thread (connected to the muscle server)
         uint32 code;
         MessageRef next;
         while((_mtt)&&(_mtt->GetNextEventFromInternalThread(code, &next) >= 0))
         {
            switch(code)
            {
               case MTT_EVENT_OUTPUT_QUEUES_DRAINED:
                  _sendingLowPriorityMessages = false;
                  CheckSendMoreLowPriorityMessages();
               break;

               case MTT_EVENT_SESSION_CONNECTED:
               {
                  _lastLoginTime = system_time();

                  // Ask the server to send us our parameters.  The parameters
                  // message it will send contains our machine's hostname and session
                  // ID, amongst other things (we'll need to know those later)
                  SendGetParamsMessage();

                  // fixed for v2.26: Subscribe to session IDs to be sure we know when people leave
                  AddServerSubscription("SUBSCRIBE:/*/*");

                  // Subscribe to all users' misc settings
                  AddServerSubscription("SUBSCRIBE:beshare/*");

                  // upload our user name...
                  SetLocalUserName(_localUserName());

                  // upload our user status...
                  SetLocalUserStatus(_localUserStatus());

                  ((ShareWindow *)Looper())->SetConnectStatus(true, true);

                  // When this pong gets back to us, it's then a good time
                  // to process our onLogin script... all users and so on will be known then.
                  SendMessageToSessions(GetMessageFromPool(PR_COMMAND_PING), true);
                  _sendOnLogins = true;

                  // and upload file info about any files in our shared dir tree
                  if (((ShareWindow*)Looper())->GetFileSharingEnabled()) ReuploadFiles(0);
               }
               break;

               case MTT_EVENT_INCOMING_MESSAGE:    
                  MessageReceived(next);
               break;

               case MTT_EVENT_SESSION_DISCONNECTED:
                  DisconnectFromServer();  // clean up the dead BMessageTransceiverThread object
                  ((ShareWindow*)Looper())->BeginAutoReconnect();
               break;
            }
         }
      }
      break;

      case B_NODE_MONITOR:   // something in our share directory has changed!
      {
         int32 opCode;
         node_ref fileNodeRef;
         if ((((ShareWindow*)Looper())->GetFileSharingEnabled())&&(msg->FindInt32("opcode", &opCode) == B_NO_ERROR))
         {
             if ((opCode == B_DEVICE_MOUNTED)||(opCode == B_DEVICE_UNMOUNTED))
             {
                uint32 code = (opCode == B_DEVICE_MOUNTED) ? RESCAN_ADDS : RESCAN_REMOVES;
                if (_scanSharesThreadID >= 0) _rescanRequestsPending |= code;
                                         else ReuploadFiles(code);
             }
             else if ((msg->FindInt64("node",   &fileNodeRef.node)   == B_NO_ERROR)&&
                      (msg->FindInt32("device", &fileNodeRef.device) == B_NO_ERROR))
             {
                BAutolock m(_dirsLock);
                switch(opCode)
                {
                   case B_ENTRY_CREATED:
                   {
                      const char * name;
                      ino_t directory;
                      if ((msg->FindInt64("directory", &directory) == B_NO_ERROR) &&
                          (msg->FindString("name", &name)          == B_NO_ERROR))
                      {
                         NodeCreated(fileNodeRef, entry_ref(fileNodeRef.device, directory, name));
                         StartFileCountRunner();
                      }
                   }
                   break;

                   case B_ENTRY_MOVED:
                   {
                      ino_t fromDirNode, toDirNode;
                      const char * name;
                      if ((msg->FindInt64("from directory", &fromDirNode) == B_NO_ERROR)&&
                          (msg->FindInt64("to directory", &toDirNode)     == B_NO_ERROR)&&
                          (msg->FindString("name", &name)                 == B_NO_ERROR))
                      {
                         node_ref fromDirNodeRef;
                         fromDirNodeRef.device = fileNodeRef.device;  
                         fromDirNodeRef.node   = fromDirNode;

                         node_ref toDirNodeRef;
                         toDirNodeRef.device = fileNodeRef.device;  
                         toDirNodeRef.node   = toDirNode;

                         WatchedDirData * towdd = _watchedDirs.Get(toDirNodeRef);
                         // Check the moved guy... is he a directory?
                         entry_ref movedGuyEntryRef(fileNodeRef.device, toDirNode, name);
                         BEntry movedGuy(&movedGuyEntryRef, true);
                         if (movedGuy.InitCheck() == B_NO_ERROR) // more paranoia
                         {
                            // If it's a file, just remove the old entry and add the new one
                            NodeRemoved(fileNodeRef);
                            if (towdd) NodeCreated(fileNodeRef, entry_ref(toDirNodeRef.device, toDirNodeRef.node, name));
                            StartFileCountRunner();
                         }
                      }
                   }
                   break;

                   case B_ENTRY_REMOVED:
                      NodeRemoved(fileNodeRef);
                      StartFileCountRunner();
                   break;
                }
             }
          }
      }
      break;

      default:
         BHandler::MessageReceived(msg);
      break;
   }
}

void
ShareNetClient::NodeRemoved(const node_ref & node)
{
   BAutolock m(_dirsLock);

   // First see if it's a directory
   if (_watchedDirs.ContainsKey(node))
   	RemoveWatchedDirectory(node);
   else
   {
      entry_ref ref;
      if ((_nodeToEntry.Remove(node, ref) == B_NO_ERROR)&&(_mtt))
      {
         UnrefFilename(ref.name);
         if (_nodeRemoveMessage() == NULL) _nodeRemoveMessage = GetMessageFromPool(PR_COMMAND_REMOVEDATA);
         if (_nodeRemoveMessage())
         {
            String path(GetFilesNodePath());
            path += ref.name;
            EscapeRegexTokens(path);
            _nodeRemoveMessage()->AddString(PR_NAME_KEYS, path());
            if ((_nodeRemoveBatchCount == 0)||(_nodeRemoveMessage()->GetNumValuesInName(PR_NAME_KEYS) > 50)) 
            {
               SendMessageToSessions(_nodeRemoveMessage, false);
               _nodeRemoveMessage.Reset();
            }
         }
      }
   }
}

void
ShareNetClient ::
SetFileSharingEnabled(bool enabled)
{
   AbortScanSharesThread();
   if (enabled) ReuploadFiles(0);
   else 
   {
      RemoveFileSharing();
      UploadSharedFileCount();
   }
}

void 
ShareNetClient ::
NodeCreated(const node_ref & noderef, const entry_ref & entryref)
{
   BAutolock m(_dirsLock);

   // See if we know which of or watched directories this file is in
   BEntry er(&entryref, false);
   BEntry parentEntry;
   BNode parentNode;
   node_ref parentNodeRef;
   WatchedDirData * wdd;
   if ((er.GetParent(&parentEntry)            == B_NO_ERROR)&&
       (parentNode.SetTo(&parentEntry)        == B_NO_ERROR)&&
       (parentNode.GetNodeRef(&parentNodeRef) == B_NO_ERROR)&&
       ((wdd = _watchedDirs.Get(parentNodeRef)) != NULL))
   {
      // Now traverse it if it's a symlink (must do this after getting the parent!)
      if (er.IsSymLink())
      {
          entry_ref tref; 
          if ((er.GetRef(&tref) != B_NO_ERROR)||(er.SetTo(&tref, true) != B_NO_ERROR)) return;
      }

      // First, try to make a BDirectory out of it.  If we can, add its contents instead of the node itself.
      if (er.IsDirectory()) AddWatchedDirectory(noderef, er, NULL, wdd->_path, false);
      else
      {
         // It's a file... add it to our list
         MessageRef value;
         off_t fileSize;
         if (GetFileInfo(value, entryref, noderef, wdd->_path, fileSize) == B_NO_ERROR)
         {
            RefFilename(entryref.name, entryref, fileSize);
            _nodeToEntry.Put(noderef, entryref);
            String nodepath(GetFilesNodePath());
            nodepath += entryref.name;
            SetDataNodeValue(nodepath(), value);
         }
      }
   }
}

void
ShareNetClient ::
MessageReceived(const MessageRef & msgRef)
{
	_lastTrafficAt = system_time();

	Message * msg = msgRef();
//   msg->PrintToStream();
	switch(msg->what) {
		case ShareFileTransfer::TRANSFER_COMMAND_REJECTED:  // sigh
      {
         const char * from;
         if (msg->FindString(PR_NAME_SESSION, &from) == B_NO_ERROR)
         {
            uint64 timeLeft = (uint64)-1;
            (void) msg->FindInt64("timeleft", (int64*)&timeLeft);
            ((ShareWindow*)Looper())->TransferCallbackRejected(from, timeLeft);
         }
      }
      break;

      case PR_RESULT_PONG:  // pong from server indicates our query is done
      {
         if (_sendOnLogins) 
         {
            ((ShareWindow*)Looper())->SendOnLogins();  // all users should be here now...
            _sendOnLogins = false;
         }

         int32 count;
         if ((msg->FindInt32("count", &count) == B_NO_ERROR)&&(count >= _pingCount-1)) ((ShareWindow *)Looper())->SetQueryInProgress(false);
      }
      break;

      // another client is requesting that we say hello to him!
      case NET_CLIENT_PING:
      {
         String replyTo;
         if (msg->FindString("session", replyTo) == B_NO_ERROR)
         {
            msg->what = NET_CLIENT_PONG;

            String toString("/*/"); 
            toString += replyTo;
            toString += "/beshare";

            msg->RemoveName(PR_NAME_KEYS);
            msg->AddString(PR_NAME_KEYS, toString);

            msg->RemoveName("session");
            msg->AddString("session", _localSessionID);

            msg->RemoveName("version");
            msg->AddString("version", VERSION_STRING);

            bigtime_t now = system_time();

            msg->RemoveName("uptime");
            msg->AddInt64("uptime", now);

            msg->RemoveName("onlinetime");
            msg->AddInt64("onlinetime", (_lastLoginTime > 0) ? now-_lastLoginTime : 0);

            SendMessageToSessions(msgRef, true);
         }
      }
      break;

      case NET_CLIENT_PONG:
      {
         ShareWindow * win = (ShareWindow*) Looper();
         const char * session;
         int64 when;
         if ((msg->FindString("session", &session) == B_NO_ERROR)&&
             (msg->FindInt64("when", &when)        == B_NO_ERROR))
         {
            String displayString;
            char temp[512];
            sprintf(temp, str(STR_PING_REPLY_LIMS), (system_time()-when)/1000LL);
            displayString = temp;

            const char * version;
            if (msg->FindString("version", &version) == B_NO_ERROR)
            {
               displayString += " (";
               if ((version[0] >= '0')&&(version[0] <= '9')) displayString += "BeShare v";
               displayString += version;
               displayString += ")";
            }
            
            int64 uptime, onlinetime;
            if ((msg->FindInt64("uptime",     &uptime)     == B_NO_ERROR)&&
                (msg->FindInt64("onlinetime", &onlinetime) == B_NO_ERROR))
            {
               displayString += " (";
               displayString += str(STR_SYSTEM_UPTIME);
               displayString += ": ";
               displayString += win->MakeTimeElapsedString(uptime);
               displayString += ", ";
               displayString += str(STR_LOGGED_IN_FOR);
               displayString += " ";
               displayString += win->MakeTimeElapsedString(onlinetime);
               displayString += ")";
            }

            win->LogMessage(LOG_REMOTE_USER_CHAT_MESSAGE, displayString(), session, &win->GetColor(COLOR_PING), true);
         }
      }
      break;

      case NET_CLIENT_CONNECT_BACK_REQUEST:
      {
         const char * session;
         int32 port;
         if ((_firewalled) &&  // not firewalled?  Then they should just connect to us directly
             (msg->FindString("session", &session) == B_NO_ERROR) &&
             (msg->FindInt32("port", &port)        == B_NO_ERROR)) ((ShareWindow*)Looper())->ConnectBackRequestReceived(session, (uint16)port, msgRef);
      }
      break;
 
      case NET_CLIENT_NEW_CHAT_TEXT:
      {
         // Someone has sent a line of chat text to display
         const char * text;
         const char * session;
         bool isPrivate = msg->HasName("private");
         ShareWindow * win = (ShareWindow *) Looper();
         if ((msg->FindString("text", &text) == B_NO_ERROR)&&(msg->FindString("session", &session) == B_NO_ERROR)) win->LogMessage(LOG_REMOTE_USER_CHAT_MESSAGE, text, session, isPrivate ? &win->GetColor(COLOR_PRIVATE) : NULL, isPrivate);
      }
      break;

		case PR_RESULT_DATAITEMS:
		{
			BSTRACE(("ShareNetClient::MessageReceived PR_RESULT_DATAITEMS\n"));
			// Part of the server-side database that we subscribed to has changed
			((ShareWindow*)Looper())->BeginBatchFileResultUpdate();

			// Look for sub-messages that indicate that nodes were removed from the tree
			String nodepath;
			for (int i = 0; (msg->FindString(PR_NAME_REMOVED_DATAITEMS, i, nodepath) == B_NO_ERROR); i++) {
				int pathDepth = GetPathDepth(nodepath());
            
				if (pathDepth >= SESSION_ID_DEPTH) {
					String sessionID = GetPathClause(SESSION_ID_DEPTH, nodepath());
					sessionID = sessionID.Substring(0, sessionID.IndexOf('/'));

					switch(GetPathDepth(nodepath()))
					{
						case SESSION_ID_DEPTH: 
							((ShareWindow*)Looper())->RemoveUser(sessionID());
						break;

 						case FILE_INFO_DEPTH: 
							((ShareWindow*)Looper())->RemoveResult(sessionID(), GetPathClause(FILE_INFO_DEPTH, nodepath())); 
						break;
					}
				}
			}

			// Look for sub-messages that indicate that nodes were added to the tree
			for (MessageFieldNameIterator iter(msg->GetFieldNameIterator(B_MESSAGE_TYPE)); iter.HasData(); iter++) {
				nodepath = iter.GetFieldName();
				int pathDepth = GetPathDepth(iter.GetFieldName().Cstr());

				if (pathDepth >= USER_NAME_DEPTH) {
					MessageRef tempRef;

					if (msg->FindMessage(nodepath(), tempRef) == B_NO_ERROR) {
						const Message * pmsg = tempRef();
						String sessionID = GetPathClause(SESSION_ID_DEPTH, nodepath());
						sessionID = sessionID.Substring(0, sessionID.IndexOf('/'));
						switch(pathDepth)
						{
							case USER_NAME_DEPTH: 
							{
								String hostName = GetPathClause(HOST_NAME_DEPTH, nodepath());
								hostName = hostName.Substring(0, hostName.IndexOf('/'));

								const char * nodeName = GetPathClause(USER_NAME_DEPTH, nodepath());
								if (strncmp(nodeName, "name", 4) == 0) {

									bool isBot;
									if (pmsg->FindBool("bot", &isBot) != B_NO_ERROR)
										isBot = false;

									uint64 installID;
									if (pmsg->FindInt64("installid", (int64*)&installID) != B_NO_ERROR)
										installID = 0;

									int32 port;
									if (pmsg->FindInt32("port", &port) != B_NO_ERROR)
										port = 0;

									const char* name;
									if (pmsg->FindString("name", &name) == B_NO_ERROR) {
										String clientStr;

										// See if VitViper's version info is available
										const char * vname;
										const char * vnum;
										if ((pmsg->FindString("version_num", &vnum) == B_NO_ERROR)
											&& (pmsg->FindString("version_name", &vname) == B_NO_ERROR)) {
											clientStr = vname;
											clientStr += " v";
											clientStr += vnum;
										}

										// See if this client is advertising that he can to the partial-md5-hash trick
										bool sph = false;
										BSTRACE(("Hittade %s\n", name));
										(void) pmsg->FindBool("supports_partial_hashing", &sph);
										((ShareWindow *)Looper())->PutUser(sessionID(), name, hostName(), port, &isBot, installID, (clientStr.Length()>0)?clientStr():NULL,&sph);
									}
								} else if (strncmp(nodeName, "userstatus", 9) == 0) {
									const char * status;
									if (pmsg->FindString("userstatus", &status) == B_NO_ERROR)
										((ShareWindow *)Looper())->SetUserStatus(sessionID(), status);
								} else if (strncmp(nodeName, "uploadstats", 11) == 0) {
									uint32 cur, max;
									if ((pmsg->FindInt32("cur", (int32*)&cur) == B_NO_ERROR)
										&& (pmsg->FindInt32("max", (int32*)&max) == B_NO_ERROR))
										((ShareWindow *)Looper())->SetUserUploadStats(sessionID(), cur, max);
								} else if (strncmp(nodeName, "bandwidth", 9) == 0) {
									uint32 bps;
									const char * label;
									if ((pmsg->FindString("label", &label) == B_NO_ERROR)
										&& (pmsg->FindInt32("bps", (int32*)&bps) == B_NO_ERROR))
										((ShareWindow *)Looper())->SetUserBandwidth(sessionID(), label, bps);
								} else if (strncmp(nodeName, "filecount", 9) == 0) {
									int32 fc;
									if (pmsg->FindInt32("filecount", &fc) == B_NO_ERROR)
										((ShareWindow *)Looper())->SetUserFileCount(sessionID(), fc);
								} else if (strncmp(nodeName, "fires", 5) == 0) {
									((ShareWindow *)Looper())->SetUserIsFirewalled(sessionID(), true);
								} else if (strncmp(nodeName, "files", 5) == 0) {
									((ShareWindow *)Looper())->SetUserIsFirewalled(sessionID(), false);
								}
							} break;

                     case FILE_INFO_DEPTH: 
                     {
                        const char * fileName = GetPathClause(FILE_INFO_DEPTH, nodepath()); 
                        if ((_queryActive)&&(_sessionIDRegExp.Match(sessionID()))&&(_fileNameRegExp.Match(fileName))) 
                        {
                           MessageRef unpacked = InflateMessage(tempRef);
                           if (unpacked()) ((ShareWindow *)Looper())->PutResult(sessionID(), fileName, (GetPathClause(USER_NAME_DEPTH, nodepath())[2] == 'r'), unpacked);
                        }
                     }
                     break;
                  }
               }
            }
         }

         ((ShareWindow*)Looper())->EndBatchFileResultUpdate();
      }
      break;

      case PR_RESULT_PARAMETERS:
      {
         const char * sessionRoot;
         if (msg->FindString(PR_NAME_SESSION_ROOT, &sessionRoot) == B_NO_ERROR)
         {
            // session root is of form "/hostname/sessionID"; parse these out
            const char * lastSlash = strrchr(sessionRoot, '/');
            if (lastSlash)
            {
               _localSessionID = lastSlash+1;
               _localHostName = String(sessionRoot).Substring(1, (lastSlash-sessionRoot));
            }
         }

         ((ShareWindow*)Looper())->ServerParametersReceived(*msg);
      }
      break;
   }
}




const char * 
ShareNetClient :: GetFilesNodePath() const
{
   return _firewalled ? "beshare/fires/" : "beshare/files/";
}

status_t
ShareNetClient ::
GetDirectory(const node_ref & dirNode, const BEntry & entry, BDirectory & retDir) const
{
   if (retDir.SetTo(&dirNode) == B_NO_ERROR) return B_NO_ERROR;
   else
   {
      node_ref temp;
      return ((entry.GetNodeRef(&temp) == B_NO_ERROR)&&(retDir.SetTo(&temp) == B_NO_ERROR)) ? B_NO_ERROR : B_ERROR;
   }
}

status_t 
ShareNetClient ::
AddWatchedDirectory(const node_ref & dirNode, const BEntry & entry, thread_id * watchThreadID, const String & path, bool diffsOnly)
{
   BAutolock m(_dirsLock);

   node_ref temp;
   BDirectory dir;
   entry_ref er;
   if ((GetDirectory(dirNode, entry, dir)          == B_NO_ERROR) &&
       ((diffsOnly)||(_watchedDirs.ContainsKey(dirNode) == false))&&
       (dir.GetNodeRef(&temp)                      == B_NO_ERROR) &&
       (entry.GetRef(&er)                          == B_NO_ERROR) &&
       (er != _settingsDir)                                       && // new for v1.81--safety feature
       (((diffsOnly)&&(_watchedDirs.ContainsKey(dirNode)))||(watch_node(&temp, B_WATCH_DIRECTORY, this) == B_NO_ERROR)))
   {
      String newPath(path);
      newPath += '/';
      newPath += er.name;

      WatchedDirData wdd(er, newPath);
      _watchedDirs.Put(dirNode, wdd);  // file it under (dirNode), as that's what the node monitor will refer to it by

      node_ref realDirNode;
      if (dir.GetNodeRef(&realDirNode) == B_NO_ERROR) _watchedDirs.Put(realDirNode, wdd);  // file it under its 'real' node too, as this is what is referred to when a sub-entry gets moved into this dir.  What a pain!!
 
      uint32 numEntries = dir.CountEntries();
      if (numEntries > 0)
      {
         MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
         if (uploadMsg())
         {
            // scan directory, and for each file add a sub-message to our setdata message
            thread_id myThreadID = find_thread(NULL);
            int fileCount = 0;
            entry_ref entryRef;
            while(dir.GetNextRef(&entryRef) == B_NO_ERROR) 
            {
               if ((watchThreadID)&&(*watchThreadID != myThreadID)) return B_ERROR;  // forced abort!

               BEntry nextEnt(&entryRef, true);
               if (nextEnt.IsDirectory())
               {
                  node_ref subDirRef;
                  if (nextEnt.GetNodeRef(&subDirRef) == B_NO_ERROR) 
                  {
                     node_ref nosymref;
                     BEntry nosym(&entryRef, false);
                     if (nosym.GetNodeRef(&nosymref) == B_NO_ERROR) (void) AddWatchedDirectory(nosymref, nextEnt, watchThreadID, newPath, diffsOnly);
                  }
               }
               else
               {
                  if ((diffsOnly == false)||(_nameToEntry.ContainsKey(entryRef.name) == false))
                  {
                     BNode node(&entryRef);
                     if (node.InitCheck() == B_NO_ERROR)
                     {
                        node_ref nodeRef;
                        MessageRef nextFileRef;
                        off_t fileSize;
                        if ((node.GetNodeRef(&nodeRef) == B_NO_ERROR)&&(GetFileInfo(nextFileRef, entryRef, nodeRef, newPath, fileSize) == B_NO_ERROR))
                        {
                           RefFilename(entryRef.name, entryRef, fileSize);
                           _nodeToEntry.Put(nodeRef, entryRef);
                           String nextpath(GetFilesNodePath());
      
                           nextpath += entryRef.name;

                           MessageRef packed = DeflateMessage(nextFileRef, 9, true);
                           if (packed())
                           {
                              uploadMsg()->AddMessage(nextpath(), packed);
         
                              // don't let the messages get too big, or we'll constipate the server!
                              if (++fileCount >= 20)
                              {
                                 fileCount = 0;
                                 SendMessageToSessions(uploadMsg, false);
                                 uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
                                 if (uploadMsg() == NULL) 
                                 {
                                    WARN_OUT_OF_MEMORY; 
                                    break;
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
            if (uploadMsg()->GetNumNames() > 0) SendMessageToSessions(uploadMsg, false);
         }
         else WARN_OUT_OF_MEMORY;
      }
      return B_NO_ERROR;
   }
   return B_ERROR;
}

void 
ShareNetClient ::
RemoveWatchedDirectory(const node_ref & dirNode)
{
   BAutolock m(_dirsLock);

   WatchedDirData wdd;
   if (_watchedDirs.Remove(dirNode, wdd) == B_NO_ERROR)
   {
      // Go through and remove any files/dirs under this dir
      BEntry entry(&wdd._entryRef);
      BDirectory dir;
      if (GetDirectory(dirNode, entry, dir) == B_NO_ERROR)
      {
         node_ref realDirNode;
         if (dir.GetNodeRef(&realDirNode) == B_NO_ERROR) _watchedDirs.Remove(realDirNode);

         NodeRemoveBatch(true); 
         BEntry entry;
         while(dir.GetNextEntry(&entry, false) == B_NO_ERROR) 
         {
            node_ref nodeRef;
            if (entry.GetNodeRef(&nodeRef) == B_NO_ERROR) NodeRemoved(nodeRef);
         }
         NodeRemoveBatch(false);
      }
   }
}

void
ShareNetClient :: 
NodeRemoveBatch(bool start)
{
        if (start) _nodeRemoveBatchCount++;
   else if ((--_nodeRemoveBatchCount == 0)&&(_nodeRemoveMessage()))
   {
      SendMessageToSessions(_nodeRemoveMessage, false);
      _nodeRemoveMessage.Reset();
   }
}

status_t
ShareNetClient ::
GetFileInfo(MessageRef & infoMessage, const entry_ref & entryRef, const node_ref & /*nodeRef*/, const String & path, off_t & retFileSize)
{
   BEntry entry(&entryRef, true);
   if (entry.InitCheck() == B_NO_ERROR)
   {
      time_t modTime;
      BFile file(&entryRef, B_READ_ONLY);
      if ((file.InitCheck() == B_NO_ERROR)&&(file.GetSize(&retFileSize) == B_NO_ERROR)&&(file.GetModificationTime(&modTime) == B_NO_ERROR)) 
      {
         infoMessage = GetMessageFromPool();
         if (infoMessage())
         {
            infoMessage()->AddInt64("beshare:File Size", retFileSize);
            infoMessage()->AddInt32("beshare:Modification Time", (int32)modTime);

            // Remove the /shared-level prefix, since that can be assumed
            const char * p = path();
            p = (strncmp(p, "/shared/", 8) == 0) ? (p + 8) : ""; 
            infoMessage()->AddString("beshare:Path", p);
   
            // Now load the attributes for our file's MIME type and add them to our message
            BNodeInfo ni(&file);
            char mimeType[B_ATTR_NAME_LENGTH];
            if ((ni.InitCheck() == B_NO_ERROR)&&(ni.GetType(mimeType) == B_NO_ERROR))
            {
               infoMessage()->AddString("beshare:Kind", mimeType);
               BMimeType mt(mimeType);
               if (mt.InitCheck() == B_NO_ERROR)
               {
                  BMessage attrInfo;
                  if (mt.GetAttrInfo(&attrInfo) == B_NO_ERROR)
                  {
                     const char * attrName;
                     for (int i=0; attrInfo.FindString("attr:name", i, &attrName) == B_NO_ERROR; i++)
                     {
                        type_code tc;
                        if (attrInfo.FindInt32("attr:type", i, (int32 *) &tc) == B_NO_ERROR)
                        {
                           uint8 attrData[256]; 
                           ssize_t ret = file.ReadAttr(attrName, tc, 0L, attrData, sizeof(attrData));
                           if (ret > 0) 
                           {
                              // only store viewable types on the server
                              switch(tc)
                              {
                                 case B_BOOL_TYPE:    case B_DOUBLE_TYPE:  case B_POINTER_TYPE: 
                                 case B_POINT_TYPE:   case B_RECT_TYPE:    case B_FLOAT_TYPE:   
                                 case B_INT64_TYPE:   case B_INT32_TYPE:   case B_INT16_TYPE:   
                                 case B_INT8_TYPE:    case B_STRING_TYPE:  
                                    (void) infoMessage()->AddData(attrName, tc, attrData, ret);
                                 break;
                              }
                           }
                        }
                     }
                  }
               }
            }
            return B_NO_ERROR;
         }
      }
   }
   return B_ERROR;
}


void 
ShareNetClient ::
AddServerSubscription(const char * subscriptionString, bool quietly)
{
   MessageRef queryMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   if (queryMsg())
   {
      queryMsg()->AddBool(subscriptionString, true);  // the true doesn't signify anything
     
      if (quietly) queryMsg()->AddBool(PR_NAME_SUBSCRIBE_QUIETLY, true);  // suppress initial-state response

      // This MessageRef object ensures that (queryMsg) gets recycled after it's sent.
      SendMessageToSessions(queryMsg, true);
   }
   else ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, "AddServerSubscription:  Error, couldn't obtain message; out of memory?");
}


void
ShareNetClient ::
RemoveServerSubscription(const char * pattern)
{
   // Tell the server we're no longer interested in file info.
   MessageRef removeMsg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
   if (removeMsg())
   {
      removeMsg()->AddString(PR_NAME_KEYS, pattern);
      SendMessageToSessions(removeMsg, true);
   }
   else ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, "Error, couldn't obtain message; out of memory?");
}



void
ShareNetClient ::
SetLocalUserName(const char * name)
{
   _localUserName = name;
   if (_mtt)
   {
      MessageRef nameMessage = GetMessageFromPool();
      if (nameMessage())
      {
         nameMessage()->AddString("name", _localUserName());
         nameMessage()->AddInt32("port", _localSharePort);
         nameMessage()->AddInt64("installid", ((ShareWindow*)Looper())->GetInstallID());
         // Identify ourselves!
         nameMessage()->AddString("version_name", "BeShare");
         nameMessage()->AddString("version_num", VERSION_STRING);
         nameMessage()->AddBool("supports_partial_hashing", true);  // as of 2.07, we support this feature
         SetDataNodeValue("beshare/name", nameMessage);

         // Make sure server knows our bandwidth info
         SetUploadBandwidth(_bandwidthLabel(), _bandwidth);
      }
   }
}



void
ShareNetClient ::
SetLocalUserStatus(const char * status)
{
   _localUserStatus = status;
   if (_mtt)
   {
      MessageRef statusMessage = GetMessageFromPool();
      if (statusMessage())
      {
         statusMessage()->AddString("userstatus", _localUserStatus());
         SetDataNodeValue("beshare/userstatus", statusMessage);
      }
   }
}


void
ShareNetClient ::
SendChatMessage(const char * targetSessionID, const char * messageText)
{
   if (_mtt)
   {
      MessageRef chatMessage = GetMessageFromPool(NET_CLIENT_NEW_CHAT_TEXT);
      if (chatMessage())
      {
         String toString("/*/");  // send message to all hosts...
         toString += targetSessionID;     // who have the given sessionID (or "*" == all session IDs)
         toString += "/beshare";          // and are beshare sessions.
         chatMessage()->AddString(PR_NAME_KEYS, toString());

         chatMessage()->AddString("session", _localSessionID());
         chatMessage()->AddString("text", messageText);
         if (strcmp(targetSessionID, "*")) chatMessage()->AddBool("private", true);
         SendMessageToSessions(chatMessage, true);
      }
   }
}

void
ShareNetClient ::
SendPing(const char * targetSessionID)
{
   if (_mtt)
   {
      MessageRef chatMessage = GetMessageFromPool(NET_CLIENT_PING);
      if (chatMessage())
      {
         String toString("/*/");  // send message to all hosts...
         toString += targetSessionID;     // who have the given sessionID (or "*" == all session IDs)
         toString += "/beshare";          // and are beshare sessions.
         chatMessage()->AddString(PR_NAME_KEYS, toString());

         chatMessage()->AddString("session", _localSessionID());
         chatMessage()->AddInt64("when", system_time());
         SendMessageToSessions(chatMessage, true);
      }
   }
}

void
ShareNetClient ::
SetDataNodeValue(const char * nodePath, const MessageRef & nodeValue)
{
   if (_mtt)
   {
      MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
      if (uploadMsg())
      {
         uploadMsg()->AddMessage(nodePath, nodeValue);
         SendMessageToSessions(uploadMsg, true);
      }
   }
}

void
ShareNetClient ::
SendConnectBackRequestMessage(const char * targetSessionID, uint16 port)
{
   if (_mtt)
   {
      MessageRef reqMsg = GetMessageFromPool(NET_CLIENT_CONNECT_BACK_REQUEST);
      if (reqMsg())
      {
         String toString("/*/");  // send message to all hosts...
         toString += targetSessionID;     // who have the given sessionID (or "*" == all session IDs)
         toString += "/beshare";          // and are beshare sessions.
         reqMsg()->AddString(PR_NAME_KEYS, toString);
         reqMsg()->AddString("session", _localSessionID);
         reqMsg()->AddInt32("port", port);
         SendMessageToSessions(reqMsg, true);
      }
   }
}


void
ShareNetClient ::
SetUploadBandwidth(const char * l, uint32 bps)
{
   _bandwidth = bps;
   _bandwidthLabel = l;
   if (_mtt)
   {
      MessageRef msg = GetMessageFromPool();
      if (msg())
      {
         msg()->AddString("label", l);
         msg()->AddInt32("bps", bps);
         SetDataNodeValue("beshare/bandwidth", msg);
      }
   }
}


void
ShareNetClient ::
SetFirewalled(bool f)
{
   if (f != _firewalled)
   {
      _firewalled = f;
      if (((ShareWindow*)Looper())->IsConnected()) ReuploadFiles(0);
   }
}


void
ShareNetClient ::
RemoveFileSharing()
{
   if (_mtt)
   {
      // Remove any pending low-priority data.  Note that this assumes
      // that all low-priority data is SETDATA commands... true for now at least!
      _lowPriorityMessages.Clear();
      
      MessageRef removeNodes = GetMessageFromPool(PR_COMMAND_REMOVEDATA);
      if (removeNodes())
      {
         removeNodes()->AddString(PR_NAME_KEYS, "beshare/fi*es");
         SendMessageToSessions(removeNodes, true);
      }
      else WARN_OUT_OF_MEMORY;
   }

   UnwatchAllDirs();
}

void
ShareNetClient ::
ReuploadFiles(uint32 code)
{
   BeginScanSharesBatch();
   AbortScanSharesThread();

   if ((_mtt)&&(_localSharePort > 0))
   {
      // Remove any of our existing file nodes from the server
      if (code == 0) RemoveFileSharing();

      // Make sure we are watching for volume mounts/unmounts too
      if (_watchingVolumeMounts == false) _watchingVolumeMounts = (watch_node(NULL, B_WATCH_MOUNT, this) == B_NO_ERROR);

      // Now upload the files again... do it in a background thread to avoid holding up the GUI
      bool removes = ((code & RESCAN_REMOVES) != 0); 
      bool adds    = ((code & RESCAN_ADDS)    != 0); 
      _scanSharesThreadID = spawn_thread(adds ? (removes ? CheckBothThreadFunc : CheckAddsThreadFunc) : (removes ? CheckRemovesThreadFunc : ScanSharesThreadFunc), "shares scan thread", B_LOW_PRIORITY, this);
      if (_scanSharesThreadID >= 0) resume_thread(_scanSharesThreadID);
                               else ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, "Couldn't start shares scan thread!");

   }
   EndScanSharesBatch();
}


void
ShareNetClient::ScanSharesThread(bool checkRemoves, bool checkAdds)
{
	// If requested, first go through and see what records we have that no longer are accessible
	if (checkRemoves) {
		NodeRemoveBatch(true);
		BAutolock a(_dirsLock);
		
		for (HashtableIterator<node_ref, entry_ref> iter(_nodeToEntry.GetIterator()); iter.HasData(); iter++) {
			if (BEntry(&iter.GetValue(), true).Exists() == false)
				NodeRemoved(iter.GetKey());
         
			if (_scanSharesThreadID < 0)
				break;   // oops, we're being aborted!
		}
		NodeRemoveBatch(false);
	}

	BMessage report(NET_CLIENT_SCAN_THREAD_REPORT);
	
	if ((checkRemoves == false) || (checkAdds)) {
		node_ref shareNodeRef;
		BEntry shareEntry;
		
		if ((_shareDir.GetNodeRef(&shareNodeRef) == B_NO_ERROR)
			&& (_shareDir.GetEntry(&shareEntry) == B_NO_ERROR)) {
			bool success = (AddWatchedDirectory(shareNodeRef, shareEntry, (thread_id *) &_scanSharesThreadID, "", checkAdds) == B_NO_ERROR);
			
			if (checkAdds == false)
				report.AddBool("success", success);
		}
	}
	Looper()->PostMessage(&report, this);
}


void
ShareNetClient ::
AbortScanSharesThread()
{
   BeginScanSharesBatch();
   thread_id temp = _scanSharesThreadID;
   if (temp >= 0)
   {
      _scanSharesThreadID = -1;
      int32 junk;
      wait_for_thread(temp, &junk);
   }
   EndScanSharesBatch();
}


entry_ref
ShareNetClient::FindSharedFile(const char* fileName) const
{
	BAutolock m((BLocker &) _dirsLock);
	entry_ref ret;
	const CountedEntryRef* ref = _nameToEntry.Get(fileName);
	
	if (ref)
		ret = ref->_ref;
	
	return ret;
}


void 
ShareNetClient::RefFilename(const char * fileName, const entry_ref & ref, off_t fileSize)
{
	CountedEntryRef * r = _nameToEntry.Get(fileName);
	
	if (r)
		r->_count++;
	else
		_nameToEntry.Put(fileName, CountedEntryRef(ref, fileSize));
}


void 
ShareNetClient ::
UnrefFilename(const char * fileName)
{
   CountedEntryRef * ref = _nameToEntry.Get(fileName);
   if ((ref)&&(--ref->_count <= 0)) _nameToEntry.Remove(fileName);
}

void
ShareNetClient ::
SendGetParamsMessage()
{
   SendMessageToSessions(GetMessageFromPool(PR_COMMAND_GETPARAMETERS), true);
}

void
ShareNetClient ::
SetUploadStats(uint32 nr, uint32 max, bool force)
{
   if ((force)||(nr != _lastSentUploadCount)||(max != _lastSentUploadMax))
   {
      _lastSentUploadCount = nr;
      _lastSentUploadMax = max;
      if (_mtt)
      {
         MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
         if (uploadMsg())
         {
            MessageRef data = GetMessageFromPool();
            if (data())
            {
               data()->AddInt32("cur", _lastSentUploadCount);
               data()->AddInt32("max", _lastSentUploadMax);
               uploadMsg()->AddMessage("beshare/uploadstats", data);
               SendMessageToSessions(uploadMsg, true);
            }
            else WARN_OUT_OF_MEMORY;
         }
         else WARN_OUT_OF_MEMORY;
      }
   }
}

off_t 
ShareNetClient :: 
GetSharedFileSize(const String & name) const
{
   const CountedEntryRef * ref = _nameToEntry.Get(name);
   return ref ? ref->_fileSize : -1;
}

};  // end namespace beshare
