
#include "TransferListView.h"

#include <PopUpMenu.h>
#include <MenuItem.h>
#include <Window.h>
#include <Alert.h>
#include <Button.h>
#include <Screen.h>
#include <PopUpMenu.h>
#include <File.h>
#include <Path.h>
#include <FindDirectory.h>
#include <NodeMonitor.h>
#include <Resources.h>
#include <Roster.h>

#include <Beep.h>

#include <BitmapStream.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>
#include <TranslatorFormats.h>

#include <santa/CLVColumnLabelView.h>
#include <santa/CLVColumn.h>

#include "util/StringTokenizer.h"
#include "util/Socket.h"
#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "message/Message.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/PathMatcher.h"
#include "util/NetworkUtilityFunctions.h"
#include "iogateway/PlainTextMessageIOGateway.h"

#include "ShareConstants.h"
#include "ShareStrings.h"
#include "ShareUtils.h"
#include "ShareWindow.h"
#include "ShareFileTransfer.h"

namespace beshare {

TransferListView::TransferListView(BRect rect, uint32 banCommand)
	: BListView(rect, NULL, B_MULTIPLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES,
		B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE|B_FULL_UPDATE_ON_RESIZE)
	, fBanCommand(banCommand)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::TransferListView begin\n"));
	SetLowColor(B_TRANSPARENT_32_BIT);// we'll draw in the background, thanks
	SetViewColor(B_TRANSPARENT_32_BIT); // we'll draw in the background, thanks
	TRACE_TRANSFERLISTVIEW(("TransferListView::TransferListView begin\n"));
}


void
TransferListView::MessageReceived(BMessage * msg)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::MessageReceived begin\n"));
	BListView::MessageReceived(msg);
	BMessage downloads;
	if (msg->FindMessage("be:originator-data", &downloads) == B_NO_ERROR) {
		ShareWindow * win = (ShareWindow *) Window();
		win->RequestDownloads(downloads, win->_downloadsDir, NULL);
	}
	TRACE_TRANSFERLISTVIEW(("TransferListView::MessageReceived begin\n"));
}


void
TransferListView::Draw(BRect ur)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::Draw begin\n"));
	BRect backgroundArea = ur;

	int numItems = CountItems();

	if (numItems > 0)
		backgroundArea.top = ItemFrame(CountItems() - 1).bottom + 1.0f;

	if (ur.Intersects(backgroundArea)) {
		SetHighColor(((ShareWindow*)Window())->GetColor(COLOR_BORDERS));
		FillRect(backgroundArea & ur);
	}

	BListView::Draw(ur);
	TRACE_TRANSFERLISTVIEW(("TransferListView::Draw end\n"));
}


void
TransferListView::MouseDown(BPoint where)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::MouseDown begin\n"));
	BPoint pt;
	ulong buttons;

	GetMouse(&pt, &buttons);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {

		// no multiple selection? select what's under the mouse
		if (CurrentSelection(1) < 0)
			Select(IndexOf(pt));

		if (CurrentSelection() >= 0) {
			ShareWindow * win = (ShareWindow *) Window();
			int idx;

			BPopUpMenu* popup = new BPopUpMenu((const char *)NULL);

			BMenuItem * moveTop = new BMenuItem(str(STR_MOVE_TO_TOP), NULL);
			popup->AddItem(moveTop);

			BMenuItem * moveUp = new BMenuItem(str(STR_MOVE_UP), NULL);
			popup->AddItem(moveUp);

			BMenuItem * moveDown = new BMenuItem(str(STR_MOVE_DOWN), NULL);
			popup->AddItem(moveDown);

			BMenuItem * moveBottom =
				new BMenuItem(str(STR_MOVE_TO_BOTTOM), NULL);

			popup->AddItem(moveBottom);

			popup->AddSeparatorItem();

 			// just for our temporary use
			static const type_code OPEN_FILE = 'OpFi';

			// just for our temporary use
			static const type_code OPEN_FOLDER = 'OpFo';

			Hashtable<uint32, bool> canBans;
			bool haltEnabled = false, resumeEnabled = false;

			for (int h=0; (idx = CurrentSelection(h)) >= 0; h++) {
				ShareFileTransfer * xfr = (ShareFileTransfer*)ItemAt(idx);

				if (xfr->IsUploadSession()) {
					uint32 rip = xfr->GetRemoteIP();

					if (rip > 0)
						canBans.Put(rip, true);

					// For uploaders, 'restart download' means 'start upload now'
					// and 'halt download' means 'go back to queued mode'

					if (xfr->IsWaitingOnLocal())
						resumeEnabled = true;
					else
						haltEnabled = true;
 				} else {
					// For downloaders, 'restart download' means 'reconnect'
					// if we failed, or 'force start now' if we are waiting
					// for the local queues to free up.
					resumeEnabled = (xfr->IsFinished()) ?
						(xfr->GetOriginalFileSet().GetNumItems() > 0) :
						xfr->IsWaitingOnLocal();

					haltEnabled = ((xfr->IsWaitingOnLocal())
						|| (xfr->IsWaitingOnRemote())
						|| (xfr->IsConnected())
						|| (xfr->IsConnecting())
						|| (xfr->IsAccepting()));
 				}
			}

			BMenuItem * haltDownload = new BMenuItem(str(STR_HALT_DOWNLOAD), NULL);
			haltDownload->SetEnabled(haltEnabled);
			popup->AddItem(haltDownload);

			BMenuItem * resumeDownload = new BMenuItem(str(STR_RESTART_DOWNLOAD), NULL);
			resumeDownload->SetEnabled(resumeEnabled);
			popup->AddItem(resumeDownload);

			BMenu * limitMenu = new BMenu(str(STR_LIMIT_BANDWIDTH));
			{
				ShareFileTransfer * xfr = (ShareFileTransfer *) ItemAt(CurrentSelection(0));
				uint32 currentLimit = xfr ? xfr->GetBandwidthLimit() : 0;
				uint32 prevVal = 0;

				popup->AddItem(limitMenu);
				_AddLimitItem(limitMenu, 0, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 1, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 2, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 3, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 5, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 10, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 20, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 50, currentLimit, prevVal);
				_AddLimitItem(limitMenu, 100, currentLimit, prevVal);
			}
			popup->AddSeparatorItem();

			BMenu* filesList = new BMenu(str(STR_OPEN_FILE));
			BMenu* foldersList = new BMenu(str(STR_OPEN_FOLDER));
			{
				for (int h = 0; (idx = CurrentSelection(h)) >= 0; h++) {
					if (filesList->CountItems() > 0)
						filesList->AddSeparatorItem();

					if (foldersList->CountItems() > 0)
						foldersList->AddSeparatorItem();

					ShareFileTransfer * xfr = (ShareFileTransfer*)ItemAt(idx);
					_AddFileItems(filesList, OPEN_FILE, xfr);
					_AddFileItems(foldersList, OPEN_FOLDER, xfr);
 				}
			}
			popup->AddItem(filesList);
			popup->AddItem(foldersList);

			popup->AddSeparatorItem();

			BMenuItem * removeItems = new BMenuItem(str(STR_REMOVE_SELECTED), NULL);
			popup->AddItem(removeItems);

			if (canBans.GetNumItems() > 0) {
				popup->AddSeparatorItem();
 				BMenu * banUser = new BMenu(str(STR_BAN_USER_FOR));
 				popup->AddItem(banUser);

 				bigtime_t minute = 60 * 1000000;
				_AddBanItem(banUser, canBans, 5,str(STR_MINUTES), minute);
 				_AddBanItem(banUser, canBans, 15, str(STR_MINUTES), minute);
 				_AddBanItem(banUser, canBans, 30, str(STR_MINUTES), minute);

 				bigtime_t hour = 60 * minute;
 				_AddBanItem(banUser, canBans, 1,str(STR_HOURS), hour);
 				_AddBanItem(banUser, canBans, 2,str(STR_HOURS), hour);
 				_AddBanItem(banUser, canBans, 5,str(STR_HOURS), hour);
 				_AddBanItem(banUser, canBans, 12, str(STR_HOURS), hour);

 				bigtime_t day = 24 * hour;
 				_AddBanItem(banUser, canBans, 1, str(STR_DAYS), day);
 				_AddBanItem(banUser, canBans, 2, str(STR_DAYS), day);
 				_AddBanItem(banUser, canBans, 7, str(STR_DAYS), day);
 				_AddBanItem(banUser, canBans, 30,str(STR_DAYS), day);

 				_AddBanItem(banUser, canBans, -1,str(STR_FOREVER), -1);
			}

			ConvertToScreen(&pt);

			BMenuItem * result = popup->Go(pt);
			BMessage * rMsg = result ? result->Message() : NULL;

			if (rMsg) {
 				entry_ref er;
 				if (rMsg->FindRef("entry", &er) == B_NO_ERROR) {
					switch(rMsg->what) {
 						case OPEN_FILE:
							be_roster->Launch(&er);
 							break;
 						case OPEN_FOLDER: {
							node_ref tempRef;
							tempRef.device = er.device;
							tempRef.node = er.directory;
							BDirectory dir(&tempRef);
							win->OpenTrackerFolder(dir);
 						} break;
					}
 				}
			}

			if (result == moveUp)
				MoveSelectedItems(-1);
			else if (result == moveDown)
				MoveSelectedItems(1);
			else if (result == moveTop)
				MoveSelectedToExtreme(-1);
			else if (result == moveBottom)
				MoveSelectedToExtreme(1);
			else if (result == haltDownload) {
				for (int i = 0; (idx = CurrentSelection(i)) >= 0; i++) {
					ShareFileTransfer* xfr = (ShareFileTransfer *) ItemAt(idx);
					if (xfr->IsUploadSession()) {
						if (xfr->IsWaitingOnLocal() == false)
							xfr->RequeueTransfer();
							xfr->SetBeginTransferEnabled(false);
						}else
							xfr->AbortSession(true, true);
				}
				win->DequeueTransferSessions();
			} else if (result == resumeDownload) {
				for (int i = 0; (idx = CurrentSelection(i)) >= 0; i++) {
					ShareFileTransfer * xfr = (ShareFileTransfer *) ItemAt(idx);
					if (xfr->IsUploadSession()) {
						if (xfr->IsWaitingOnLocal()) {
							if (xfr->GetBeginTransferEnabled())
								xfr->BeginTransfer();
 							else xfr->SetBeginTransferEnabled(true);
						}
					} else {
						if (xfr->IsWaitingOnLocal())
							xfr->BeginTransfer();
						else xfr->RestartSession();
					}
				}
				win->DequeueTransferSessions();
			} else if (result == removeItems)
				Window()->PostMessage(ShareWindow::SHAREWINDOW_COMMAND_CANCEL_DOWNLOADS);
			else if (result) {
				BMessage * m = result->Message();
				if (m) {
					if (m->what == fBanCommand)
						Window()->PostMessage(m);
					else if (m->what == LIMIT_BANDWIDTH_COMMAND) {
						uint32 limit;
						if (m->FindInt32("limit", (int32*)&limit) == B_NO_ERROR)
							for (int i = 0; (idx = CurrentSelection(i)) >= 0; i++)
								((ShareFileTransfer *) ItemAt(idx))->SetBandwidthLimit(limit);
					}
				}
			}

			delete popup;
			Invalidate();
 		}
	} else
		BListView::MouseDown(where);
	TRACE_TRANSFERLISTVIEW(("TransferListView::MouseDown end\n"));
}


void
TransferListView::MoveSelectedItems(int delta)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::MoveSelectedItems begin\n"));
	// First, identify our movers by value...
	BList movers;
	{
		int idx;
		for (int i = 0; (idx = CurrentSelection(i)) >= 0; i++)
			movers.AddItem(ItemAt(idx));
	}

	DeselectAll();

	// Now move each one...
	int numItems = CountItems();
	int numMovers = movers.CountItems();
	for (int j = (delta < 0) ? 0 : (numMovers-1); (delta < 0)?(j < numMovers):(j >= 0); j -= delta) {
		int oldIdx = IndexOf((BListItem*)movers.ItemAt(j));
		int newIdx = oldIdx+delta;

		if (newIdx >= numItems)
			newIdx = numItems-1;

		if (newIdx < 0)
			newIdx = 0;

		if ((newIdx != oldIdx) && (oldIdx >= 0) && (movers.IndexOf(ItemAt(newIdx))==-1))
			SwapItems(oldIdx, newIdx);
	}

	// And reselect
	for (int k = 0; k<numMovers; k++) {
		int32 idx = IndexOf((BListItem*)movers.ItemAt(k));
		if (idx >= 0) Select(idx, true);
	}
	TRACE_TRANSFERLISTVIEW(("TransferListView::MoveSelectedItems end\n"));
}


void
TransferListView::MoveSelectedToExtreme(int dir)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::MoveSelectedToExtreme begin\n"));
	// First, Make a list of selected items and unselected items
	BList selected, unselected;
	{
		int32 numItems = CountItems();
		for (int i = 0; i < numItems; i++) {
			BListItem * next = ItemAt(i);
			if (next->IsSelected())
				selected.AddItem(next);
			else
				unselected.AddItem(next);
		}
	}

	DeselectAll();
	MakeEmpty();

	if (dir > 0) {
		AddList(&unselected);
		AddList(&selected);
		Select(CountItems()-selected.CountItems(), CountItems()-1);
	} else {
		AddList(&selected);
		AddList(&unselected);
		Select(0, selected.CountItems()-1);
	}
	TRACE_TRANSFERLISTVIEW(("TransferListView::MoveSelectedToExtreme end\n"));
}


void
TransferListView::_AddFileItems(BMenu * menu, type_code tc, const ShareFileTransfer * xfr)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::_AddFileItems begin\n"));
	ShareWindow* win = (ShareWindow *) Looper();
	String next;

	for (HashtableIterator<String, OffsetAndPath> iter(xfr->GetDisplayFileSet().GetIterator()); iter.HasData(); iter++) {
		next = iter.GetKey();

		BMenuItem * mi = new BMenuItem(next.Cstr(), new BMessage(tc));
		bool enableIt = false;

		if (xfr->IsUploadSession()) {
			entry_ref er = win->FindSharedFile(next.Cstr());

			if (BEntry(&er).Exists())
				enableIt = (mi->Message()->AddRef("entry", &er) == B_NO_ERROR);
		} else {
			String path = iter.GetValue()._path;

			if (path.Length() > 0)
				path += '/';

			BEntry entry(&win->_downloadsDir, (path+(next))(), true);
			entry_ref er;

			if ((entry.Exists()) && (entry.GetRef(&er) == B_NO_ERROR))
				enableIt = (mi->Message()->AddRef("entry", &er) == B_NO_ERROR);
		}
		mi->SetEnabled(enableIt);
		menu->AddItem(mi);
	}
	TRACE_TRANSFERLISTVIEW(("TransferListView::_AddFileItems end\n"));
}


void
TransferListView::_AddLimitItem(BMenu * addTo, uint32 transferRate, uint32 currentLimit, uint32 & prevVal)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::_AddLimitItem begin\n"));
	char buf[128];
	if (transferRate > 0) {
		sprintf(buf, "%luKB%s", transferRate, str(STR_SEC));
		char * comma = strchr(buf, ','); if (comma) *comma = '\0';
	} else
		strcpy(buf, str(STR_NO_LIMIT));

	transferRate *= 1024;// convert into bytes
	BMessage * msg = new BMessage(LIMIT_BANDWIDTH_COMMAND);
	msg->AddInt32("limit", transferRate);

	BMenuItem * mi = new BMenuItem(buf, msg);

	if ((currentLimit == transferRate) || ((prevVal < currentLimit) && (transferRate > currentLimit)))
		mi->SetMarked(true);

	addTo->AddItem(mi);
	prevVal = transferRate;
	TRACE_TRANSFERLISTVIEW(("TransferListView::_AddLimitItem end\n"));
}


void
TransferListView::_AddBanItem(BMenu* addTo, const Hashtable<uint32, bool>& canBans, int count, const char* unit, bigtime_t microsPerUnit)
{
	TRACE_TRANSFERLISTVIEW(("TransferListView::_AddBanItem begin\n"));
	char buf[128];
	if (count > 0)
		sprintf(buf, "%i %s", count, unit);
	else
		strcpy(buf, unit);

	BMessage * msg = new BMessage(fBanCommand);
	HashtableIterator<uint32,bool> iter = canBans.GetIterator();
	uint32 nextKey;

	while((nextKey = iter.GetKey()) == B_NO_ERROR)
		msg->AddInt32("ip", nextKey);

	msg->AddString("durstr", buf);

	if (microsPerUnit > 0)
		msg->AddInt64("duration", (count >= 0) ? count*microsPerUnit : 0);

	addTo->AddItem(new BMenuItem(buf, msg));
	TRACE_TRANSFERLISTVIEW(("TransferListView::_AddBanItem end\n"));
}

};// end namespace beshare
