#ifndef USERLISTVIEW_H
#define USERLISTVIEW_H

#include <santa/ColumnListView.h>

namespace beshare {
class UserListView : public ColumnListView
{
public:
	UserListView(uint32 replyWhat, BRect frame,
		CLVContainerView** containerView, const char* name = NULL,
		uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP,
			uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE,
			list_view_type type = B_SINGLE_SELECTION_LIST,
			bool hierarchical = false, bool horizontal = true, bool vertical = true,
			bool scroll_view_corner = true, border_style border = B_NO_BORDER,
			const BFont* labelFont = be_plain_font);
	void MouseDown(BPoint where);
private:
	uint32 fReplyWhat;
};

};  // end namespace beshare

#endif
