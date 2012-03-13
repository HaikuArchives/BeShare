#include "ShareFileTransfer.h"
#include "ShareNetClient.h"
#include "ShareStrings.h"
#include "ShareWindow.h"
#include <app/Roster.h>
#include <kernel/OS.h>
#include <kernel/fs_attr.h>
#include <interface/ListView.h>
#include <interface/Region.h>
#include <storage/Mime.h>
#include <storage/NodeInfo.h>

#include "dataio/TCPSocketDataIO.h"
#include "util/StringTokenizer.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/RateLimitSessionIOPolicy.h"

#include "Colors.h"
#include "ChatWindow.h"
#include "ShareUtils.h"
#include "md5.h"

namespace beshare {

#define MD5_DIGEST_SIZE 16
#define ANIM_RECT_SPACING 16
#define ANIM_RECT_WIDTH   16

#define MAX_TRANSFER_STAMP_QUEUE_SIZE 30
#define OPTIONAL_REFRESH_INTERVAL 100000  // 100ms max update rate

static rgb_color ModifyColor(rgb_color col, int delta)
{
   int vars[3] = {col.red, col.green, col.blue};
   int max = muscleMax(vars[0], muscleMax(vars[1], vars[2])); 
   bool allSame = ((vars[0] == vars[1])&&(vars[1] == vars[2]));
   for (int i=0; i<3; i++) if ((allSame)||(vars[i] != max)) vars[i] += delta;

   col.red   = (uint8) muscleClamp(vars[0], 0, 255);
   col.green = (uint8) muscleClamp(vars[1], 0, 255);
   col.blue  = (uint8) muscleClamp(vars[2], 0, 255);
   return col;
}

// Make sure (path) doesn't try to go up any levels.  If it does, make it blank
static void CheckPath(String & path);
static void CheckPath(String & path)
{
  if ((path()[0] == '/')||(path.StartsWith("../"))||(path.EndsWith("/.."))||(path.IndexOf("/../") >= 0)||(path.Equals(".."))) path = "";
}



/* Abstract base class */
class MD5Looper : public BLooper 
{
public:
   MD5Looper() : BLooper(NULL, B_LOW_PRIORITY)
   {
      // empty
   }

   status_t StartProcessing(int32 replyCode, const MessageRef & msgRef, const BMessenger & replyTo, volatile bool * shutdownFlag)
   {
      _replyCode = replyCode;
      _msgRef    = msgRef;
      _replyTo   = replyTo; 
      _shutdownFlag = shutdownFlag;

      if (Run() >= 0)
      {
         BMessage whatever;
         PostMessage(&whatever);  // just to get us going
         return B_NO_ERROR;
      }
      else return B_ERROR;
   }

   virtual void MessageReceived(BMessage *) = 0;

   void AddEntryRef(const entry_ref & ref, const char * path) {_entryRefs.AddTail(ref); _entryPaths.AddTail(path);}
   MessageRef GetMessageRef() {return _msgRef;}

protected:
   void Reply()
   {
      BMessage reportDone(_replyCode);
      reportDone.AddPointer("from", this);
      (void) _replyTo.SendMessage(&reportDone);
   }

   int32 _replyCode;
   Queue<entry_ref> _entryRefs;
   Queue<String> _entryPaths;
   MessageRef _msgRef;
   BMessenger _replyTo;
   volatile bool * _shutdownFlag;
};


/* Background thread used for reading MD5 hashcodes into our TRANSFER_COMMAND_FILE_LIST method, for sending */
class MD5SendLooper : public MD5Looper
{
public:
   MD5SendLooper(const Hashtable<String, OffsetAndPath> & fileSet, const BDirectory & dir, off_t bytesFromBack) : _fileSet(fileSet), _dir(dir), _bytesFromBack(bytesFromBack) {/* empty */}

   virtual void MessageReceived(BMessage *)
   {
      Message & msg = *_msgRef();

      HashtableIterator<String, OffsetAndPath> iter = _fileSet.GetIterator();
      const String * next;
      while(((_shutdownFlag == NULL)||((*_shutdownFlag) == false))&&((next = iter.GetNextKey()) != NULL))
      {
         const OffsetAndPath * nextValue = iter.GetNextValue();

         msg.AddString("files", next->Cstr());

         BDirectory dir(_dir);
         StringTokenizer tok(nextValue->_path(), "/");
         const char * nextDir;
         while((nextDir = tok.GetNextToken()) != NULL)
         {
            BDirectory subDir(&dir, nextDir);
            if (subDir.InitCheck() == B_NO_ERROR) dir = subDir;
            else
            {
               dir.Unset();  // oops, nevermind;  file doesn't exist
               break;
            }
         }

         BEntry ent(&dir, next->Cstr(), true);
         entry_ref er;
         (void) ent.GetRef(&er);
         AddEntryRef(er, nextValue->_path());
      }

      int numRefs = _entryRefs.GetNumItems();
      for (int i=0; i<numRefs; i++)
      {
         // Add current file length and md5, if any
         off_t fileOffset = 0;  // i.e. autodetect file size for offset
         uint8 digest[MD5_DIGEST_SIZE];
         off_t retBytesHashed = 0;
         (void) HashFileMD5(BEntry(&_entryRefs[i], true), fileOffset, _bytesFromBack, retBytesHashed, digest, _shutdownFlag);
         msg.AddInt64("offsets", fileOffset);
         msg.AddString("beshare:Path", _entryPaths[i]);
         if (_bytesFromBack > 0) msg.AddInt64("numbytes", retBytesHashed);
         msg.AddData("md5", B_RAW_TYPE, digest, (fileOffset > 0) ? sizeof(digest) : 1);
      }
      Reply();
   }

private:
   Hashtable<String, OffsetAndPath> _fileSet;
   BDirectory _dir;
   off_t _bytesFromBack;
};


/* Background thread used for reading MD5 hashcodes from files to verify them against the codes sent by the remote client */
class MD5ReceiveLooper : public MD5Looper
{
public:
   virtual void MessageReceived(BMessage *)
   {
      Message * msg = _msgRef.GetItemPointer();
      int numRefs = _entryRefs.GetNumItems();
      for (int i=0; i<numRefs; i++) 
      {
         // Check to see if the requestor already has the beginning of this file
         // If he does, we can do the old auto-resume-download trick!
         off_t offset = 0LL;
         if ((msg->FindInt64("offsets", i, &offset) == B_NO_ERROR)&&(offset > 0))
         {
            off_t uploadOffset = 0LL;  // default to starting the upload at the beginning of the file
            const uint8 * hisDigest;
            size_t numBytes;
            if ((msg->FindData("md5", B_RAW_TYPE, i, (const void **) &hisDigest, &numBytes) == B_NO_ERROR)&&(numBytes == MD5_DIGEST_SIZE))
            {
               // Okay, the requestor has told us enough so that we can check our local file...
               uint8 myDigest[MD5_DIGEST_SIZE];
               for (size_t h=0; h<ARRAYITEMS(myDigest); h++) myDigest[h] = 'x';  // so I know when it hasn't been overwritten

               off_t onSuccessOffset = offset;  // save this now, as we might change it below, but need it later

               off_t readLength = 0;
               if ((msg->FindInt64("numbytes", i, (int64*)&readLength) == B_NO_ERROR)&&(readLength <= offset))
               {
                  // He's requesting that we only verify the bytes from (offset-readLength) to (offset).
                  // This "partial verify" mode is new for BeShare 2.07, but speeds up verification of
                  // large files a lot.
                  // This is weird looking, but it works... essentially
                  // readLength now gets the value to seek too,
                  // and offset is now the number of bytes to hash.
                  // Just the opposite of the var names, but, oh well
                  off_t temp = readLength;     
                  readLength = offset - readLength;  // readLength is now the seekTo value
                  offset = temp;                     // and offset is now the numBytes value
               }

               off_t retBytesHashed = 0;
               BFile tempFile;
               if ((HashFileMD5(BEntry(&_entryRefs[i], true), offset, readLength, retBytesHashed, myDigest, _shutdownFlag) == B_NO_ERROR)&&
                   (memcmp(hisDigest, myDigest, sizeof(myDigest)) == 0)) uploadOffset = onSuccessOffset;
            }
            msg->ReplaceInt64(false, "offsets", i, uploadOffset);
         }
      }
      Reply();
   }
};

ShareFileTransfer ::
ShareFileTransfer(const BDirectory & fileDir, const char * localSessionID, uint64 remoteInstallID, uint64 partialHashSize, uint32 bandwidthLimit) :
   _dir(fileDir),
   _currentFileSize(-1),
   _currentFileBytesDone(-1),
   _saveLastFileSize(-1),
   _saveLastFileBytesDone(-1),
   _autoRestart(false),
   _mtt(NULL),
   _uploadSession(false),
   _bitmap(NULL),
   _localSessionID(localSessionID),
   _remoteUserName(str(STR_UNKNOWN)),
   _displayRemoteUserName(str(STR_UNKNOWN)),
   _remoteIP(0),
   _isAccepting(false),
   _isAcceptSession(false),
   _isConnecting(false),
   _isConnected(false),
   _isWaitingOnLocal(true),
   _isWaitingOnRemote(false),
   _wasConnected(false),
   _errorOccurred(false),
   _isFinished(false),
   _currentFileIndex(0),
   _origFileSetSize(0),
   _animShift(0.0f),
   _lastTransferTime(system_time()),
   _shutdownFlag(false),
   _beginTransferEnabled(true),
   _uploadWaitingForSendToFinish(false),
   _mungeMode(MUNGE_MODE_NONE),  // by default, we don't do munging.  Only if the other guy asks for it.
   _remoteInstallID(remoteInstallID),
   _partialHashSize(partialHashSize),
   _bandwidthLimit(bandwidthLimit),
   _lastRefreshTime(0),
   _banEndTime(0)
{
   // empty
}

ShareFileTransfer ::
~ShareFileTransfer()
{
   _shutdownFlag = true;  // get all child loopers to stop immediately

   // Clean up any MD5 loopers still in progress
   HashtableIterator<MD5Looper *, bool> iter = _md5Loopers.GetIterator();
   MD5Looper * next;
   while(iter.GetNextKey(next) == B_NO_ERROR) if (next->Lock()) next->Quit();

   if (_mtt) 
   {
      _mtt->ShutdownInternalThread();
      delete _mtt;
   }
}

void ShareFileTransfer :: SaveToArchive(BMessage & archive) const
{
   archive.AddInt64("lastfilesize", _saveLastFileSize);
   archive.AddInt64("lastfilebytesdone", _saveLastFileBytesDone);

   archive.AddInt32("bandwidthlimit", _bandwidthLimit);
   archive.AddInt64("remoteinstallid", (int64)_remoteInstallID);
   archive.AddInt64("partialhashsize", (int64)_partialHashSize);
   archive.AddString("remoteusername", _remoteUserName());
   archive.AddBool("acceptsession", _isAcceptSession);

   HashtableIterator<String, OffsetAndPath> iter = _origFileSet.GetIterator();
   const String * nextKey;
   const OffsetAndPath * nextValue;
   while((nextKey = iter.GetNextKey())&&(nextValue = iter.GetNextValue()))
   {
      archive.AddString("file", nextKey->Cstr());
      BMessage oapMsg;
      nextValue->SaveToArchive(oapMsg);
      archive.AddMessage("oap", &oapMsg);
   }
}

void ShareFileTransfer :: SetFromArchive(const BMessage & archive)
{
   _origFileSet.Clear();
   _errorOccurred = true;

   if (archive.FindInt64("lastfilesize",      &_saveLastFileSize)       != B_NO_ERROR) _saveLastFileSize  = -1;
   if (archive.FindInt64("lastfilebytesdone", &_saveLastFileBytesDone)  != B_NO_ERROR) _saveLastFileBytesDone = -1;
   if (archive.FindInt32("bandwidthlimit",   (int32*)&_bandwidthLimit)  != B_NO_ERROR) _bandwidthLimit    = 0;
   if (archive.FindBool("acceptsession",     &_isAcceptSession)         != B_NO_ERROR) _isAcceptSession   = false;
   if (archive.FindInt64("partialhashsize",  (int64*)&_partialHashSize) != B_NO_ERROR) _partialHashSize   = 0;
   if (archive.FindInt64("remoteinstallid",  (int64*)&_remoteInstallID) != B_NO_ERROR) _remoteInstallID   = 0;
   const char * temp;
   _remoteUserName = (archive.FindString("remoteusername", &temp) == B_NO_ERROR) ? temp : str(STR_UNKNOWN);
   _displayRemoteUserName = SubstituteLabelledURLs(_remoteUserName).Trim();

   const char * nextFile;
   for (int i=0; archive.FindString("file", i, &nextFile) == B_NO_ERROR; i++)
   {
      BMessage msg;
      if (archive.FindMessage("oap", i, &msg) == B_NO_ERROR)
      {
         OffsetAndPath oap;
         oap.SetFromArchive(msg);
         _origFileSet.Put(nextFile, oap);
      }
   }
   _displayFileSet = _origFileSet;
}

void
ShareFileTransfer ::
SetBandwidthLimit(uint32 limit)
{
   if (_bandwidthLimit != limit)
   {
      _bandwidthLimit = limit;
      if (_mtt)
      {
         PolicyRef pref;
         if (_bandwidthLimit > 0) pref.SetRef(new RateLimitSessionIOPolicy(_bandwidthLimit));
         if (_uploadSession) _mtt->SetNewOutputPolicy(pref);
                        else _mtt->SetNewInputPolicy(pref);
      }
   }
}

void 
ShareFileTransfer::
AddRequestedFileName(const char * fileName, off_t optStartByte, const char * path, BPoint *point)
{
   String s(fileName);
   OffsetAndPath oap(optStartByte, path);
   _origFileSet.Put(s, oap);
   _displayFileSet.Put(s, oap);
   _fileSet.Put(s, oap);
   if (point) _pointSet.Put(s, *point);
}

void
ShareFileTransfer ::
UpdateRemoteUserName() 
{
   const char * name = ((ShareWindow*)Looper())->GetUserNameBySessionID(_remoteSessionID());
   if (name) 
   {
      _remoteUserName = name;
      _displayRemoteUserName = SubstituteLabelledURLs(_remoteUserName).Trim();
   }
}

status_t 
ShareFileTransfer::
InitConnectSession(const char * hostName, uint16 port, uint32 remoteIP, const char * remoteSessionID)
{
   _banEndTime = 0;
   _isAcceptSession = false;
   _remoteHostName = hostName;
   _remotePort = port;
   _remoteIP = remoteIP;
   _remoteSessionID = remoteSessionID;
   UpdateRemoteUserName();
   _origFileSetSize = _origFileSet.GetNumItems();
   _fileSet = _origFileSet;
   _fileSetIter = _fileSet.GetIterator();
   _currentFileBytesDone = _currentFileSize = -1;
   _transferStamps.Clear();
   _uploadSession = (_origFileSetSize == 0);
   if (_uploadSession) ((ShareWindow*)Looper())->DoBeep(SYSTEM_SOUND_UPLOAD_STARTED);

   BMessenger toMe(this);
   if (_mtt)
   {
      _mtt->ShutdownInternalThread();
      delete _mtt;  // just in case there is an old one around for some reason
   }
   _mtt = new BMessageTransceiverThread(toMe);
   return B_NO_ERROR;
}

status_t
ShareFileTransfer ::
InitAcceptSession(const char * remoteSessionID)
{
   _banEndTime = 0;
   _isAccepting = _isAcceptSession = true;
   _remoteSessionID = remoteSessionID;
   UpdateRemoteUserName();
   _origFileSetSize = _origFileSet.GetNumItems();
   _fileSet = _origFileSet;
   _fileSetIter = _fileSet.GetIterator();
   _currentFileBytesDone = _currentFileSize = -1;
   _transferStamps.Clear();
   _uploadSession = (_origFileSetSize == 0);
   if (_uploadSession) ((ShareWindow*)Looper())->DoBeep(SYSTEM_SOUND_UPLOAD_STARTED);

   BMessenger toMe(this);
   if (_mtt)
   {
      _mtt->ShutdownInternalThread();
      delete _mtt;  // just in case there is an old one around for some reason
   }
   _mtt = new BMessageTransceiverThread(toMe);

   // First try the "standard" ports (to satisfy those firewall-configging guys),
   // if none of those are available, we'll use any old port.
   ReflectSessionFactoryRef factoryRef(new ThreadWorkerSessionFactory());
   status_t ret = B_ERROR;
   for (int i=DEFAULT_LISTEN_PORT; i<=DEFAULT_LISTEN_PORT+LISTEN_PORT_RANGE; i++)
   {
      if ((ret = _mtt->PutAcceptFactory(((i<DEFAULT_LISTEN_PORT+LISTEN_PORT_RANGE)?i:0), factoryRef)) == B_NO_ERROR)
      {
         _acceptingOn = factoryRef()->GetPort();
         ret = _mtt->StartInternalThread();
         break;
      }
   }

   if (ret == B_NO_ERROR) _isAccepting = true;
   UpdateTransferTime();  // enable inactivity timeout when waiting for a connection
   return ret;
}


status_t 
ShareFileTransfer::
InitSocketUploadSession(int socket, uint32 remoteIP, bool sendQueuedNotify)
{
   _isAcceptSession = false;
   _remoteIP = remoteIP;

   _isConnected = _uploadSession = true;
   _banEndTime = 0;

   ((ShareWindow*)Looper())->DoBeep(SYSTEM_SOUND_UPLOAD_STARTED);
   _origFileSetSize = -1;  // we'll know this when we get the TRANSFER_COMMAND_FILE_LIST message

   BMessenger toMe(this);

   if (_mtt)
   {
      _mtt->ShutdownInternalThread();
      delete _mtt;  // just in case there is an old one around for some reason
   }
   _mtt = new BMessageTransceiverThread(toMe);
   status_t ret = (_mtt->AddNewSession(socket, GetTransferSessionRef()) == B_NO_ERROR) ? _mtt->StartInternalThread() : B_ERROR;
   if ((ret == B_NO_ERROR)&&(sendQueuedNotify)) SendNotifyQueuedMessage();
   return ret;
}

AbstractReflectSessionRef
ShareFileTransfer :: GetTransferSessionRef()
{
   AbstractReflectSessionRef workerRef;
   if (_bandwidthLimit > 0)
   {
      workerRef.SetRef(new ThreadWorkerSession);
      PolicyRef pref(new RateLimitSessionIOPolicy(_bandwidthLimit));
      if (_uploadSession) workerRef()->SetOutputPolicy(pref);
                     else workerRef()->SetInputPolicy(pref);
   }
   return workerRef; 
}

void
ShareFileTransfer ::
SendNotifyQueuedMessage()
{
   MessageRef notifyMsg = GetMessageFromPool(TRANSFER_COMMAND_NOTIFY_QUEUED);
   if (notifyMsg())
   {
      if (_mtt) _mtt->SendMessageToSessions(notifyMsg);
      ResetTransferTime(); // don't timeout if he's queued, in this case it's okay that the connection is idle
   }
}


void 
ShareFileTransfer::
MessageReceived(BMessage * msg)
{
   switch(msg->what)
   {
      case TRANSFER_COMMAND_MD5_RECV_READ_DONE:
      {
         UpdateTransferTime();  // now the clock starts ticking again...
         MD5Looper * looper;
         if ((msg->FindPointer("from", (void **) &looper) == B_NO_ERROR)&&(_md5Loopers.Remove(looper) == B_NO_ERROR))
         {
            MessageRef msgRef;
            msgRef = looper->GetMessageRef();
            if (looper->Lock()) looper->Quit();

            Message * pmsg = msgRef();
            const char * nextFile; 
            for (int i=0; (pmsg->FindString("files", i, &nextFile) == B_NO_ERROR); i++)
            {
               String nextPath;
               if (pmsg->FindString("beshare:Path", i, nextPath) == B_NO_ERROR) CheckPath(nextPath); 

               uint64 offset = 0LL;
               (void) pmsg->FindInt64("offsets", i, (int64*) &offset);
               AddRequestedFileName(nextFile, offset, nextPath(), NULL);
            }

            int numFiles = _fileSet.GetNumItems();
            if (numFiles > 0) 
            {
               _origFileSetSize = numFiles;
               _fileSetIter = _fileSet.GetIterator();
               ((ShareWindow*)Looper())->RefreshTransferItem(this);
               ((ShareWindow*)Looper())->DequeueTransferSessions();   
               DoUpload();
            }
            else AbortSession(true);   // he's gotta ask for something, otherwise why did he connect?
         }
      }
      break;

      case TRANSFER_COMMAND_MD5_SEND_READ_DONE:
      {
         UpdateTransferTime();  // now the clock starts ticking again...
         MD5Looper * looper;
         if ((msg->FindPointer("from", (void **) &looper) == B_NO_ERROR)&&(_md5Loopers.Remove(looper) == B_NO_ERROR))
         {
            MessageRef msgRef;
            msgRef = looper->GetMessageRef();
            if (looper->Lock()) looper->Quit();
            if (_mtt) _mtt->SendMessageToSessions(msgRef);
            ((ShareWindow*)Looper())->RefreshTransferItem(this);
            ((ShareWindow*)Looper())->DequeueTransferSessions();    // in case the resume changed our sort position
         }
      }
      break;

      case MUSCLE_THREAD_SIGNAL:
      {
         if (_mtt)
         {
            uint32 code;
            MessageRef next;
            while((_mtt)&&(_mtt->GetNextEventFromInternalThread(code, &next) >= 0))
            {
               switch(code)
               {
                  case MTT_EVENT_SESSION_ACCEPTED:
                     _mtt->RemoveAcceptFactory(0);  // now that we have our connection, no need for the factory!
                     // fall thru!
                  case MTT_EVENT_SESSION_CONNECTED:
                  {
                     _isConnecting = false;
                     _isConnected = _wasConnected = true;
                     if (_mtt)  // paranoia
                     {
                        if (_uploadSession)
                        {
                           if (_isWaitingOnLocal) SendNotifyQueuedMessage();
                        }
                        else
                        {
                           // Tell the remote BeShare connection who we are (this is redundant with the
                           // info from the FILE_LIST message, but since the FILE_LIST message can take
                           // a fairly long time to generate, this will let the remote user know who
                           // we are in the meantime)
                           MessageRef idRef = GetMessageFromPool(TRANSFER_COMMAND_PEER_ID);
                           if (idRef())
                           {
                              idRef()->AddString("beshare:FromSession", _localSessionID);
                              String localUserName; ((ShareWindow *)Looper())->GetLocalUserName(localUserName);
                              idRef()->AddString("beshare:FromUserName", localUserName);
                              _mtt->SendMessageToSessions(idRef);
                           }

                           // Tell the remote BeShare connection what files we want from him
                           MessageRef fileRequest = GetMessageFromPool(TRANSFER_COMMAND_FILE_LIST);
                           if (fileRequest())
                           {
                              // yes, here too!  (For backwards compatibility)
                              fileRequest()->AddString("beshare:FromSession", _localSessionID);

                              String localUserName; ((ShareWindow *)Looper())->GetLocalUserName(localUserName);
                              fileRequest()->AddString("beshare:FromUserName", localUserName);
                              fileRequest()->AddInt32("mm", MUNGE_MODE_XOR);  // let him know we prefer XOR munging, if he can do that

                              // Reading files and calculating md5 hashes could take a long time if there are lots of files, 
                              // or the files are big, so I'll do it all in a background thread instead.
                              MD5Looper * md5Looper = new MD5SendLooper(_fileSet, _dir, _partialHashSize);
                              if (md5Looper->StartProcessing(TRANSFER_COMMAND_MD5_SEND_READ_DONE, fileRequest, BMessenger(this), &_shutdownFlag) == B_NO_ERROR) 
                              {
                                 _md5Loopers.Put(md5Looper, true);
                                 ResetTransferTime();  // don't kill us due to inactivity while we're checksumming!
                                 ((ShareWindow*)Looper())->RefreshTransferItem(this);
                              }
                              else 
                              {
                                 delete md5Looper;  // ???
                                 AbortSession(true);
                              }
                           }
                           else AbortSession(true);
                        }
                     }
                     ((ShareWindow*)Looper())->FileTransferConnected(this);
                  }
                  break;

                  case MTT_EVENT_SESSION_DISCONNECTED:
                  {
                     _currentFile.Unset();  // to free up the file descriptor
                     _shutdownFlag = true;  // no sense reading any more md5 data if we're no longer connected!
                     _isFinished = true;
                     _isConnecting = _isConnected = _isAccepting = _isWaitingOnLocal = _isWaitingOnRemote = false;
                     if ((_currentFileBytesDone < _currentFileSize)||(_currentFileIndex < _origFileSetSize)) _errorOccurred = true;
                     else 
                     {
                        _autoRestart = false; // no need to restart, we're done
                        if (_uploadSession == false) ((ShareWindow*)Looper())->DoBeep(SYSTEM_SOUND_DOWNLOAD_FINISHED);
                     }

                     if (_autoRestart == false) ((ShareWindow*)Looper())->FileTransferDisconnected(this);
                     if (_mtt)
                     {
                        _mtt->ShutdownInternalThread();
                        delete _mtt;  // might as well get rid of it, it's useless now
                        _mtt = NULL;
                     }
                     ResetTransferTime();
               
                     if (_autoRestart)
                     {
                        _autoRestart = false;
                        RestartSession();
                        ((ShareWindow*)Looper())->DequeueTransferSessions();   
                     }
                  }
                  break;

                  case MTT_EVENT_INCOMING_MESSAGE:
                     MessageReceived(next);
                  break;

                  case MTT_EVENT_OUTPUT_QUEUES_DRAINED:
                     if (_uploadSession) 
                     {
                        if (_uploadWaitingForSendToFinish) 
                        {
                           AbortSession(false);
                           ((ShareWindow*)Looper())->DoBeep(SYSTEM_SOUND_UPLOAD_FINISHED);
                        }
                        else DoUpload();
                     }
                  break;
               }
            }
         }
      }
      break;            
   }
}

uint32
ShareFileTransfer ::
CalculateChecksum(const uint8 * data, size_t bufSize) const
{
   uint32 sum = 0L;
   for (size_t i=0; i<bufSize; i++) sum += (*(data++)<<(i%24));
   return sum;
}

void
ShareFileTransfer :: DoUpload()
{
   // This silly hack is done because the previous solution,
   // wherein DoUpload() would recurse, could cause DoUpload()
   // to overload the stack frame and crash under some circumstances.
   // Doing it this way ensures the stack won't fill up.
   while(DoUploadAux()) {/* empty */}
}

bool
ShareFileTransfer ::
DoUploadAux()
{
   if (_isWaitingOnLocal) return false;   // not yet!

   if (_mtt)
   {
      if (_currentFileBytesDone >= 0)
      {
         MessageRef msg = GetMessageFromPool(TRANSFER_COMMAND_FILE_DATA);
         const size_t bufferSize = 8 * 1024;  // 8k buffers chosen to minimize overhead while keeping a nice GUI update rate
         uint8 * scratchBuffer;

         if ((msg())&&(msg()->AddData("data", B_RAW_TYPE, NULL, bufferSize) == B_NO_ERROR)&&
                      (msg()->FindDataPointer("data", B_RAW_TYPE, (void **) &scratchBuffer, NULL) == B_NO_ERROR))
         {
            UpdateTransferTime();  // postpone the moribund-connection-reaper, since we're uploading some now
            ssize_t numBytes = _currentFile.Read(scratchBuffer, bufferSize);
            if (numBytes > 0) 
            {
               // Optional data-munging to fool censorious routers and such
               if (_mungeMode != MUNGE_MODE_NONE)
               {
                  bool unknownMungeMode = false;  // optimistic default
                  switch(_mungeMode)
                  {
                     case MUNGE_MODE_XOR:
                        for (ssize_t x=0; x<numBytes; x++) scratchBuffer[x] ^= 0xFF;
                     break;
  
                     default:  
                        unknownMungeMode = true;  // oops, we don't how to do that that munge.  We'll leave the data as-is.
                     break;
                  }
                  if (unknownMungeMode == false) msg()->AddInt32("mm", _mungeMode);  // so remote peer will know how we're encoding the data
               }

               msg()->AddInt32("chk", CalculateChecksum(scratchBuffer, numBytes));  // a little paranoioa, due to file-resumes not working.... (TCP should handle this BUT...)

               if ((size_t)numBytes < bufferSize)
               {
                  // oops!  Better chop the extra bytes out of the field!
                  msg()->AddData("temp", B_RAW_TYPE, scratchBuffer, numBytes);  // copy just the valid bytes
                  msg()->Rename("temp", "data");                                // and replace the old buffer
               }

               _transferStamps.AddTail(TransferStamp(system_time(), numBytes));
               if (_transferStamps.GetNumItems() > MAX_TRANSFER_STAMP_QUEUE_SIZE) _transferStamps.RemoveHead();

               _mtt->SendMessageToSessions(msg);
               _mtt->RequestOutputQueuesDrainedNotification(MessageRef());  // so we will know when it has been sent
               _currentFileBytesDone += numBytes;
               _saveLastFileBytesDone = _currentFileBytesDone;
               ((ShareWindow*)Looper())->AddToTransferCounts(true, (uint32) numBytes);
               if (OnceEvery(OPTIONAL_REFRESH_INTERVAL, _lastRefreshTime)) ((ShareWindow *)Looper())->RefreshTransferItem(this);
               return false;  
            }

            if (numBytes == 0)
            {
               // move to the next file
               _currentFileBytesDone = _currentFileSize = -1;
               return true;
            }
         }
         else
         {
            WARN_OUT_OF_MEMORY;
            AbortSession(true);
            return false;
         }
      }
      else 
      {
         String userString(str(STR_USER_NUMBER));
         userString += _remoteSessionID;
         if ((_remoteUserName.Length() > 0)&&(_remoteUserName[0]))
         {
            userString += str(STR_AKA);
            userString += _remoteUserName;
            userString += ")";
         }

         if (_currentFileName.Length() > 0)
         {
            String finishedDownloading = userString;
            finishedDownloading += str(STR_HAS_FINISHED_DOWNLOADING);
            finishedDownloading += _currentFileName;
            ((ShareWindow*)Looper())->LogMessage(LOG_UPLOAD_EVENT_MESSAGE, finishedDownloading());
         }

         // Get the next file from our send list
         if (_fileSetIter.GetNextKey(_currentFileName) == B_NO_ERROR)
         {
            const OffsetAndPath * value = _fileSetIter.GetNextValue();
            _saveLastFileBytesDone = _currentFileBytesDone = value->_offset;  // get the starting byte to read from...
            _currentFileEntry = ((ShareWindow*)Looper())->FindSharedFile(_currentFileName());
            if (_currentFile.SetTo(&_currentFileEntry, B_READ_ONLY) == B_NO_ERROR)
            {
               MessageRef header = GetMessageFromPool(TRANSFER_COMMAND_FILE_HEADER);
               if ((header())&&
                   (_currentFile.GetSize(&_currentFileSize) == B_NO_ERROR) &&
                   (_currentFile.Seek(_currentFileBytesDone, SEEK_SET) == _currentFileBytesDone))
               {
                  _saveLastFileSize = _currentFileSize;
                  _currentFileIndex++;

                  String isDownloading = userString;
                  isDownloading += str(STR_IS_DOWNLOADING);
                  isDownloading += _currentFileName;
                  ((ShareWindow*)Looper())->LogMessage(LOG_UPLOAD_EVENT_MESSAGE, isDownloading());

                  header()->AddString("beshare:File Name", _currentFileName());
                  header()->AddInt64("beshare:File Size", _currentFileSize);
                  header()->AddString("beshare:FromSession", _localSessionID);
                  header()->AddString("beshare:Path", value->_path);
                  if (_currentFileBytesDone > 0LL) header()->AddInt64("beshare:StartOffset", _currentFileBytesDone);  // resume-download mode!

                  char attrName[B_ATTR_NAME_LENGTH];
                  BNodeInfo ni(&_currentFile);
                  if ((ni.InitCheck() == B_NO_ERROR)&&(ni.GetType(attrName) == B_NO_ERROR))
                  {
                     header()->AddString("beshare:Kind", attrName);
                     _bitmap = ((ShareWindow*)Looper())->GetBitmap(attrName);
                  }
                  else _bitmap = ((ShareWindow*)Looper())->GetBitmap(NULL); 

                  while(_currentFile.GetNextAttrName(attrName) == B_NO_ERROR)
                  {
                     struct attr_info attrInfo;
                     if ((_currentFile.GetAttrInfo(attrName, &attrInfo) == B_NO_ERROR)&&(attrInfo.size > 0))
                     {
                        char * attrData = new char[attrInfo.size];
                        if (_currentFile.ReadAttr(attrName, attrInfo.type, 0L, attrData, attrInfo.size) == attrInfo.size) (void) header()->AddData(attrName, attrInfo.type, attrData, attrInfo.size);
                        delete [] attrData;
                     }
                  }

                  ((ShareWindow*)Looper())->RefreshTransferItem(this);

                  _mtt->SendMessageToSessions(header);
               }
               return true;
            }
         }
         else 
         {
            _uploadWaitingForSendToFinish = true;
            _mtt->RequestOutputQueuesDrainedNotification(MessageRef());  // so we will know when it has been sent
         }
         return false;
      }
   }

   // If we got here, there was an error
   AbortSession(true);
   return false;
}


static const uint64 BESHARE_UNKNOWN_BAN_TIME_LEFT = (uint64)-2;

void 
ShareFileTransfer ::
MessageReceived(const MessageRef & msgRef)
{
   Message * msg = msgRef.GetItemPointer();
   UpdateTransferTime();  // postpone the reaper, since we've got some activity
   switch(msg->what)
   {
      case TRANSFER_COMMAND_REJECTED:
      {
         uint64 timeLeft = BESHARE_UNKNOWN_BAN_TIME_LEFT;  // for backwards compatibility
         (void) msg->FindInt64("timeleft", (int64*)&timeLeft);
         TransferCallbackRejected(timeLeft);
      }
      break;

      case TRANSFER_COMMAND_NOTIFY_QUEUED:
         if (_uploadSession == false) 
         {
            _isWaitingOnRemote = true;
            _transferStamps.Clear();
            ((ShareWindow *)Looper())->RefreshTransferItem(this);
            ResetTransferTime(); // If we're queued, don't timeout, as we may not be getting data for a long time anyway
         }
      break;

      // Received from 2.07+ clients first, to establish who they are right away so we don't have to wait for long md5 hashes
      case TRANSFER_COMMAND_PEER_ID:
      {
         if (msg->FindString("beshare:FromUserName", _remoteUserName) == B_NO_ERROR) _displayRemoteUserName = SubstituteLabelledURLs(_remoteUserName).Trim();
         if (msg->FindString("beshare:FromSession", _remoteSessionID) == B_NO_ERROR) UpdateRemoteUserName();
         ((ShareWindow*)Looper())->RefreshTransferItem(this);
      }
      break;

      // Received when the remote peer wishes to tell us what files he wants to download from us
      case TRANSFER_COMMAND_FILE_LIST:
      {
         const char * file;
         if ((_uploadSession)&&(msg->FindString("files", &file) == B_NO_ERROR))  // make sure he's asking for at least one file
         {
            // Find out what his munging preference is (if any)
            int32 mm;
            if (msg->FindInt32("mm", &mm) == B_NO_ERROR) _mungeMode = mm;

            // This is used just as a backup, in case the session ID wasn't sent, or is unknown
            if (msg->FindString("beshare:FromUserName", _remoteUserName) == B_NO_ERROR) _displayRemoteUserName = SubstituteLabelledURLs(_remoteUserName).Trim();

            if (msg->FindString("beshare:FromSession", _remoteSessionID) == B_NO_ERROR) UpdateRemoteUserName();

            // If we are currently scanning files, then our file list may not be complete yet;  in that case
            // we will defer the processing of this Message until the scan has finished!
            _saveFileListMessage = msgRef;
            if (((ShareWindow *)Looper())->IsScanningShares()) SendNotifyQueuedMessage();  // so they know they are waiting for us to get done...
                                                          else SharesScanComplete();

            // in case our session ID allows us to start...
            ((ShareWindow*)Looper())->DequeueTransferSessions();
         }
         else AbortSession(true);  // you're going the wrong way buddy!
      }
      break;      

      // When downloading, this indicates the start of the next incoming file
      case TRANSFER_COMMAND_FILE_HEADER:
      {
         String prevFile(_currentFileName);
         String old(_remoteSessionID);
         if ((_uploadSession == false)                                             &&
             (msg->FindString("beshare:File Name", _currentFileName) == B_NO_ERROR)&&
             (msg->FindInt64("beshare:File Size", &_currentFileSize) == B_NO_ERROR)&&
             (msg->FindString("beshare:FromSession", _remoteSessionID) == B_NO_ERROR)&&
             (_fileSet.Remove(_currentFileName) == B_NO_ERROR))  // note that while downloading we ignore the value of this hashtable entry
         {
            _saveLastFileSize = _currentFileSize;

            // so we won't try to get it again on restart...
            (void) _origFileSet.Remove(prevFile);

            off_t startByte = 0LL;

            BDirectory dlDir(_dir);
            {
               String path;
               if ((((ShareWindow*)Looper())->GetRetainFilePaths())&&(msg->FindString("beshare:Path", path) == B_NO_ERROR))
               {
                  CheckPath(path);
                  StringTokenizer tok(path(), "/");
                  const char * nextPart;
                  while((nextPart = tok.GetNextToken()) != NULL)
                  {
                     BDirectory subDir(&dlDir, nextPart);
                     if (subDir.InitCheck() == B_NO_ERROR) dlDir = subDir;
                     else
                     {
                        if (dlDir.CreateDirectory(nextPart, &subDir) == B_NO_ERROR) dlDir = subDir;
                                                                               else printf("Error creating subdirectory [%s]!\n", nextPart);
                     }
                  }
               }
            }

            // Using a BEntry to support symlinks in the downloads dir (apparently some people want this? ;^))
            BEntry existingFileEntry(&dlDir, _currentFileName(), true);

            bool append = false;
            if ((msg->FindInt64("beshare:StartOffset", &startByte) == B_NO_ERROR)&&(startByte > 0LL))
            {
               // Double check:  Do we have a file named _currentFileName that is exactly the right size?
               // If so, go to append mode; if not, fall back to the old way
               off_t curFileSize;
               if ((_currentFile.SetTo(&existingFileEntry, B_READ_WRITE) == B_NO_ERROR)&&
                   (_currentFile.GetSize(&curFileSize) == B_NO_ERROR)&&(curFileSize == startByte)) append = true;
            }

            if (((append)||(GenerateNewFilename(dlDir, existingFileEntry, 15) == B_NO_ERROR))&&
                (_currentFile.SetTo(&existingFileEntry, B_WRITE_ONLY|B_CREATE_FILE|(append?B_OPEN_AT_END:0)) == B_NO_ERROR))
            {
               // First thing we need to do after creating the file is set its poseinfo-attribute,
               // otherwise Tracker won't show it in the right location
               struct {
                  bool hidden;
                  char padding[3];
                  ino_t directory;
                  BPoint point;
               } pinfo;
               pinfo.hidden = false;
               node_ref noderef;
               dlDir.GetNodeRef(&noderef);
               pinfo.directory = noderef.node;
               if (_pointSet.Remove(_currentFileName, pinfo.point) == B_NO_ERROR)
               {
                  // Store the position info, unless the attribute already exists
                  BNode node(&existingFileEntry);
                  attr_info attrinfo;
                  const char *attrname = B_HOST_IS_LENDIAN?"_trk/pinfo_le":"_trk/pinfo";
                  if (node.GetAttrInfo(attrname, &attrinfo) != B_NO_ERROR)
                  {
                     // Does not exist yet, write it. NOTE: this code does not support
                     // big-endian bfs mounted on x86, or little-endian bfs mounted on PPC
                     (void) node.WriteAttr(attrname, B_RAW_TYPE, 0, &pinfo, sizeof(pinfo));
                  }
               }
               
               // Make sure our display, etc, all reflect the generated file name
               char buf[B_FILE_NAME_LENGTH];
               existingFileEntry.GetName(buf);
               _currentFileName = buf;
               (void) existingFileEntry.GetRef(&_currentFileEntry);

               _isWaitingOnRemote = false;  // if we're getting data, we must not be waiting anymore!
               UpdateRemoteUserName();
               _currentFileIndex++;
               _saveLastFileBytesDone = _currentFileBytesDone = startByte;
               MessageFieldNameIterator iter = msg->GetFieldNameIterator();
               String fieldName;
               while(iter.GetNextFieldName(fieldName) == B_NO_ERROR)
               {
                  if (strncmp(fieldName(), "beshare:", 8))  // don't save our attributes, they aren't necessary
                  {
                     const void * attrData;
                     size_t attrSize;
                     uint32 c;
                     type_code type;
                     if ((msg->GetInfo(fieldName(), &type, &c)                   == B_NO_ERROR)&&
                         (msg->FindData(fieldName(), type, &attrData, &attrSize) == B_NO_ERROR))
                     {
                        (void)_currentFile.WriteAttr(fieldName(), type, 0, attrData, attrSize);
                     }
                  }
               }

               BNodeInfo ni(&_currentFile);
               const char * mimeString;
               if ((ni.InitCheck() == B_NO_ERROR)&&(msg->FindString("beshare:Kind", &mimeString) == B_NO_ERROR))
               {
                  ni.SetType(mimeString);
                  _bitmap = ((ShareWindow*)Looper())->GetBitmap(mimeString);
               }
               else _bitmap = ((ShareWindow*)Looper())->GetBitmap(NULL);

               ((ShareWindow*)Looper())->RefreshTransferItem(this);

               // in case our name allows us to start...
               if (old != _remoteSessionID) ((ShareWindow*)Looper())->DequeueTransferSessions();
            }
            else AbortSession(true);
         }
         else AbortSession(true);
      }
      break;

      case TRANSFER_COMMAND_FILE_DATA:
      {
         uint8 * data;
         size_t numBytes;
         if ((_uploadSession == false)&&(msg->FindDataPointer("data", B_RAW_TYPE, (void **) &data, &numBytes) == B_NO_ERROR))
         {
            bool abortSession = false;

            // paranoia!!!  This shouldn't be necessary, since TCP is supposed to handle this sort of thing.  But I want to check anyway, just in case.
            uint32 checksum;
            if (msg->FindInt32("chk", (int32*)&checksum) == B_NO_ERROR)
            {
               uint32 myChecksum = CalculateChecksum(data, numBytes);  // a little paranoioa (people keep getting munged data -> download-resume failures, why?)
               if (myChecksum != checksum)
               {
                  String errStr("Data Checksum mismatch in file [");
                  errStr += _currentFileName;

                  char temp[256];
                  sprintf(temp, "] (mine=%lu, his=%lu, %lu bytes)", myChecksum, checksum, numBytes);
                  errStr += temp;

                  errStr += " This shouldn't ever happen... looks like something is garbling your incoming TCP stream!?!?";

                  ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, errStr());
                  abortSession = true;
               }
            }

            // See if the sender has munged the data.  He should only have done this
            // if we asked him to, of course... so hopefully we'll never see
            // unknown munge code here
            int32 mungeMode;
            if (msg->FindInt32("mm", &mungeMode) == B_NO_ERROR)
            {
               switch(mungeMode)
               {
                  case MUNGE_MODE_NONE:
                     // no need to do anything!  It's in "plaintext" already
                  break;

                  case MUNGE_MODE_XOR:
                     for (size_t x=0; x<numBytes; x++) data[x] ^= 0xFF;
                  break;
 
                  default:  
                  {
                     // Oh dear.  This should never happen....
                     String errStr(str(STR_ERROR_UNKNOWN_DATA_FORMAT));
                     ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, errStr());
                     abortSession = true;
                  }
                  break;
               }
            }

            _isWaitingOnRemote = false;  // obviously....
            if (abortSession == false)
            {
               _transferStamps.AddTail(TransferStamp(system_time(), numBytes));
               if (_transferStamps.GetNumItems() > MAX_TRANSFER_STAMP_QUEUE_SIZE) _transferStamps.RemoveHead();

               if (_currentFile.Write(data, numBytes) == (ssize_t) numBytes) 
               {
                  _autoRestart = true;  // mark to restart on error only if we got something (to avoid endless cycling)
                  _currentFileBytesDone += numBytes;
                  _saveLastFileBytesDone = _currentFileBytesDone;
                  ((ShareWindow*)Looper())->AddToTransferCounts(false, (uint32) numBytes);
                  if (_currentFileBytesDone >= _currentFileSize) _origFileSet.Remove(_currentFileName);
                  if (OnceEvery(OPTIONAL_REFRESH_INTERVAL, _lastRefreshTime)) ((ShareWindow*)Looper())->RefreshTransferItem(this);
               }
               else 
               {
                  String errStr(str(STR_ERROR_WRITING_TO_FILE));
                  errStr += _currentFileName;
                  ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, errStr());
                  abortSession = true;
               }
            }
            if (abortSession) AbortSession(true);
         }
      }
      break;
   }
}

void ShareFileTransfer :: SharesScanComplete()
{
   MessageRef temp = _saveFileListMessage;    // save this separately
   _saveFileListMessage.Reset();   // clear this first, just to avoid any potential re-entrancy problems

   const Message * msg = temp();
   if (msg)
   {
      MD5Looper * md5Looper = new MD5ReceiveLooper();
      const char * nextFile;
      for (int i=0; (msg->FindString("files", i, &nextFile) == B_NO_ERROR); i++) 
      {
         String nextPath;
         if (msg->FindString("beshare:Path", i, nextPath) == B_NO_ERROR) CheckPath(nextPath);
         md5Looper->AddEntryRef(((ShareWindow*)Looper())->FindSharedFile(nextFile), nextPath());
      }

      if (md5Looper->StartProcessing(TRANSFER_COMMAND_MD5_RECV_READ_DONE, temp, BMessenger(this), &_shutdownFlag) == B_NO_ERROR) 
      {
         SendNotifyQueuedMessage();  // so they know they are waiting for us to get done...
         _md5Loopers.Put(md5Looper, true);
         ResetTransferTime();  // don't kill us due to inactivity while we're checksumming!
         ((ShareWindow*)Looper())->RefreshTransferItem(this);
      }
      else 
      {
         delete md5Looper;
         AbortSession(true);
      }
   }
}

status_t 
ShareFileTransfer :: 
GenerateNewFilename(const BDirectory & dir, BEntry & entry, uint32 count) const
{
   if (count == 0) return B_ERROR;  // avoid infinite recursion on read-only media

   // don't let BFile's pile up on the stack, they are too expensive
   bool fileExists = false;
   {
      BFile file(&entry, B_READ_WRITE);  // it's gotta be writable as well as readable!
      if (file.InitCheck() == B_NO_ERROR) fileExists = true;
   }

   if (fileExists)
   {
      // oops, file exists... gotta generate a new file name and try again!

      // Find last word in name
      char buf[B_FILE_NAME_LENGTH];
      if (entry.GetName(buf) == B_NO_ERROR)
      {
         const char * lastWord = "";
         StringTokenizer tok(buf);
         const char * t;
         while((t = tok.GetNextToken()) != NULL) lastWord = t;

         // Check last word to make sure it's just a number
         bool okay = true;
         for (const char * l = lastWord; *l != '\0'; l++)
         {
            if ((*l < '0')||(*l > '9'))
            {
               okay = false;
               break;
            }
         }

         String str(buf);
         if (okay)
         {
            int idx = str.LastIndexOf(lastWord);
            if (idx >= 0) str = str.Substring(0, idx);

            char newSuffix[100];
            sprintf(newSuffix, "%i", atoi(lastWord)+1);
            str += newSuffix;
         }
         else str += " 2";

         entry = BEntry(&dir, str(), true);
         if (GenerateNewFilename(dir, entry, count-1) != B_NO_ERROR) return B_ERROR;
      }
      else return B_ERROR;
   }
   return B_NO_ERROR;  
}


void
ShareFileTransfer::
AbortSession(bool error, bool forceNoRestart, bool notifyWindow)
{
   if ((error == false)||(forceNoRestart)) _autoRestart = false;  // don't restart if we are done or the user asked to be stopped...
   _isConnecting = _isAccepting = _isConnected = _isWaitingOnLocal = _isWaitingOnRemote = false;
   _currentFileBytesDone = _currentFileSize = -1;
   _errorOccurred = error;
   _isFinished = true;

   if (_mtt)
   {
      if (notifyWindow) ((ShareWindow*)Looper())->FileTransferDisconnected(this);
      _mtt->ShutdownInternalThread();
      delete _mtt;
      _mtt = NULL;
   }
   ResetTransferTime();

   if (_autoRestart)
   {
      _autoRestart = false;
      RestartSession();
      ((ShareWindow*)Looper())->DequeueTransferSessions();   
   }
}


void 
ShareFileTransfer ::
Update(BView * lv, const BFont * font)
{
   BListItem::Update(lv, font);

   font_height fontAttrs;
   font->GetHeight(&fontAttrs);
   _fontAscent = fontAttrs.ascent;
   _fontHeight = fontAttrs.descent+_fontAscent;

   float totalHeight = ceil(_fontHeight+1.0f);
   SetHeight(2.0f*totalHeight+max_c(totalHeight,_bitmap?_bitmap->Bounds().Height():0)+4.0f);
}

void ShareFileTransfer :: DrawText(BView * lv, const BRect & r, const BBitmap * b, const char * text, float y, float iconBottom)
{
   lv->DrawString(text, BPoint(r.left+2.0f+(((b)&&((y-_fontAscent)<=iconBottom))?(b->Bounds().Width()+6.0f):2.0f), y+r.top));
}

void 
ShareFileTransfer ::
DrawItem(BView * lv, BRect itemRect, bool /*complete*/)
{
   ShareWindow * win = (ShareWindow *) Looper();
   if ((win == NULL)||(itemRect.Width() <= 0)||(itemRect.Height() <= 0)) return; // dunno why this is necessary, but it is...

   BView * doubleBufferView = win->GetDoubleBufferView();
   if (doubleBufferView == NULL) return;

   BBitmap * doubleBufferBitmap = win->GetDoubleBufferBitmap(itemRect.Width(), itemRect.Height());
   if (doubleBufferBitmap == NULL) return;

   if (doubleBufferBitmap->Lock())
   {
      const uint32 colorType = ((_isWaitingOnLocal)||(_isWaitingOnRemote)||(_isConnected)||(_isConnecting)) ? (_uploadSession ? (_beginTransferEnabled ? COLOR_UPLOAD : COLOR_PAUSEDUPLOAD) : COLOR_DOWNLOAD) : COLOR_BORDERS;
      const int colorDiff = (colorType == COLOR_BORDERS) ? 10 : 20;
      rgb_color backgroundColor = ModifyColor(win->GetColor(colorType), colorDiff);
    
      BRect drawRect(0,0,itemRect.Width(),itemRect.Height());

      BRegion Region;
      Region.Include(drawRect);
      doubleBufferView->ConstrainClippingRegion(&Region);

      BRect colorRect = drawRect;
      colorRect.InsetBy(1.0f, 1.0f);  // avoid flicker by not drawing on the outer layer of pixels 
      String text;
      float percent = ((_isConnected)&&(_currentFileSize > 0)) ? ((float)_currentFileBytesDone)/_currentFileSize : ((_saveLastFileSize > 0) ? ((float)_saveLastFileBytesDone)/_saveLastFileSize : 0.0f);
      BRect fillRectRight(colorRect);
      fillRectRight.left = (colorRect.left + colorRect.Width()*percent);
      doubleBufferView->SetHighColor(backgroundColor);
      doubleBufferView->FillRect(fillRectRight);

      rgb_color barColor = backgroundColor;
      BRect fillRectLeft(colorRect);
      {
         barColor = ModifyColor(win->GetColor(colorType), -colorDiff);
         fillRectLeft.right = fillRectRight.left;
         doubleBufferView->SetHighColor(barColor);
         doubleBufferView->FillRect(fillRectLeft);

         // Do little scrolly barber pole animation thingy
         {
            _animShift += 2.0f;
            if (_animShift > ANIM_RECT_SPACING*2.0f) _animShift -= (ANIM_RECT_SPACING*2.0f);

            doubleBufferView->SetHighColor(win->GetColor(colorType));

            BRegion barberRegion;
            barberRegion.Include(fillRectLeft);
            doubleBufferView->ConstrainClippingRegion(&barberRegion);
            for (float rl=fillRectRight.right+(ANIM_RECT_SPACING*4)-_animShift; rl >= -ANIM_RECT_SPACING; rl -= ANIM_RECT_SPACING*2.0f)
            {
               const BPoint points[4] = {
                  BPoint(rl,                   fillRectLeft.top),
                  BPoint(rl-ANIM_RECT_WIDTH,   fillRectLeft.top),
                  BPoint(rl-ANIM_RECT_SPACING*4, fillRectLeft.bottom),
                  BPoint(ANIM_RECT_WIDTH+(rl-ANIM_RECT_SPACING*4), fillRectLeft.bottom)
               };
               doubleBufferView->FillPolygon(points, ARRAYITEMS(points));
            }
            doubleBufferView->ConstrainClippingRegion(&Region);
         }
      }

      bool addSizeData = true;
      if (_errorOccurred) 
      {
         if (_banEndTime > 0)
         {
            text = str(STR_BANNED);
            if (_banEndTime != BESHARE_UNKNOWN_BAN_TIME_LEFT)
            {
               text += ' ';
               if (_banEndTime == ((uint64)-1)) text += str(STR_FOREVER);
               else
               {
                  text += str(STR_UNTIL);
                  time_t tt = _banEndTime/1000000;
                  struct tm * t = localtime(&tt);
                  if (t)
                  {
                     struct tm then = *t;
                     tt = GetCurrentTime64()/1000000;
                     t = localtime(&tt);
                     if (t) 
                     {
                        char buf[128];

                        // Only show the date if it's different from today's date
                        struct tm now = *t;
                        if ((now.tm_year != then.tm_year)||
                            (now.tm_yday != then.tm_yday))
                        {
                           sprintf(buf, " %i/%i/%02i", then.tm_mon+1, then.tm_mday, then.tm_year%100); 
                           text += buf;
                        }
    
                        sprintf(buf, " %02i:%02i", then.tm_hour, then.tm_min);
                        text += buf;
                     }
                  }
               }
            }
         }
         else text = str(STR_AN_ERROR_OCCURRED);
      }
      else
      {
              if (_isWaitingOnLocal) text = str(STR_QUEUED_LOCAL_MACHINE_TOO_BUSY);
         else if (_isWaitingOnRemote) text = str(STR_QUEUED_REMOTE_MACHINE_TOO_BUSY);
         else if (_isConnected)
         {
            if (_md5Loopers.GetNumItems() > 0) text = str(STR_EXAMINING_FILES);
            else
            {
               text = _uploadSession ? str(STR_SENT) : str(STR_RCVD);

               bool useCurrent = (_currentFileSize > 0); 
               text += GetFileSizeDataString(useCurrent ? _currentFileBytesDone : _saveLastFileBytesDone, useCurrent ? _currentFileSize : _saveLastFileSize);
               addSizeData = false;

               if (_transferStamps.GetNumItems() > 2)
               {
                  bigtime_t elapsedTime = system_time() - _transferStamps.Head().GetWhen();
                  if (elapsedTime > 0)
                  {
                     uint64 bytesInQueue = 0;
                     for (int32 i=_transferStamps.GetNumItems()-1; i>=0; i--) bytesInQueue += _transferStamps[i].GetNumBytes();

                     // Add performance info, e.g. (43KB/sec, 5:05)
                     text += " (";
        
                     uint64 bytesPerSecond = (((bytesInQueue)*1000000LL)/elapsedTime);
                     uint64 bytesToGo      = _currentFileSize-_currentFileBytesDone;
                     uint64 secondsToGo    = (bytesPerSecond > 0) ? (bytesToGo/bytesPerSecond) : 0;

                     char buf[32];
                     GetByteSizeString(bytesPerSecond, buf); 
                     text += buf;
                     text += str(STR_SEC);
                 
                     if (secondsToGo >= (60*60)) sprintf(buf, "%Lu:%02Lu:%02Lu", secondsToGo/(60*60), (secondsToGo/60)%60, secondsToGo%60);
                                            else sprintf(buf, "%Lu:%02Lu", secondsToGo/60, secondsToGo%60);
                     text += buf;
                     text += ")";
                  }
               }
            }
         }
         else
         {
                 if (_isAccepting) text = str(STR_AWAITING_CALLBACK);
            else if (_isConnecting) text = str(STR_CONNECTING);
            else if (_isFinished) 
            {
               if (_wasConnected) text = (_fileSet.GetNumItems() == 0) ? str(STR_DOWNLOAD_COMPLETE) : str(STR_DOWNLOAD_ABORTED);
                             else text = str(STR_COULDNT_CONNECT);
            }
            else text = _uploadSession ? str(STR_PREPARING_TO_UPLOAD) : str(STR_PREPARING_TO_DOWNLOAD);
         }
      }

      if (addSizeData) 
      {
         String fsds = GetFileSizeDataString(_saveLastFileBytesDone, _saveLastFileSize);
         if (fsds.Length() > 0) 
         {
            text += " - ";
            text += fsds;
         }
      }

      char line2[64];
      sprintf(line2, "(%i/%i) %s ", _currentFileIndex, _origFileSetSize, _uploadSession?str(STR_TO):str(STR_FROM));
      String line2Str(line2);
      line2Str += _displayRemoteUserName;
      line2Str += " (";
      line2Str += _remoteSessionID;
      line2Str += ")";

      const char * cfn = _currentFileName();
      if (cfn[0] == '\0')
      {
         // Get the first name from the set then, if possible
         const String * first = _fileSet.GetIterator().GetNextKey();
         if (first) cfn = first->Cstr();
      }
      if (cfn[0] == '\0')
      {
         // Still nothing?  Get the first name from the orig set then, if possible
         const String * first = _origFileSet.GetIterator().GetNextKey();
         if (first) cfn = first->Cstr();
      }

      float vNudge = 2.0f;
      float vSpace = (colorRect.Height()-(_fontHeight*3.0f))/4.0f;
      float topTextY = vSpace+_fontHeight-vNudge;
      float iconHeight = _bitmap?_bitmap->Bounds().Height():0.0f;
      float iconBottom = (topTextY-(_fontHeight/2.0f))+(iconHeight/2.0f);
      if (iconBottom < iconHeight) iconBottom = iconHeight;

      doubleBufferView->SetHighColor(Black);
      for (int i=0; i<2; i++)
      {
         doubleBufferView->SetLowColor((i==0)?barColor:backgroundColor);
         BRegion region((i==0)?fillRectLeft:fillRectRight);
         doubleBufferView->ConstrainClippingRegion(&region);
         DrawText(doubleBufferView, colorRect, _bitmap, cfn, topTextY, iconBottom);
         DrawText(doubleBufferView, colorRect, _bitmap, line2Str(), (colorRect.Height()+_fontHeight-vNudge)/2.0f, iconBottom);
         DrawText(doubleBufferView, colorRect, _bitmap, text(), colorRect.Height()-(vSpace+vNudge), iconBottom);
         doubleBufferView->ConstrainClippingRegion(NULL);
      }

      if (_bitmap)
      {
         doubleBufferView->SetDrawingMode(B_OP_OVER);
         doubleBufferView->DrawBitmap(_bitmap, BPoint(colorRect.left + 2.0f, colorRect.top+iconBottom-_bitmap->Bounds().Height()));
      }

      // Do border highlighting
      bool selected = IsSelected();
      doubleBufferView->SetHighColor(selected ? Black : White);
      doubleBufferView->StrokeLine(BPoint(drawRect.left, drawRect.top), BPoint(drawRect.right, drawRect.top));
      doubleBufferView->StrokeLine(BPoint(drawRect.left, drawRect.top), BPoint(drawRect.left, drawRect.bottom));
      doubleBufferView->SetHighColor(selected ? White : Black);
      doubleBufferView->StrokeLine(BPoint(drawRect.left, drawRect.bottom), BPoint(drawRect.right, drawRect.bottom));
      doubleBufferView->StrokeLine(BPoint(drawRect.right, drawRect.top), BPoint(drawRect.right, drawRect.bottom));

      doubleBufferView->ConstrainClippingRegion(NULL);

      doubleBufferView->Sync();
      doubleBufferBitmap->Unlock(); 

      lv->DrawBitmap(doubleBufferBitmap, drawRect, itemRect);
   }
}

String ShareFileTransfer :: GetFileSizeDataString(int64 bd, int64 fs) const
{
   if (fs > 0)
   {
      char doneBuf[32], sizeBuf[32];
      GetByteSizeString(bd, doneBuf);
      GetByteSizeString(fs, sizeBuf);
      char buf[80];
      sprintf(buf, "%s/%s", doneBuf, sizeBuf);
      return buf;
   }
   return "";
}


void
ShareFileTransfer ::
BeginTransfer()
{
   if ((_beginTransferEnabled)&&(_isWaitingOnLocal))
   {
      bool startConnect = false;

      _isWaitingOnLocal = false;
      if (_uploadSession)
      {
         if (_isConnected) 
         {
            if (_fileSet.GetNumItems() > 0) DoUpload();
         }
         else startConnect = true;
      }
      else
      {
         if (_isAccepting)
         {
            uint16 port = _mtt ? _acceptingOn : 0;
            if (port > 0) ((ShareWindow*)Looper())->SendConnectBackRequestMessage(_remoteSessionID(), port);
                     else ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, str(STR_ERROR_STARTING_DOWNLOAD_NO_ACCEPT_PORT));
         }
         else startConnect = true;
      }

      if (startConnect)
      {
         if (_mtt)
         {
            if ((_mtt->StartInternalThread() == B_NO_ERROR)&&(_mtt->AddNewConnectSession(_remoteHostName(), _remotePort, GetTransferSessionRef()) == B_NO_ERROR)) _isConnecting = true;
            else ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, str(STR_ERROR_STARTING_DELAYED_CONNECT));
         }
         else ((ShareWindow*)Looper())->LogMessage(LOG_ERROR_MESSAGE, str(STR_ERROR_STARTING_DELAYED_CONNECT_NO_TRANSCEIVER_THREAD));
      }

      ((ShareWindow *)Looper())->RefreshTransferItem(this);
   }
   UpdateTransferTime();
}


void
ShareFileTransfer ::
OpenCurrentFileFolder()
{
    node_ref tempRef;
    tempRef.device = _currentFileEntry.device;
    tempRef.node = _currentFileEntry.directory;
    BDirectory dir(&tempRef);
    ((ShareWindow*)Looper())->OpenTrackerFolder(dir);
}

status_t
ShareFileTransfer ::
LaunchCurrentItem()
{
   const char * cfn = _currentFileName();
   if (cfn[0] != '\0') return be_roster->Launch(&_currentFileEntry);

   // No current file name?  Get the first name from the set then, if possible
   {
      const String * first = _fileSet.GetIterator().GetNextKey();
      if (first) 
      {
         entry_ref er = ((ShareWindow*)Looper())->FindSharedFile(first->Cstr());
         return be_roster->Launch(&er);
      }
   }

   // Still nothing?  Get the first name from the orig set then, if possible
   {
      const String * first = _origFileSet.GetIterator().GetNextKey();
      if (first) 
      {
         entry_ref er = ((ShareWindow*)Looper())->FindSharedFile(first->Cstr());
         return be_roster->Launch(&er);
      }
   }

   return B_ERROR;
}

void
ShareFileTransfer ::
UpdateTransferTime()
{
   _lastTransferTime = ((_isWaitingOnLocal)||(_isWaitingOnRemote)) ? 0LL : system_time();
}

void
ShareFileTransfer ::
ResetTransferTime()
{
   _lastTransferTime = 0LL;
}


void
ShareFileTransfer ::
RestartSession()
{
   // Close any current session....
   AbortSession(false, true, false);

   // but it isn't over yet!
   _isFinished = false;
   _isWaitingOnLocal = true;
   _currentFileIndex = 0;
   _shutdownFlag = false;  // so our MD5's will work
   
   // Reset our state to the initial state
   _transferStamps.Clear();
   _currentFileName = "";    // gotta reset this!
   _fileSet = _origFileSet;  // store this in case we need to restart
   _origFileSetSize = _fileSet.GetNumItems();
   _fileSetIter = _fileSet.GetIterator();  // rewind file set
   if (_remoteHostName.Length() > 0) InitConnectSession(_remoteHostName(), _remotePort, _remoteIP, _remoteSessionID());
                                else InitAcceptSession(_remoteSessionID());
}

void
ShareFileTransfer ::
RequeueTransfer()
{
   _isWaitingOnLocal = true;
   SendNotifyQueuedMessage();
}

void 
ShareFileTransfer :: TransferCallbackRejected(uint64 banTimeLeft)
{
   _banEndTime = (banTimeLeft >= BESHARE_UNKNOWN_BAN_TIME_LEFT) ? banTimeLeft : GetCurrentTime64()+banTimeLeft;
   AbortSession(true, true);
}

uint64 
ShareFileTransfer :: GetNumBytesLeftToUpload(const ShareNetClient * snc) const
{
   off_t count = 0;
   if (IsUploadSession())
   {
      if (_currentFileBytesDone < _currentFileSize) count += (_currentFileSize - _currentFileBytesDone);

      HashtableIterator<String, OffsetAndPath> iter = _fileSetIter;  // keeps track of where we are in our fileset for uploading
      const String * nextName;
      while((nextName = iter.GetNextKey()) != NULL) 
      {
         off_t fs = snc->GetSharedFileSize(*nextName);
         if (fs > 0) count += fs;
      }
   }
   return count;
}

};  // end namespace beshare
