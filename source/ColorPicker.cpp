/*		ColorPicker
 *		Copyright (C) 2002 Vitaliy Mikitchenko
 *		
 *		This source is free software. You can use it in anyway that you want,
 *		as long as this header remains intact. If you do use this source outside
 *		of BeShare, you must give credit in About box or a README, etc... If you
 *		extend this class, feel free to send changes to vitviper@hotmail.com, though
 *		you are not required to do so. You will be given credit in this header.
 */

#include "ColorPicker.h"

#include <MenuField.h>
#include <Catalog.h>
#include <MenuItem.h>

#include "ShareUtils.h"
#include "ChatWindow.h"	// For constants...
#include "ShareStrings.h"


#undef B_TRANSLATE_CONTEXT
#define B_TRANSLATE_CONTEXT "ColorPicker"


namespace beshare {

enum { CP_BUTTON_REVERT = 'cpBR', CP_BUTTON_DEFAULT, CP_COL_UPDATE, CP_SELECTION_CHANGED, CP_NEW_COLOR_TYPE, CP_INIT };

const int CP_COL_WIDTH = 10;

ColorPicker::ColorPicker(BLooper * target) : BWindow(BRect(50, 75, 100 + (CP_COL_WIDTH * 32), 120 + (CP_COL_WIDTH * 8)), str(STR_SET_COLORS), B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_NOT_RESIZABLE), _target(target), _whichColor(0)
{
	const float margin = 5.0f;
	const float topRowHeight = 25.0f;
	_colControl = new BColorControl(BPoint(margin, margin+topRowHeight+margin), B_CELLS_32x8, CP_COL_WIDTH, "color_picker", new BMessage(CP_COL_UPDATE));
	ResizeTo(_colControl->Frame().right+margin, _colControl->Frame().bottom+margin);

	BView * view = new BView(Bounds(), NULL, B_FOLLOW_ALL, 0);
	AddChild(view);
	view->SetViewColor(BeBackgroundGrey);
  
	const BRect & bounds = view->Bounds(); 
	float revertWidth = view->StringWidth(str(STR_REVERT))+10.0f;
	BRect revertBounds(bounds.right-(revertWidth+margin), margin, bounds.right-margin, margin+topRowHeight);
	view->AddChild(_revert = new BButton(revertBounds, NULL, str(STR_REVERT), new BMessage(CP_BUTTON_REVERT)));

	float defaultWidth = view->StringWidth(str(STR_DEFAULT))+10.0f;
	BRect defaultBounds(revertBounds.left-(margin+defaultWidth), revertBounds.top, revertBounds.left-margin, revertBounds.bottom);
	view->AddChild(_default = new BButton(defaultBounds, NULL, str(STR_DEFAULT), new BMessage(CP_BUTTON_DEFAULT)));
	
	view->AddChild(_sampleView = new BView(BRect(margin, margin, margin+topRowHeight, margin+topRowHeight), NULL, B_FOLLOW_TOP|B_FOLLOW_LEFT, B_WILL_DRAW));

	_colorMenu = new BMenu("");
	_colorMenu->SetLabelFromMarked(true);
	for (int i=0; i<NUM_COLORS; i++)
	{
		BMessage * msg = new BMessage(CP_SELECTION_CHANGED);
		msg->AddInt32("which", i);
		BMenuItem * mi = new BMenuItem(str(STR_COLOR_BG+i), msg);
		_colorMenu->AddItem(mi);
		if (i==0) mi->SetMarked(true);
	}
	view->AddChild(new BMenuField(BRect(margin+topRowHeight+margin, margin, revertBounds.left-margin, revertBounds.bottom), NULL, NULL, _colorMenu));
	view->AddChild(_colControl);
	
	BMessage initMsg(CP_INIT);
	PostMessage(&initMsg);
}


ColorPicker::~ColorPicker()
{
	// empty
}

void
ColorPicker :: RequestColor(uint32 which, bool getDefault)
{
	BMessage askForColor(getDefault ? ChatWindow::CHATWINDOW_COMMAND_REQUEST_DEFAULT_COLOR : ChatWindow::CHATWINDOW_COMMAND_REQUEST_COLOR);
	askForColor.AddInt32("which", which);
	askForColor.AddInt32("replywhat", CP_NEW_COLOR_TYPE);
	askForColor.AddMessenger("replyto", this);
	_target->PostMessage(&askForColor);
}

void 
ColorPicker::MessageReceived(BMessage * msg)
{
	switch (msg->what)
	{
		case CP_INIT:
			_colorMenu->SetTargetForItems(this);
			_colControl->SetTarget(this);
			_revert->SetTarget(this);
			RequestColor(0, false);
		break;

		case CP_BUTTON_DEFAULT: RequestColor(_whichColor, true);  break;

		case CP_BUTTON_REVERT:  
			_colControl->SetValue(_color);
		// fall through
		case CP_COL_UPDATE:
			if (_whichColor >= 0)
			{
				rgb_color col = _colControl->ValueAsColor();
				BMessage msg(ChatWindow::CHATWINDOW_COMMAND_COLOR_CHANGED);
				msg.AddInt32("color", _whichColor);
				SaveColorToMessage("rgb", col, msg);
				_target->PostMessage(&msg);
				_colControl->SetEnabled(true);
				_sampleView->SetViewColor(col);
				_sampleView->Invalidate();
			}
		break;
		
		case CP_SELECTION_CHANGED:
		{
			int32 selected;
			if (msg->FindInt32("which", &selected) == B_NO_ERROR) RequestColor(selected, false);
		}
		break;

		case CP_NEW_COLOR_TYPE:
		{
			int32 which;
			rgb_color rgb;
			if ((msg->FindInt32("which", &which) == B_NO_ERROR)&&(RestoreColorFromMessage("rgb", rgb, *msg) == B_NO_ERROR))
			{
				_whichColor = which;
				_color = rgb;
				_colControl->SetValue(rgb);
				PostMessage(CP_COL_UPDATE);
			}
		}
		break;
		
		default:
			BWindow::MessageReceived(msg);
		break;
	}
}

};  // end namespace beshare
