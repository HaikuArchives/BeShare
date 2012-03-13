#include "RemoteUserItem.h"
#include "RemoteFileItem.h"
#include "ShareStrings.h"
#include "ShareWindow.h"

namespace beshare {

enum {
   REMOTE_USER_COLUMN_HANDLE = 0,
   REMOTE_USER_COLUMN_STATUS,
   REMOTE_USER_COLUMN_ID,
   REMOTE_USER_COLUMN_FILES,
   REMOTE_USER_COLUMN_BANDWIDTH,
   REMOTE_USER_COLUMN_LOAD,
   REMOTE_USER_COLUMN_CLIENT,
   NUM_REMOTE_USER_COLUMNS
};

RemoteUserItem ::
RemoteUserItem(ShareWindow * owner, const char * sessionID)
   : _owner(owner), _sessionID(sessionID), _handle(str(STR_ANONYMOUS)), _displayHandle(str(STR_ANONYMOUS)), _port(0), _firewalled(false), _isBot(false), _supportsPartialHash(false), _bandwidthLabel(str(STR_UNKNOWN)), _bandwidth(0), _installID(0)
{
   _files.SetKeyCompareFunction(CStringCompareFunc);
 
   String text = GetUserString();
   text += str(STR_IS_NOW_CONNECTED); 
   _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());

   SetColumnContent(REMOTE_USER_COLUMN_HANDLE, _displayHandle(), false, false);
   SetColumnContent(REMOTE_USER_COLUMN_STATUS, _displayStatus(), false, false);
   SetColumnContent(REMOTE_USER_COLUMN_ID,     _sessionID(), false, true);
   SetNumSharedFiles(-1);
   SetUploadStats(NO_FILE_LIMIT, NO_FILE_LIMIT);
}

RemoteUserItem ::
~RemoteUserItem()
{
   String text = GetUserString();
   text += str(STR_HAS_DISCONNECTED);
   _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());
   ClearFiles();
}

String
RemoteUserItem ::
GetUserString() const
{
   String text(str(STR_USER_NUMBER));
   text += _sessionID;

   if (!_handle.Equals(str(STR_ANONYMOUS)))
   {
      text += str(STR_AKA);
      text += _handle();
      text += ")";
   }
   return text;
}

void
RemoteUserItem ::
SetHandle(const char * handle, const char * displayHandle)
{
   if ((strcmp(handle, _handle()))||(strcmp(displayHandle, _displayHandle())))
   {
      String text = GetUserString();
      text += str(STR_IS_NOW_KNOWN_AS);
      text += handle;
      _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());

      _handle = handle;
      _displayHandle = displayHandle;

      SetColumnContent(REMOTE_USER_COLUMN_HANDLE, _displayHandle(), false, false);
      _owner->RefreshUserItem(this);
      _owner->RefreshTransfersFor(this);

      // Make sure all our files refresh too, in case the "owner name" column is visible
      RemoteFileItem * next;
      HashtableIterator<const char *, RemoteFileItem *> iter = _files.GetIterator();
      while(iter.GetNextValue(next) == B_NO_ERROR) _owner->RefreshFileItem(next);
   }

   
}

void
RemoteUserItem ::
SetStatus(const char * status, const char * displayStatus)
{
   String text = GetUserString();
   text += str(STR_IS_NOW);
   text += status;
   _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());

   _status = status;
   _displayStatus = displayStatus;
   SetColumnContent(REMOTE_USER_COLUMN_STATUS, _displayStatus(), false, false);
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetNumSharedFiles(int32 bw)
{
   _numSharedFiles = bw;
  
   char temp[100] = "?";
   if (_numSharedFiles >= 0) 
   {
      if ((_firewalled)&&(_owner->GetFirewalled())) sprintf(temp, "(%li)", _numSharedFiles);
                                               else sprintf(temp, "%li", _numSharedFiles);
   }
   SetColumnContent(REMOTE_USER_COLUMN_FILES, temp, false, true);
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetUploadStats(uint32 cu, uint32 mu)
{
   _curUploads = cu;
   _maxUploads = mu;

   char temp[128];
   if (_curUploads < NO_FILE_LIMIT)
   {
      if (_maxUploads >= NO_FILE_LIMIT) sprintf(temp, "(%lu) 0%%", _curUploads);
                                   else sprintf(temp, "(%lu/%lu) %.0f%%", _curUploads, _maxUploads, GetLoadFactor()*100.0f);
   }
   else strcpy(temp, "?");

   SetColumnContent(REMOTE_USER_COLUMN_LOAD, temp, false, true);
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetBandwidth(const char * bandwidthLabel, uint32 bps)
{
   _bandwidthLabel = bandwidthLabel;
   _bandwidth = bps;

   // Make sure all our files refresh too, in case the "owner bps" column is visible
   RemoteFileItem * next;
   HashtableIterator<const char *, RemoteFileItem *> iter = _files.GetIterator();
   while(iter.GetNextValue(next) == B_NO_ERROR) _owner->RefreshFileItem(next);

   SetColumnContent(REMOTE_USER_COLUMN_BANDWIDTH, _bandwidthLabel(), false, false);
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetClient(const char * client, const char * displayClient)
{
   _client = client;
   _displayClient = displayClient;
   SetColumnContent(REMOTE_USER_COLUMN_CLIENT, _displayClient(), false, false);
}

void
RemoteUserItem ::
ClearFiles()
{
   HashtableIterator<const char *, RemoteFileItem *> iter = _files.GetIterator();
   RemoteFileItem * next;
   while(iter.GetNextValue(next) == B_NO_ERROR) 
   {
      _owner->RemoveFileItem(next);
      delete next;
   }
   _files.Clear();
}

void
RemoteUserItem ::
PutFile(const char * fileName, const MessageRef & fileAttrs)
{
   RemoveFile(fileName);
   RemoteFileItem * item = new RemoteFileItem(this, fileName, fileAttrs);
   _files.Put(item->GetFileName(), item);
   _owner->AddFileItem(item);
}


void
RemoteUserItem ::
RemoveFile(const char * fileName)
{
   RemoteFileItem * item;
   if (_files.Remove(fileName, item) == B_NO_ERROR)
   {
      _owner->RemoveFileItem(item);
      delete item;
   }
}


void
RemoteUserItem ::
SetFirewalled(bool fw)
{
   if (fw != _firewalled)
   {
      _firewalled = fw;
      SetNumSharedFiles(GetNumSharedFiles());  // force update
   }
}

void 
RemoteUserItem :: 
DrawItemColumn(BView* owner, BRect rect, int32 col, bool complete)
{
   if ((_isBot)&&(col == 0))
   {
      BFont f;
      owner->GetFont(&f);
      font_family family; 
      font_style style;
      f.GetFamilyAndStyle(&family, &style);
      f.SetFamilyAndStyle(family, "Italic");
      owner->SetFont(&f);
      CLVEasyItem::DrawItemColumn(owner, rect, col, complete);
      f.SetFamilyAndStyle(family, style);
      owner->SetFont(&f);
   }
   else CLVEasyItem::DrawItemColumn(owner, rect, col, complete);
}

int 
RemoteUserItem ::
Compare(const RemoteUserItem * u2, int32 sortKey) const
{
   switch(sortKey)
   {
      case REMOTE_USER_COLUMN_HANDLE:    return strcasecmp(GetDisplayHandle(), u2->GetDisplayHandle());
      case REMOTE_USER_COLUMN_STATUS:    return strcasecmp(GetDisplayStatus(), u2->GetDisplayStatus());
      case REMOTE_USER_COLUMN_ID:        return atol(GetSessionID())-atol(u2->GetSessionID());
      case REMOTE_USER_COLUMN_FILES:     return (GetNumSharedFiles() > u2->GetNumSharedFiles()) ? -1 : ((GetNumSharedFiles() == u2->GetNumSharedFiles()) ? 0 : 1);
      case REMOTE_USER_COLUMN_BANDWIDTH: return (GetBandwidth() > u2->GetBandwidth()) ? -1 : ((GetBandwidth() == u2->GetBandwidth()) ? 0 : 1);
      case REMOTE_USER_COLUMN_LOAD:      
      {
         float diff = GetLoadFactor()-u2->GetLoadFactor();
         return (diff > 0.0f) ? 1 : ((diff < 0.0f) ? -1 : 0);
      }
      break;
      case REMOTE_USER_COLUMN_CLIENT: return strcasecmp(GetDisplayClient(), u2->GetDisplayClient());
      default: return 0;
   }
}

float
RemoteUserItem ::
GetLoadFactor() const
{
   if (_maxUploads >= NO_FILE_LIMIT) return  0.0f;
   if (_curUploads >= NO_FILE_LIMIT) return -1.0f;
   return ((float)_curUploads)/((float)_maxUploads);
}

};  // end namespace beshare
