#ifndef CHAT_WINDOW_H
#define CHAT_WINDOW_H

#include <app/Message.h>
#include <interface/Window.h>
#include <interface/TextControl.h>
#include <interface/TextView.h>
#include <storage/Directory.h>

#include "util/Queue.h"
#include "util/String.h"

#include "ShareConstants.h"
#include "ReflowingTextView.h"

class ColumnListView;

namespace beshare {

enum
{
   COLOR_BG = 0,
   COLOR_SCROLLBG,
   COLOR_BORDERS,
   COLOR_TEXT,
   COLOR_SYSTEM,
   COLOR_WARNING,
   COLOR_ERROR,
   COLOR_ACTION,
   COLOR_PRIVATE,
   COLOR_NAMESAID,
   COLOR_PING,
   COLOR_LOCAL,
   COLOR_REMOTE,
   COLOR_URL,
   COLOR_SELECTION,
   COLOR_WATCH,
   COLOR_DOWNLOAD,
   COLOR_UPLOAD,
   COLOR_PAUSEDUPLOAD,
   NUM_COLORS
};

/* This is a semi-abstract base class; it defines a window that can be used for chat. */
/* Subclasses are ShareWindow (the main BeShare window) and PrivateChatWindow. */
class ChatWindow : public BWindow
{
public:
   ChatWindow(BRect defPos, const char * title, window_look look, window_feel feel, uint32 flags);
   ~ChatWindow();

   virtual void MessageReceived(BMessage * msg);

   virtual void DispatchMessage(BMessage * msg, BHandler * target);

   virtual void LogHelp(const char * cmd, int tokenID, int descID, ChatWindow * optEchoTo = NULL);
   virtual void LogMessage(LogMessageType type, const char * text, const char * optSessionID=NULL, const rgb_color * optTextColor = NULL, bool isPersonal = false, ChatWindow * optEchoTo = NULL);

   virtual bool ShowMessageTargets() const = 0;
   virtual bool ShowTimestamps(LogDestinationType d) const = 0;
   virtual bool ShowUserIDs(LogDestinationType d) const = 0;
   virtual bool OkayToLog(LogMessageType messageType, LogDestinationType destType, bool isPrivate) const = 0;
   virtual void UpdateColors();
   virtual void ReadyToRun();  // called right after ctor

#ifdef B_BEOS_VERSION_DANO
   virtual status_t UISettingsChanged(const BMessage* changes, uint32 flags);
#endif

   virtual void UpdateTitleBar() = 0;

   void SetCommandURLTarget(const BMessenger & target, const BMessage & queryMessage, const BMessage & privMessage);

   void DoBeep(const char * which) const;

   const rgb_color & GetColor(uint32 which, int forceDelta=0) const;

   enum {
      CHATWINDOW_COMMAND_BASE = 'cwc0',
      CHATWINDOW_COMMAND_SEND_CHAT_TEXT,
      CHATWINDOW_COMMAND_SET_COMMAND_TARGET,
      CHATWINDOW_COMMAND_SET_CUSTOM_TITLE,
      CHATWINDOW_COMMAND_INVALIDATE_TEXT_VIEW,
      CHATWINDOW_COMMAND_UPDATE_COLORS,
      CHATWINDOW_COMMAND_COLOR_CHANGED,
      CHATWINDOW_COMMAND_REQUEST_COLOR,
      CHATWINDOW_COMMAND_REQUEST_DEFAULT_COLOR
   };

protected:
   void ClearChatLog() {_chatText->Clear();}

   virtual status_t DoTabCompletion(const char * origText, String & returnCompletedText, const char * optMatchExpression) const = 0;
   virtual void GetUserNameForSession(const char * sessionID, String & retUserName) const = 0;
   virtual void GetLocalUserName(String & retLocalUserName) const = 0;
   virtual void GetLocalSessionID(String & retLocalSessionID) const = 0;
   virtual status_t ExpandAlias(const String & str, String & ret) const = 0;
   virtual void SendChatText(const String & text, ChatWindow * optEchoTo) = 0;
   virtual BView * GetChatView() const = 0;
   virtual String GetQualifiedSharedFileName(const String & name) const = 0;
   
   void MakeChatTextFocus() {_chatText->MakeFocus();}

   virtual void InsertChatText(LogDestinationType dest, const char * text, int textLen, text_run_array * optStyle);
   virtual void UserChatted();
   virtual void ChatTextReceivedBeep(bool isPersonal, bool mentionsName);

   void SetCustomWindowTitle(const String & title) {_customWindowTitle = title;}
   const String & GetCustomWindowTitle() const {return _customWindowTitle;}

   virtual const char * GetLogFileNamePrefix() const = 0;

   void SetFont(const String & fontStr, bool doLog);
   const String & GetFont() const {return _fontName;}

   void SetFontSize(const String & cmdString);
   void SetFontSize(float fs) {_fontSize = fs;}
   float GetFontSize() const {return _fontSize;}

   bool IsScrollBarNearBottom() const;

   void SetColor(uint32 which, const rgb_color & c);

   void UpdateTextViewColors(BTextView * view);
   void UpdateColumnListViewColors(ColumnListView * clv);
   void SetViewColor(BView * view, const rgb_color & color);
   bool ColorsDiffer(const rgb_color & c1, const rgb_color & c2) const;
   void SetCustomColorsEnabled(bool cce) {_customColorsEnabled = cce;}
   bool GetCustomColorsEnabled() const {return _customColorsEnabled;}

   BView * AddBorderView(BView * view);
   status_t GetAppSubdir(const char * subDirName, BDirectory & setSubDirRef, bool createIfNecessary) const;
   void CloseLogFile();
   const BDirectory & GetLogsDir() const {return _logsDir;}

private:
   void InsertChatTextAux(const char * text, int textLen, text_run_array * optStyle);
   void ScrollToBottom();
   void ScrollToTop();
   String ProcessURL(const String & url) const;
   void InsertDroppedText(const char * u);
   int32 FindMatchingBracket(const String & s, uint32 & count) const;

   Queue<String> _chatHistory;  // last <n> Items that you previously typed in
   int _chatHistoryPosition;    // -1, or index into (_chatHistory)
   bool _isInitialized;
   bool _isScrolling;           // don't ScrollToBottom() when user is scrolling

   String _customWindowTitle;
   String _fontName;
   float _fontSize;
   rgb_color _colors[NUM_COLORS]; 

   ReflowingTextView * _chatText;
   BTextControl * _textEntry;
   BScrollView * _chatScrollView;

   Queue<BView *> _borderViews;
   bool _customColorsEnabled;

   BDirectory _logsDir;
   FILE * _logFile;

   BFont _customPlainFont;
   BFont _customBoldFont;
};

};  // end namespace beshare

#endif
