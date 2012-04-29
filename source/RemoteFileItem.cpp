#include "RemoteFileItem.h"

#include <Region.h>
#include <View.h>

#include "RemoteUserItem.h"

#include "ShareWindow.h"
#include "ColumnListView.h"

#include "CLVListItem.h"

namespace beshare {

RemoteFileItem::RemoteFileItem(RemoteUserItem* owner, const char* fileName, const MessageRef& attrs)
  : 
  CLVListItem(0, false, false, 18.0f),
  _owner(owner),
  _fileName(fileName),
  _attributes(attrs)
{
	// empty
}


RemoteFileItem::~RemoteFileItem()
{
	// empty
}


void
RemoteFileItem::DrawItemColumn(BView * clv, BRect itemRect, int32 colIdx, bool complete)
{
	bool selected = IsSelected();
	rgb_color color = (selected) ? ((ColumnListView*)clv)->ItemSelectColor() : ((ColumnListView *)clv)->BgColor();
	clv->SetLowColor(color);
	
	if ((selected) || (complete)) {
		clv->SetHighColor(color);
		clv->FillRect(itemRect);
	}

	if (colIdx > 0) {
		BRegion Region;
		Region.Include(itemRect);
		clv->ConstrainClippingRegion(&Region);
		clv->SetHighColor(((ColumnListView *)clv)->TextColor());
		const char* text = _owner->GetShareWindow()->GetFileCellText(this, colIdx);
		
		if (text)
			clv->DrawString(text, BPoint(itemRect.left+2.0,itemRect.top+_textOffset));
		
		clv->ConstrainClippingRegion(NULL);
	} else if (colIdx == 0) {
		const BBitmap* bmp = _owner->GetShareWindow()->GetBitmap(this, colIdx);
		
		if (bmp) {
			clv->SetDrawingMode(B_OP_OVER);
			clv->DrawBitmap(bmp, BPoint(itemRect.left + ((itemRect.Width()-bmp->Bounds().Width())/2.0f), itemRect.top+((itemRect.Height()-bmp->Bounds().Height())/2.0f)));
		}
	}
}


void
RemoteFileItem::Update(BView* owner, const BFont *font)
{
	CLVListItem::Update(owner, font);

	font_height fontAttrs;
	font->GetHeight(&fontAttrs);
	_textOffset = ceil(fontAttrs.ascent) + (Height()-(ceil(fontAttrs.ascent) + ceil(fontAttrs.descent)))/2.0;
}


int
RemoteFileItem::Compare(const RemoteFileItem* item2, int32 key) const
{
	return _owner->GetShareWindow()->Compare(this, item2, key);
}


const char*
RemoteFileItem::GetPath() const
{
	const char * ret;
	return (_attributes.GetItemPointer()->FindString("beshare:Path", &ret) == B_NO_ERROR) ? ret : "";
}

};  // end namespace beshare
