
#include "UserListView.h"

#include <PopUpMenu.h>
#include <MenuItem.h>
#include <Window.h>

#include "util/String.h"

#include "ShareStrings.h"
#include "ShareWindow.h"
#include "RemoteUserItem.h"

namespace beshare {
UserListView::UserListView(uint32 replyWhat, BRect frame, 
		CLVContainerView** containerView, const char* name, uint32 resizingMode, 
		uint32 flags, list_view_type type, bool hierarchical, bool horizontal,
		bool vertical, bool scroll_view_corner, border_style border, 
		const BFont* labelFont) 
   		: ColumnListView(frame, containerView, name, resizingMode, flags, type, 
   			hierarchical, horizontal, vertical, scroll_view_corner, border, 
   			labelFont),
   		fReplyWhat(replyWhat)
{

}


void
UserListView::MouseDown(BPoint where)
{
	BPoint pt;
	ulong buttons;
	
	GetMouse(&pt, &buttons);
	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		String handles, sessionIDs;
		
		if (CurrentSelection(1) < 0) 
			Select(IndexOf(pt)); // no multiple selection? select what's under the mouse
         
		int32 next;
		bool truncate = false;
		bool truncated = false;
		for (int i = 0; (next = CurrentSelection(i)) >= 0; i++) {
			RemoteUserItem * user = (RemoteUserItem*)ItemAt(next);
			if (i > 0) {
				
				if (!truncate)
					handles += ", ";
				
				sessionIDs += ", ";
			}
            
			if (!truncate) 
				handles += user->GetDisplayHandle();
            
            sessionIDs += user->GetSessionID();

            if ((truncate) && (!truncated)) {
               truncated = true;
               handles += ", ...";
			}
            
			if (handles.Length() > 25)
            	truncate = true;
		}
         
		if (handles.Length() > 0) {
			BPopUpMenu* popup = new BPopUpMenu((const char *)NULL);

			String s(str(STR_CHAT_WITH));
            s += ' ';
            s += handles;
            BMenuItem* mi = new BMenuItem(s(), NULL);
            popup->AddItem(mi);

            popup->AddSeparatorItem();

            String s2(str(STR_WATCH));
            s2 += ' ';
            s2 += handles;
            BMenuItem* mi2 = new BMenuItem(s2(), NULL);
            popup->AddItem(mi2);

            ConvertToScreen(&pt);
            BMenuItem * result = popup->Go(pt);
            if (result == mi) {
               BMessage msg(fReplyWhat);
               msg.AddString("users", sessionIDs());
               Window()->PostMessage(&msg);
            } else if (result == mi2) {
               String mi2text;
               mi2text = "/watch ";
               mi2text += sessionIDs();
               ((ShareWindow*)Looper())->SendChatText(mi2text, NULL);
            }

            delete popup;
            return;
         }
      }
	ColumnListView::MouseDown(where);
}
};  // end namespace beshare
