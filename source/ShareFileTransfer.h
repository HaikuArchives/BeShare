#ifndef SHARE_FILE_TRANSFER_H
#define SHARE_FILE_TRANSFER_H

#include <app/Handler.h>
#include <storage/Entry.h>
#include <storage/File.h>
#include <storage/Directory.h>
#include <interface/Bitmap.h>
#include <interface/Font.h>
#include <interface/ListItem.h>

#include "util/Hashtable.h"
#include "util/String.h"
#include "besupport/BThread.h"

using namespace muscle;

namespace beshare {

// Different ways to disguise our data packets
// to avoid being filtered by evil men in the middle
enum {
   MUNGE_MODE_NONE = 0,   // data is placed in messages as is
   MUNGE_MODE_XOR,        // data is XOR'd first
   NUM_MUNGE_MODES
};

class MD5Looper;
class ShareNetClient;

/* a simple storage class, holds a file offset and a file path */
class OffsetAndPath
{ 
public:
   OffsetAndPath() {/* empty */}
   OffsetAndPath(off_t offset, const String & path) : _offset(offset), _path(path) {/* empty */}

   void SaveToArchive(BMessage & archive) const
   {
      archive.AddString("path", _path());
      archive.AddInt64("offset", _offset);
   }

   void SetFromArchive(const BMessage & archive)
   {
      const char * p; 
      _path = (archive.FindString("path", &p) == B_NO_ERROR) ? p : "";
      if (archive.FindInt64("offset", &_offset) != B_NO_ERROR) _offset = 0;
   }

   off_t _offset;
   String _path;
};

/*
 * This class handles the sending and receiving of one or more files 
 * to/from another BeShare client.  It also serves as a BListItem 
 * for the BeShare GUI, so that the user can track the download's
 * progress.
 */
class ShareFileTransfer : public BHandler, public BListItem 
{
public:
   /* This opens a Tracker window that the current file exists in. */
   void OpenCurrentFileFolder();

   /* 
    * Creates a new session.  Call AddFile() on it with whatever files you 
    * want it to access for upload/download, then call Init*Session().
    */
   ShareFileTransfer(const BDirectory & fileDir, const char * localSessionID, uint64 remoteInstallID, uint64 partialHashSize, uint32 bandwidthLimit);

   /*
    * Destroys the ShareFileTransfer object, cancelling any transfer currently in progress. 
    */
   ~ShareFileTransfer();

   /** Save relevant info into (archive) for storage */
   void SaveToArchive(BMessage & archive) const;

   /** Retrieve state from (archive) */
   void SetFromArchive(const BMessage & archive);

   /* 
    * Adds the given file to our list of files to request.
    * (startByteOffset) indicates the first byte to transfer (only
    * meaningful when uploading files)
    * (path) is the sub-path to download to
    */
   void AddRequestedFileName(const char * fileName, off_t startByteOffset, const char * path, BPoint *point);

   /*
    * Initiates an outgoing connection to (hostName, port).  Once connected, behaviour is
    * determined by our file set:  If we have no files to request, we will allow the remote
    * user to request and download files from us, otherwise we will request and download files
    * from him.  Call BeginTransfer() to start the actual transfer of data.
    */
   status_t InitConnectSession(const char * hostName, uint16 port, uint32 remoteIP, const char * remoteSessionID);

   /*
    * Begins listening on a chosen port for incoming connections.  When a connection is received,
    * handle it as described above (in InitConnectSession())
    * If (wait) is false, we will send a message to the MUSCLE server
    * asking the peer client to connect back to us; if it's true, we won't.
    * Call BeginTransfer() to start the actual transfer of data. 
    */
   status_t InitAcceptSession(const char * remoteSessionID);

   /*
    * Begins a session that will allow the remote user to request files to be sent to him.
    * If (sendNotifyQueued) is true, we will send the peer a message telling him that he's 
    * on our queue of things to do (i.e. not ready to be processed yet).
    * Call BeginTransfer() to start the actual transfer of data. 
    */
   status_t InitSocketUploadSession(const ConstSocketRef & socket, uint32 remoteIP, bool sendNotifyQueued);

   /* Handles notification BMessages from the BMessageTransceiverThread */
   virtual void MessageReceived(BMessage * msg);

   /* Handles Messages from the BMessageTransceiverThread */
   void MessageReceived(const MessageRef & msgRef);

   /* BListItem interface */
   virtual void Update(BView * owner, const BFont * font);
   virtual void DrawItem(BView * owner, BRect itemRect, bool complete = false);

   /* Called if we want the list item to show a MIME file icon.
    * Note that this object does not assume ownership of (bmap).
    */
   void SetBitmap(const BBitmap * bmap);

   /* Returns our current set of files to be uploaded or downloaded */
   Hashtable<String, OffsetAndPath> & GetFileSet() {return _fileSet;}

   /* Returns the set of files that we started with at connect time */
   Hashtable<String, OffsetAndPath> & GetOriginalFileSet() {return _origFileSet;}

   /** Returns the originally requeusted set of all time, for display purposes. */
   const Hashtable<String, OffsetAndPath> & GetDisplayFileSet() const {return _displayFileSet;}

   /* Accessors for some miscellaneous state variables */
   bool IsUploadSession() const {return _uploadSession;}
   bool IsConnected() const {return _isConnected;}
   bool IsConnecting() const {return _isConnecting;}
   bool WasConnected() const {return _wasConnected;}
   bool IsWaitingOnLocal() const {return _isWaitingOnLocal;}
   bool IsWaitingOnRemote() const {return _isWaitingOnRemote;}
   bool IsFinished() const {return _isFinished;}
   bool IsAcceptSession() const {return _isAcceptSession;}
   bool IsAccepting() const {return _isAccepting;}

   /* Returns true iff this session is considered to be 'doing something' */
   bool IsActive() const {return ((_isWaitingOnLocal == false)&&(_isFinished == false));}

   /* Call this when you're ready to actually begin the transfer.  */
   void BeginTransfer();

   /* This turns an uploading session back into a queued one */
   void RequeueTransfer();

   /* Tries to execute the currently active file */
   status_t LaunchCurrentItem();

   static status_t FindSharedFile(BFile & retFile, const BDirectory & rootDir, const char * fileName, entry_ref * optRetEntryRef, volatile bool * optShutdownFlag);

   /* Returns the system_time() at which the last packet of data was sent or
    * received, or zero if we are still waiting to start transfer. */
   bigtime_t LastTransferTime() const {return _lastTransferTime;}

   /* Cause this session to abort, with or without an error condition */
   void AbortSession(bool errorOccurred, bool forceNoRestart = false, bool notifyWindow = true);

   /* Try to restart the session (reconnect, etc) */
   void RestartSession();

   /* Return the remote session ID, if known. */
   const char * GetRemoteSessionID() const {return _remoteSessionID();}

   /* Returns true if something went wrong in the download */
   bool ErrorOccurred() const {return _errorOccurred;}

   /* Set this false so that this upload never begins!  (Makes BeginTransfer() a no-op) */
   void SetBeginTransferEnabled(bool e) {_beginTransferEnabled = e;}

   /* Return the state of the transfer-enabled flag */
   bool GetBeginTransferEnabled() const {return _beginTransferEnabled;}

   /* Returns an estimate of the number of bytes this session still has left to upload. */
   uint64 GetNumBytesLeftToUpload(const ShareNetClient * snc) const;

   /* Returns the IP address of the remote user for an upload.
    * Returns zero if IP address isn't known. 
    */
   uint32 GetRemoteIP() const {return _remoteIP;}

   /** Returns the install ID of the computer we are connected to (or zero if unknown).  */
   uint64 GetRemoteInstallID() const {return _remoteInstallID;}

   /** Updates our displayed username from the name database.  Doesn't refresh the display tho */
   void UpdateRemoteUserName();

   /** Changes our local-session-ID.  Useful when the local session ID wasn't known at construct time. */
   status_t SetLocalSessionID(const String & id) {_localSessionID = id; return B_NO_ERROR;}

   /** Sets a bandwidth limit for this session in bytes/sec.  Zero means 'no limit'.  May be called at any time. */
   void SetBandwidthLimit(uint32 limit);

   /** Returns the current bandwidth limit for this session in bytes/sec.  Zero means 'no limit'. */
   uint32 GetBandwidthLimit() const {return _bandwidthLimit;}

   /** Called by the ShareWindow whenever an asynchronous-file-scan has completed. */
   void SharesScanComplete();

   /** Called if our request for a callback was rejected */
   void TransferCallbackRejected(uint64 banTimeLeft);

   enum 
   {
      TRANSFER_COMMAND_CONNECTED_TO_PEER='tshr', 
      TRANSFER_COMMAND_DISCONNECTED_FROM_PEER,
      TRANSFER_COMMAND_FILE_LIST,      // a list of files the remote user would like to download
      TRANSFER_COMMAND_FILE_HEADER,    // contains filename, attributes, etc.
      TRANSFER_COMMAND_FILE_DATA,      // a chunk of the file's contents
      TRANSFER_COMMAND_DEPRECATED,     // was the on-empty message
      TRANSFER_COMMAND_NOTIFY_QUEUED,  // tells the receiving session he's being put on a wait list to download
      TRANSFER_COMMAND_MD5_SEND_READ_DONE, // sent to us by our MD5Looper when it's done
      TRANSFER_COMMAND_MD5_RECV_READ_DONE, // sent to us by our MD5Looper when it's done
      TRANSFER_COMMAND_PEER_ID,            // just communicates the requester's name & sessionID
      TRANSFER_COMMAND_REJECTED            // tell wannabe downloader his requested has been denied
   };

private:
   status_t GenerateNewFilename(const BDirectory & dir, BEntry & getRetEntry, uint32 count) const;
   void DrawText(BView * lv, const BRect & r, const BBitmap * b, const char * text, float y, float labelBottom);
   ThreadWorkerSessionRef GetTransferSessionRef();
   String GetFileSizeDataString(int64 bd, int64 fs) const;

   void DoUpload();
   bool DoUploadAux();  // Should be called by DoUpload() only!
   void SendNotifyQueuedMessage();
   void UpdateTransferTime();
   void ResetTransferTime();

   uint32 CalculateChecksum(const uint8 * data, size_t bufSize) const;
   const char* _GetFileName();

   const BDirectory _dir;

   Hashtable<String, BPoint> _pointSet;                    // Location of the file in case drag&dropped
   Hashtable<String, OffsetAndPath> _fileSet;              // set of files to request from remote peer -> starting byte offset
   Hashtable<String, OffsetAndPath> _origFileSet;          // backup of the file set, for restarts
   Hashtable<String, OffsetAndPath> _displayFileSet;       // original set, for display purposes only
   HashtableIterator<String, OffsetAndPath> _fileSetIter;  // keeps track of where we are in our fileset for uploading

   Hashtable<MD5Looper *, bool> _md5Loopers;

   BFile _currentFile;
   entry_ref _currentFileEntry;
   String _currentFileName;

   // Used to calculate a running average of the transfer rate
   class TransferStamp
   {
   public:
      TransferStamp() : _when(0), _numBytes(0) {/* empty */}
      TransferStamp(uint64 when, uint64 numBytes) : _when(when), _numBytes(numBytes) {/* empty */}

      uint64 GetWhen() const {return _when;}
      uint64 GetNumBytes() const {return _numBytes;}
   
   private:
      uint64 _when;
      uint64 _numBytes;
   };
       
   Queue<TransferStamp> _transferStamps;

   off_t _currentFileSize;
   off_t _currentFileBytesDone;
   off_t _saveLastFileSize;
   off_t _saveLastFileBytesDone;
   bool _autoRestart;

   BMessageTransceiverThread * _mtt;

   bool _uploadSession;
 
   const BBitmap * _bitmap;
   float _fontHeight;
   float _fontAscent;

   String _localSessionID;
   String _remoteSessionID;
   String _remoteUserName;
   String _displayRemoteUserName;
   String _remoteHostName;
   int _remotePort;
   uint32 _remoteIP;         // used for bans

   uint16 _acceptingOn;      // port we're accepting on, if we're accepting
   bool _isAccepting;        // true iff we're listening on a local port for incoming connections
   bool _isAcceptSession;    // true iff we were started via InitAcceptSession()
   bool _isConnecting;       // true iff we're in the process of establishing an outgoing connection
   bool _isConnected;        // true iff the connection to the peer is established
   bool _isWaitingOnLocal;   // true iff we've been queued until the local computer has room for us
   bool _isWaitingOnRemote;  // true iff we've been queued until the remote computer has room for us
   bool _wasConnected;       // true iff are connected now, or were connected previously
   bool _errorOccurred;      // true iff something went wrong
   bool _isFinished;         // true iff we have nothing more to do

   int _currentFileIndex;
   int _origFileSetSize;

   float _animShift;
   bigtime_t _lastTransferTime;

   volatile bool _shutdownFlag;

   bool _beginTransferEnabled;
   bool _uploadWaitingForSendToFinish;

   int32 _mungeMode;         // what mode we should use for this session, given a choice
   uint64 _remoteInstallID;
   uint64 _partialHashSize;

   uint32 _bandwidthLimit;

   uint64 _lastRefreshTime;

   MessageRef _saveFileListMessage;
   uint64 _banEndTime;
};

};  // end namespace beshare

#endif
