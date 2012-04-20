/*		ColorPicker
 *		Copyright (C) 2002 Vitaliy Mikitchenko
 *		
 *		This source is free software. You can use it in anyway that you want,
 *		as long as this header remains intact. If you do use this source outside
 *		of BeShare, you must give credit in About box or a README, etc... If you
 *		extend this class, feel free to send changes to vitviper@hotmail.com, though
 *		you are not required to do so. You will be given credit in this header.
 */
#ifndef COLOR_PICKER_H
#define COLOR_PICKER_H

#include <Window.h>
#include <ColorControl.h>
#include <Button.h>
#include <Menu.h>
#include <View.h>
#include <Looper.h>

namespace beshare {

class ColorPicker : public BWindow
{
public:
	ColorPicker(BLooper * target);
	virtual ~ColorPicker();
	
	virtual void MessageReceived(BMessage * msg);
	
private:
	void RequestColor(uint32 which, bool getDefault);

	BButton * _revert;
	BButton * _default;
	BMenu * _colorMenu;
	BView * _sampleView;
	BColorControl * _colControl;
	
	BLooper * _target;
	rgb_color _color;
	int32 _whichColor;
};

};

#endif

