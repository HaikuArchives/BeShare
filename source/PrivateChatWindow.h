#ifndef PRIVATE_CHAT_WINDOW_H
#define PRIVATE_CHAT_WINDOW_H

#include <CheckBox.h>
#include <View.h>
#include <TextControl.h>
#include <Message.h>
#include <Handler.h>

#include "CLVListItem.h"
#include "ColumnListView.h"
#include "SplitPane.h"
#include "ShareWindow.h"

namespace beshare {

class PrivateChatWindow : public ChatWindow
{
public:
	PrivateChatWindow(bool loggingEnabled, const BMessage& msg, uint32 index, ShareWindow* win, const char* defaultStr);
	~PrivateChatWindow();

	virtual void MessageReceived(BMessage* msg);
	virtual void DispatchMessage(BMessage* msg, BHandler* handler);

	void SaveStateTo(BMessage & msg) const;

	uint32 GetIndex() const {return _index;}
	const char * GetTarget() const {return _usersEntry->Text();}

	enum {
		PRIVATE_WINDOW_USER_TEXT_CHANGED = 'pcw0',
		PRIVATE_WINDOW_CLOSED,
		PRIVATE_WINDOW_ADD_USER,
		PRIVATE_WINDOW_REMOVE_USER,
	};

	virtual bool ShowMessageTargets() const {return (_munged == false);}
	virtual bool ShowTimestamps(LogDestinationType d) const;
	virtual bool ShowUserIDs(LogDestinationType d) const;
	virtual bool OkayToLog(LogMessageType messageType, LogDestinationType destType, bool isPrivate) const ;
	virtual void UpdateColors();
	virtual void UpdateTitleBar();

protected:
	virtual status_t DoTabCompletion(const char * origText, String & returnCompletedText, const char * optMatchExpression) const;
	virtual void GetUserNameForSession(const char * sessionID, String & retUserName) const;
	virtual void GetLocalUserName(String & retLocalUserName) const;
	virtual void GetLocalSessionID(String & retLocalSessionID) const;
	virtual status_t ExpandAlias(const String & str, String & ret) const;
	
	virtual void SendChatText(const String & text, ChatWindow * optEchoTo);
	virtual BView * GetChatView() const {return _chatView;}
	virtual String GetQualifiedSharedFileName(const String & name) const;
	virtual void ChatTextReceivedBeep(bool isPersonal, bool mentionsName);
	virtual const char * GetLogFileNamePrefix() const {return "Private";}
	
private:
	static int CompareFunc(const CLVListItem * i1, const CLVListItem * i2, int32 sortKey);

	uint32 _index;
	BView * _chatView;
	ShareWindow * _mainWindow;
	BTextControl * _usersEntry;
	BCheckBox * _logEnabled;
	ColumnListView * _usersList;
	SplitPane * _split;
	bool _munged;
};

};  // end namespace beshare

#endif
