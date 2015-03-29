#ifndef RESULTSVIEW_H
#define RESULTSVIEW_H

#include <santa/ColumnListView.h>

namespace beshare {

class ResultsView : public ColumnListView
{
public:
	ResultsView(uint32 replyWhat, BRect frame, CLVContainerView** containerView,
		const char* name = NULL,
		uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP,
		uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE,
		list_view_type type = B_SINGLE_SELECTION_LIST,
		bool hierarchical = false, bool horizontal = true,
		bool vertical = true, bool scroll_view_corner = true,
		border_style border = B_NO_BORDER,
		const BFont* labelFont = be_plain_font);

	~ResultsView();

	void MouseDown(BPoint where);
	bool InitiateDrag(BPoint /*point*/, int32 /*index*/, bool /*wasSelected*/);

#ifdef SAVE_BEOS
	virtual void Draw(BRect ur);
	virtual bool AddItem(BListItem *item);
	virtual bool AddItem(BListItem *item, int32 atIndex);
	virtual bool AddList(BList *newItems);
	virtual bool AddList(BList *newItems, int32 atIndex);
	bool RemoveItem(BListItem *item);
	BListItem *RemoveItem(int32 index);
	bool RemoveItems(int32 index, int32 count);
	void MakeEmpty();
#endif

private:
	uint32 fReplyWhat;

#if SAVE_BEOS
	BBitmap * fSbe;
#endif
};
};  // end namespace beshare
#endif
