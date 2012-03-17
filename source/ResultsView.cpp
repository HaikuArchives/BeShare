#include "ResultsView.h"

#include <PopUpMenu.h>
#include <MenuItem.h>
#include <Window.h>
#include <MimeType.h>

#include "util/String.h"

#include "ShareStrings.h"
#include "ShareWindow.h"
#include "RemoteUserItem.h"

namespace beshare {
	
static String RemoveSpecialQueryChars(const String & localString)
{
   String s = localString;
   s.Replace(' ', '?');
   s.Replace('@', '?');
   s.Replace('/', '?');
   s.Replace(',', '?');
   return s;
}


ResultsView::ResultsView(uint32 replyWhat, BRect frame, 
	CLVContainerView** containerView, const char* name, uint32 resizingMode, 
	uint32 flags, list_view_type type, bool hierarchical, bool horizontal, 
	bool vertical, bool scroll_view_corner, border_style border, 
	const BFont* labelFont) 
	: ColumnListView(frame, containerView, name, resizingMode, flags, 
		type, hierarchical, horizontal, vertical, scroll_view_corner, border, 
		labelFont),
	fReplyWhat(replyWhat)
{
#if SAVE_BEOS
	fSbe = NULL;
	BResources * rsrc = be_app->AppResources();
	
	if (rsrc) {
		size_t bitSize;
		const void * bits = rsrc->LoadResource('PNG ', "beshare_320x200.png", 
			&bitSize);
         
		if (bits) {
			BMemoryIO mio(bits, bitSize);
			fSbe = BTranslationUtils::GetBitmap(&mio);
		}
	}
#endif
}


ResultsView::~ResultsView()
{
#if SAVE_BEOS
	delete fSbe;
#endif
}


void 
ResultsView::MouseDown(BPoint where)
{
	BPoint pt;
	ulong buttons;

	GetMouse(&pt, &buttons);
	
	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		int numPages = ((ShareWindow*)Window())->GetNumResultsPages();
		if (numPages > 1) {
			int currentPage = ((ShareWindow*)Window())->GetCurrentResultsPage();
            BPopUpMenu * popup = new BPopUpMenu((const char *)NULL);
            for (int i = 0; i < numPages; i++) {
               char temp[128];
               sprintf(temp, "%s %i", str(STR_SWITCH_TO_PAGE), i+1);
               BMessage * msg = new BMessage(fReplyWhat);
               msg->AddInt32("page", i);
               BMenuItem * mi = new BMenuItem(temp, msg);
               mi->SetEnabled(i != currentPage);
               popup->AddItem(mi);
			}
            ConvertToScreen(&pt);
            BMenuItem * result = popup->Go(pt);
            
            if (result) 
            	Window()->PostMessage(result->Message());
            
            delete popup;
			return;
		}
	} else 
      	ColumnListView::MouseDown(where);
}


bool
ResultsView::InitiateDrag(BPoint /*point*/, int32 /*index*/, bool /*wasSelected*/)
{
	BMessage dragMessage(B_SIMPLE_DATA);
	BMessage dragData;
	BRect rect;
	BRect bounds = Bounds();

	dragMessage.AddInt32("be:actions", B_MOVE_TARGET);
	dragMessage.AddString("be:types", B_FILE_MIME_TYPE);

	for(int i = 0; ;i++) {
		int32 selindex = CurrentSelection(i);
		
		if (selindex < 0)
			break;

		const RemoteFileItem * item = (const RemoteFileItem *)ItemAt(selindex);
		dragData.AddPointer("item", item);
        // For each item, we also add a 'URL' that fully describes the file
        // The URL is of the form:
        //     beshare://UserIP:UserPort/InstallID@BeShareServer/filename

        // The idea is that an application that understands this format will
        // first try UserIP and UserPort to set up a direct connection, and
        // if that doesn't work (because the remote user is firewalled for
        // example), can use InstallID and BeShareServer to set up a callback
        // session.  -- marco

        RemoteUserItem * owner = item->GetOwner();
        uint64 ID = owner->GetInstallID();
        char strbuf[17];
        sprintf(strbuf,"%Lx", ID);

        String URL;
        URL << "beshare://" 
			<< owner->GetHostName() << ":" << ((owner->GetFirewalled()) ? 0 : owner->GetPort())
            << "/" << strbuf << "@" << ((ShareWindow*)Window())->GetConnectedTo()
            << "/" << item->GetFileName();
        dragMessage.AddString("be:url", URL());

        BRect itemrect = ItemFrame(selindex);
        
        if (itemrect.Intersects(bounds)) {
            if (itemrect.IsValid()) {
				if (rect.IsValid()) 
					rect = rect | itemrect;
				else
					rect = itemrect;
            }
		}
	}

    // Let's also put in a BeShare-friendly link-text, in case 
    // the user drops the Message back into BeShare, again.
    {
    	String ownerString, fileString, humanReadableString = "[";
        
		for(int j = 0; ;j++) {
			int32 selindex = CurrentSelection(j);
            
            if (selindex < 0)
            	break;

            const RemoteFileItem * item = (const RemoteFileItem *)ItemAt(selindex);
            
            if (humanReadableString.Length() > 1)
            	humanReadableString += ", ";
            
            humanReadableString += item->GetFileName();
            fileString += (fileString.Length() == 0) ? "beshare:" : ",";

            String fn(item->GetFileName());
            EscapeRegexTokens(fn);

            fileString += RemoveSpecialQueryChars(fn);
            
            if (ownerString.Length() > 0)
            	ownerString += ',';
            
            ownerString += RemoveSpecialQueryChars(item->GetOwner()->GetSessionID());
         }
         
         if (fileString.Length() > 0) {
            
            if (ownerString.Length() > 0)
            	fileString += ownerString.Prepend("@");
            
            dragMessage.AddString("beshare:link", fileString());

            humanReadableString += ']';
            dragMessage.AddString("beshare:desc", humanReadableString());
         }
      }

      dragMessage.AddMessage("be:originator-data", &dragData);
      
      if (rect.IsValid())
      	DragMessage(&dragMessage, rect, Window());

      return true;
   }


#ifdef SAVE_BEOS
virtual void 
ResultsView::Draw(BRect ur)
{
	if ((fSbe) && (CountItems() == 0))
		DrawBitmapAsync(fSbe, ur, ur);

	ColumnListView::Draw(ur);
}


virtual bool
ResultsView::AddItem(BListItem *item)
{
	bool ret = ColumnListView::AddItem(item);
	
	if ((ret) && (CountItems() == 1))
		Invalidate();
	
	return ret;
}


virtual bool
ResultsView::AddItem(BListItem *item, int32 atIndex)
{
	bool ret = ColumnListView::AddItem(item, atIndex);
	
	if ((ret) && (CountItems() == 1))
		Invalidate();
	
	return ret;
}


virtual bool
ResultsView::AddList(BList *newItems)
{
	bool inv = (CountItems() == 0);
	bool ret = ColumnListView::AddList(newItems);

	if ((ret) && (inv))
		Invalidate();

	return ret;
}


virtual bool
ResultsView::AddList(BList *newItems, int32 atIndex)
{
	bool inv = (CountItems() == 0);
	bool ret = ColumnListView::AddList(newItems, atIndex);
	
	if ((ret) && (inv))
		Invalidate();
	
	return ret;
}


bool
ResultsView::RemoveItem(BListItem *item)
{
	bool ret = ColumnListView::RemoveItem(item);
	
	if ((ret) && (CountItems() == 0))
		Invalidate();

	return ret;
}


BListItem*
ResultsView::RemoveItem(int32 index)
{
	BListItem * ret = ColumnListView::RemoveItem(index);
	
	if ((ret) && (CountItems() == 0))
		Invalidate();

	return ret;
}


bool
ResultsView::RemoveItems(int32 index, int32 count)
{
	bool ret = ColumnListView::RemoveItems(index, count);
	
	if ((ret) && (CountItems() == 0))
		Invalidate();
	
	return ret;
}


void
ResultsView::MakeEmpty()
{
	bool inv = (CountItems() == 0);
	ColumnListView::MakeEmpty();
	
	if (inv)
		Invalidate();
}
#endif

};  // end namespace beshare
