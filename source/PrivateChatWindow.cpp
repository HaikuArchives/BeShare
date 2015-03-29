#include "PrivateChatWindow.h"

#include <Screen.h>
#include <LayoutBuilder.h>

#include "ShareStrings.h"

#include <santa/CLVEasyItem.h>

namespace beshare {

// Window sizing constraints
#define MIN_WIDTH  200
#define MIN_HEIGHT 125
#define MAX_WIDTH  65535
#define MAX_HEIGHT 65535

#define TEXT_ENTRY_HEIGHT 20.0f
#define USER_LIST_WIDTH	125.0f

static BRect ExtractRect(const BMessage & msg);
BRect ExtractRect(const BMessage & msg)
{
	BRect rect;
	if (msg.FindRect("frame", &rect) == B_NO_ERROR)
	{
		BRect screenBounds;
		{
			BScreen s;
			screenBounds = s.Frame();
		}
		if (rect.left > screenBounds.Width()) rect.left = 50;
		if (rect.top > screenBounds.Height()) rect.top = 50;
		return rect;
	}
	else return BRect(30,523,670,700);
}

void
PrivateChatWindow :: UpdateTitleBar()
{
	const String & s = GetCustomWindowTitle();
	SetTitle((s.Length() > 0) ? s() : str(STR_BESHARE_PRIVATE_CHAT));
}

PrivateChatWindow :: PrivateChatWindow(bool loggingEnabled, const BMessage & msg, uint32 idx, ShareWindow * mainWindow, const char * defaultStr) :
	ChatWindow(ExtractRect(msg),str(STR_BESHARE_PRIVATE_CHAT),B_FLOATING_WINDOW_LOOK,B_NORMAL_WINDOW_FEEL,0L),
	_index(idx),
	_mainWindow(mainWindow),
	_munged(false)
{
	SetSizeLimits(MIN_WIDTH, MAX_WIDTH, MIN_HEIGHT, MAX_HEIGHT);

	BMessenger toMe(this);

	_logEnabled = new BCheckBox(NULL, str(STR_LOG), NULL);
	if (loggingEnabled) _logEnabled->SetValue(B_CONTROL_ON);

	// text control for indicating users stretches all the way across the top of the window
	_usersEntry = new BTextControl(NULL, str(STR_CHAT_WITH), defaultStr, new BMessage(PRIVATE_WINDOW_USER_TEXT_CHANGED));
	_usersEntry->SetDivider(_usersEntry->StringWidth(str(STR_CHAT_WITH))+4.0f);
	_usersEntry->SetTarget(toMe);
	_usersEntry->MakeFocus();

	// chat view is the left side of the split...
	_chatView = new BView(NULL, 0);

	// the user list is the right side of the split
	const float ID_WIDTH=24.0f;
	CLVContainerView * cv;
	_usersList = new ColumnListView(BRect(),&cv,NULL,B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE,B_MULTIPLE_SELECTION_LIST,false,false,true,true,B_FANCY_BORDER);

	_usersList->AddColumn(new CLVColumn(str(STR_NAME), _usersList->Bounds().Width()-ID_WIDTH, CLV_SORT_KEYABLE));
	_usersList->AddColumn(new CLVColumn(str(STR_ID), ID_WIDTH, CLV_SORT_KEYABLE|CLV_RIGHT_JUSTIFIED));
	_usersList->SetSortFunction((CLVCompareFuncPtr)CompareFunc);
	BMessage tableMsg;
	if (msg.FindMessage("table", &tableMsg) == B_NO_ERROR)
	{
		int numColumns = _usersList->CountColumns();
		int32 * order = new int32[numColumns];
		for (int i=0; i<numColumns; i++)
		{
			float width;
			if (msg.FindInt32("colorder", i, &order[i]) != B_NO_ERROR) order[i] = i;
			if (msg.FindFloat("colwidth", i, &width) == B_NO_ERROR) _usersList->ColumnAt(i)->SetWidth(width);
		}
		_usersList->SetDisplayOrder(order);
		delete [] order;
	}

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_SMALL_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.SetInsets(0, 0, 0, 0)
			.Add(_usersEntry)
			.Add(_logEnabled)
		.End()
		.AddSplit(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.SetInsets(0, 0, 0, 0)
			.Add(_chatView, 3)
			.Add(cv, 1)
		.End()
	.End();

	float fontSize;
	if (msg.FindFloat("fontsize", &fontSize) == B_NO_ERROR) SetFontSize(fontSize);

	const char * font;
	if (msg.FindString("font", &font) == B_NO_ERROR) SetFont(font, false);
}

PrivateChatWindow :: ~PrivateChatWindow()
{
	BMessage msg(PRIVATE_WINDOW_CLOSED);
	msg.AddPointer("which", this);

	BMessage state;
	SaveStateTo(state);
	msg.AddMessage("state", &state);
	(void) _mainWindow->PostMessage(&msg);
}

void
PrivateChatWindow ::
SaveStateTo(BMessage & msg) const
{
	msg.AddRect("frame", Frame());

	BMessage sp;
	//_split->GetState(sp); // TODO
	msg.AddMessage("split", &sp);
	msg.AddInt32("index", _index);  // not persistent, but needed by ShareWindow

	BMessage table;
	int numColumns = _usersList->CountColumns();
	int32 * order = new int32[numColumns];
	_usersList->GetDisplayOrder(order);
	for (int i=0; i<numColumns; i++)
	{
		msg.AddInt32("colorder", order[i]);
		msg.AddFloat("colwidth", _usersList->ColumnAt(i)->Width());
	}
	delete [] order;

	msg.AddMessage("table", &table);
	msg.AddFloat("fontsize", GetFontSize());
	msg.AddString("font", GetFont()());
}

void PrivateChatWindow :: MessageReceived(BMessage * msg)
{
	switch(msg->what)
	{
		case PRIVATE_WINDOW_USER_TEXT_CHANGED:
		{
			BMessage forward(PRIVATE_WINDOW_USER_TEXT_CHANGED);
			forward.AddString("users", _usersEntry->Text());
			forward.AddPointer("which", this);
			(void) _mainWindow->PostMessage(&forward);
			MakeChatTextFocus();
		}
		break;

		case PRIVATE_WINDOW_ADD_USER:
		{
			const char * id;
			const char * name;
			if ((msg->FindString("name", &name) == B_NO_ERROR)&&
				 (msg->FindString("id",	&id)	== B_NO_ERROR))
			{
				CLVEasyItem * item = new CLVEasyItem;
				item->SetColumnContent(0, name, false);
				item->SetColumnContent(1, id, false, true);
				_usersList->AddItem(item);
				_usersList->SortItems();
			}
		}
		break;

		case PRIVATE_WINDOW_REMOVE_USER:
		{
			const char * id;
			if (msg->FindString("id", &id) != B_NO_ERROR) id = NULL;

			for (int i=_usersList->CountItems()-1; i>=0; i--)
			{
				CLVEasyItem * item = (CLVEasyItem *) _usersList->ItemAt(i);
				if ((id == NULL)||(strcmp(id, item->GetColumnContentText(1)) == 0))
				{
					_usersList->RemoveItem(i);
					delete item;
					if (id) break;  // since we are removing one, and id's are unique....
				}
			}
		}
		break;

		case LOG_REMOTE_USER_CHAT_MESSAGE:
		{
			const char * text, * id;
			if ((msg->FindString("text", &text) == B_NO_ERROR)&&
				 (msg->FindString("sid", &id) == B_NO_ERROR))
			{
				if (IsActive() == false) DoBeep(SYSTEM_SOUND_INACTIVE_CHAT_WINDOW_RECEIVED_TEXT);
				LogMessage(LOG_REMOTE_USER_CHAT_MESSAGE, text, id, &GetColor(COLOR_PRIVATE), true, NULL);
			}
		}
		break;

		default:
			ChatWindow :: MessageReceived(msg);
		break;
	}
}

status_t PrivateChatWindow :: DoTabCompletion(const char * origText, String & returnCompletedText, const char * optMatchExpression) const
{
	status_t ret = B_ERROR;
	if (_mainWindow->Lock())
	{
		ret = _mainWindow->DoTabCompletion(origText, returnCompletedText, optMatchExpression ? optMatchExpression : _usersEntry->Text());
		_mainWindow->Unlock();
	}
	return ret;
}

void PrivateChatWindow :: GetUserNameForSession(const char * sessionID, String & retUserName) const
{
	if (_mainWindow->Lock())
	{
		_mainWindow->GetUserNameForSession(sessionID, retUserName);
		_mainWindow->Unlock();
	}
}

void PrivateChatWindow :: GetLocalUserName(String & retLocalUserName) const
{
	if (_mainWindow->Lock())
	{
		_mainWindow->GetLocalUserName(retLocalUserName);
		_mainWindow->Unlock();
	}
}

void PrivateChatWindow :: GetLocalSessionID(String & retLocalSessionID) const
{
	if (_mainWindow->Lock())
	{
		_mainWindow->GetLocalSessionID(retLocalSessionID);
		_mainWindow->Unlock();
	}
}


bool PrivateChatWindow :: OkayToLog(LogMessageType /*messageType*/, LogDestinationType destType, bool /*isPrivate*/) const
{
	switch(destType)
	{
		case DESTINATION_DISPLAY:  return true;
		case DESTINATION_LOG_FILE: return (_logEnabled->Value() == B_CONTROL_ON);
		default:						 return false;
	}
}


bool PrivateChatWindow :: ShowTimestamps(LogDestinationType dest) const
{
	bool ret = false;
	if (_mainWindow->Lock())
	{
		ret = _mainWindow->ShowTimestamps(dest);
		_mainWindow->Unlock();
	}
	return ret;
}

bool PrivateChatWindow :: ShowUserIDs(LogDestinationType dest) const
{
	bool ret = false;
	if (_mainWindow->Lock())
	{
		ret = _mainWindow->ShowUserIDs(dest);
		_mainWindow->Unlock();
	}
	return ret;
}

void PrivateChatWindow :: SendChatText(const String & text, ChatWindow *)
{
	String altText;
	bool useAltText = false;

	if ((text.StartsWith("/"))&&(!text.StartsWith("//")))
	{
		_munged = false;
		// We don't want private /actions to be seen by everyone, so
		// we'll transform them into /msg strings!
		int actionKeywordChars = 0;
			  if (text.StartsWith("/me "))	  actionKeywordChars = 4;
		else if (text.StartsWith("/me's "))	actionKeywordChars = 3;
		else if (text.StartsWith("/action ")) actionKeywordChars = 8;
		else if (text.StartsWith("/clear"))
		{
			ClearChatLog();
			return;
		}
		else if (text.StartsWith("/fontsize"))
		{
			SetFontSize(text);
			return;
		}
		else if (text.StartsWith("/font"))
		{
			SetFont(text, true);
			return;
		}

		if (actionKeywordChars > 0)
		{
			useAltText = true;
			altText = text.Substring(actionKeywordChars);

			String pre("/msg ");

			String target(_usersEntry->Text());
			target.Replace(' ', CLUMP_CHAR);  // keep spaces in target str from being interpreted as delimiters
			pre += target;
			pre += ' ';

			String myName;
			GetLocalUserName(myName);
			pre += myName;
			pre += ' ';
			altText = pre.Append(altText);
		}
	}
	else
	{
		String target(_usersEntry->Text());
		target = target.Trim();

		String temp("/msg ");
		target.Replace(' ', CLUMP_CHAR);  // keep spaces in target str from being interpreted as delimiters
		temp += target;
		temp += ' ';
		useAltText = true;
		altText = temp.Append(text);
		_munged = true;
	}
	if (_mainWindow->Lock())
	{
		_mainWindow->SendChatText(useAltText ? altText : text, this);
		_mainWindow->Unlock();
	}
}

status_t
PrivateChatWindow ::
ExpandAlias(const String & str, String & retStr) const
{
	status_t ret = B_ERROR;
	if (_mainWindow->Lock())
	{
		ret = _mainWindow->ExpandAlias(str, retStr);
		_mainWindow->Unlock();
	}
	return ret;
}

int
PrivateChatWindow ::
CompareFunc(const CLVListItem* item1, const CLVListItem* item2, int32 sortKey)
{
	CLVEasyItem * e1 = (CLVEasyItem *) item1;
	CLVEasyItem * e2 = (CLVEasyItem *) item2;
	switch(sortKey)
	{
		case 0:  return strcasecmp(e1->GetColumnContentText(0), e2->GetColumnContentText(0));
		case 1:  return atoi(e1->GetColumnContentText(1))-atoi(e2->GetColumnContentText(1));
	}
	return 0;
}

void
PrivateChatWindow ::
DispatchMessage(BMessage * msg, BHandler * handler)
{
	switch(msg->what)
	{
		case B_MOUSE_DOWN:
			if (handler == _usersList) _usersList->MakeFocus();
		break;

		case B_KEY_DOWN:
		{
			int8 key;
			int32 mods;
			int8 sc = shortcut(SHORTCUT_CLEAR_CHAT_LOG);
			if ((msg->FindInt8("byte", &key) == B_NO_ERROR)&&(msg->FindInt32("modifiers", &mods) == B_NO_ERROR)&&((key == sc)||(key == sc +('a'-'A')))&&((mods & B_CONTROL_KEY)||(mods & B_COMMAND_KEY)))
			{
				ClearChatLog();
				msg = NULL;
			}
		}
		break;
	}
	if (msg) ChatWindow::DispatchMessage(msg, handler);
}

String
PrivateChatWindow ::
GetQualifiedSharedFileName(const String & name) const
{
	if (_mainWindow->Lock())
	{
		String ret = _mainWindow->GetQualifiedSharedFileName(name);
		_mainWindow->Unlock();
		return ret;
	}
	else return name;
}

void
PrivateChatWindow ::
ChatTextReceivedBeep(bool, bool)
{
	DoBeep(SYSTEM_SOUND_PRIVATE_MESSAGE_WINDOW);
}

void
PrivateChatWindow ::
UpdateColors()
{
	ChatWindow::UpdateColors();

	UpdateColumnListViewColors(_usersList);
	UpdateTextViewColors(_usersEntry->TextView());
}

};  // end namespace beshare
