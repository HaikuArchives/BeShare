#ifndef REMOTE_USER_H
#define REMOTE_USER_H

#include "message/Message.h"
#include "CLVEasyItem.h"
#include "BeShareNameSpace.h"

class BBitmap;

namespace beshare {

class RemoteFileItem;
class ShareWindow;

// This class represents a single remote user and his current matching file set
class RemoteUserItem : public CLVEasyItem
{
public:
   RemoteUserItem(ShareWindow * owner, const char * sessionID);

   ~RemoteUserItem();

   void SetHandle(const char * handle, const char * displayHandle);
   void SetStatus(const char * status, const char * displayStatus);
   void SetHostName(const char * hostName) {_hostName = hostName;}
   void SetPort(int port) {_port = port;}
   void SetInstallID(uint64 id) {_installID = id;}
   void SetClient(const char * client, const char * displayClient);
   void SetSupportsPartialHash(bool sph) {_supportsPartialHash = sph;}

   virtual void DrawItemColumn(BView* owner, BRect item_column_rect, int32 column_index, bool complete);

   void PutFile(const char * fileName, const MessageRef & fileAttrs);
   void RemoveFile(const char * fileName);
   void ClearFiles();

   int GetNumFiles() const {return _files.GetNumItems();}

   const char * GetSessionID() const {return _sessionID();}
   const char * GetVerbatimHandle() const {return _handle();}
   const char * GetDisplayHandle() const {return _displayHandle();}
   const char * GetVerbatimStatus() const {return _status();}
   const char * GetDisplayStatus() const {return _displayStatus();}
   const char * GetHostName() const {return _hostName();}
   const char * GetBandwidthLabel() const {return _bandwidthLabel();}
   int GetPort() const {return _port;}
   uint64 GetInstallID() const {return _installID;}
   const char * GetVerbatimClient() const {return _client();}
   const char * GetDisplayClient() const {return _displayClient();}
   bool GetSupportsPartialHash() const {return _supportsPartialHash;}

   int Compare(const RemoteUserItem * i2, int32 sortKey) const;

   ShareWindow * GetOwner() const {return _owner;}

   String GetUserString() const;

   bool GetFirewalled() const {return _firewalled;}
   void SetFirewalled(bool f);

   void SetIsBot(bool isBot) {_isBot = isBot;}
   bool IsBot() const {return _isBot;}

   void SetBandwidth(const char * label, uint32 bps);
   uint32 GetBandwidth() const {return _bandwidth;}

   void SetNumSharedFiles(int32 nsf);
   int32 GetNumSharedFiles() const {return _numSharedFiles;}

   void SetUploadStats(uint32 curUploads, uint32 maxUploads);

private:
   float GetLoadFactor() const;

   ShareWindow * _owner;
   String _sessionID;
   String _handle;
   String _displayHandle;
   String _status;
   String _displayStatus;
   String _hostName;
   String _client;
   String _displayClient;
   int _port;
   bool _firewalled;
   bool _isBot;
   uint32 _curUploads;
   uint32 _maxUploads;
   bool _supportsPartialHash;

   OrderedKeysHashtable<const char *, RemoteFileItem *> _files;

   String _bandwidthLabel;
   uint32 _bandwidth;

   int32 _numSharedFiles;

   uint64 _installID;
};

};  // end namespace beshare


#endif
