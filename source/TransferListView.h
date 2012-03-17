#ifndef TRANSFERLISTVIEW_H
#define TRANSFERLISTVIEW_H

/* 	Subclass of BListView that only clears the background area that doesn't contain items.  
	This reduces flicker during downloads
*/

#include <ListView.h>
#include <Message.h>
#include <Rect.h>
#include <Menu.h>
#include <Point.h>

#include "util/Hashtable.h"
#include "ShareConstants.h"

namespace beshare {
class ShareFileTransfer;
class TransferListView : public BListView
{
public:
   TransferListView(BRect rect, uint32 banCommand);
   virtual void MessageReceived(BMessage * msg);
   virtual void Draw(BRect ur);
   virtual void MouseDown(BPoint where);
   void MoveSelectedItems(int delta);
   void MoveSelectedToExtreme(int dir);
private:
   void _AddFileItems(BMenu* menu, type_code tc, const ShareFileTransfer* xfr);
   void _AddLimitItem(BMenu* addTo, uint32 transferRate, uint32 currentLimit
   	, uint32& prevVal);
   void _AddBanItem(BMenu* addTo, const Hashtable<uint32, bool>& canBans, 
   			int count, const char* unit, bigtime_t microsPerUnit);
   			
   uint32 fBanCommand;
};
}
#endif
