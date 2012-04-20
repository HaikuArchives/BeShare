/*******************************************************
*	SplitPane©
*
*	SplitPane is a usefull UI component. It alows the 
*	use to ajust two view Horizontaly or Vertacly so
*	that they are a desired size. This type of Pane 
*	shows up most comonly in Mail/News Readers.
*
*	@author  YNOP (ynop@acm.org)
*	@version beta
*	@date	 Dec. 10 1999
*******************************************************/
#ifndef _SPLIT_PANE_VIEW_H
#define _SPLIT_PANE_VIEW_H

#include <Application.h>
#include <AppKit.h>
#include <InterfaceKit.h>

namespace beshare {

#define SPLITPANE_STATE 'spst'

/* jaf: All BPoint arguments represent the value to use when in
 *		B_VERTICAL alignment followed by the value to use when in
 *		B_HORIZONTAL alignment. 
 */
class SplitPane : public BView {
	public:
		SplitPane(BRect,BView*,BView*,uint32);

		void AddChildOne(BView*);
		void AddChildTwo(BView*);

		void SetAlignment(uint);
		uint GetAlignment() const;

		void SetSwapped(bool);
		bool GetSwapped() const;

		void SetBarPosition(BPoint pos);
		BPoint GetBarPosition() const;

		void SetBarThickness(BPoint);
		BPoint GetBarThickness() const;

		void SetJump(BPoint);
		BPoint GetJump() const;

		bool HasViewOne() const;
		bool HasViewTwo() const;

		void SetViewOneDetachable(bool);
		void SetViewTwoDetachable(bool);

		bool IsViewOneDetachable() const;
		bool IsViewTwoDetachable() const;

		void SetEditable(bool);
		bool IsEditable() const;

		void SetViewInsetBy(BPoint);
		BPoint GetViewInsetBy() const;

		void SetMinSizeOne(const BPoint & size);
		BPoint GetMinSizeOne() const;

		void SetMinSizeTwo(const BPoint & size);
		BPoint GetMinSizeTwo() const;

		void GetState(BMessage & writeTo) const;
		void SetBarLocked(bool);

		bool IsBarLocked() const;
		void SetBarAlignmentLocked(bool);

		bool IsBarAlignmentLocked() const;
		void SetState(BMessage*);

		void SetResizeViewOne(bool whileInVertAlign, bool whileInHorizAlign);
		void GetResizeViewOne(bool & returnWhileInVertAlign, bool & returnWhileInHorizAlign) const;

		virtual void Draw(BRect);
		virtual void AttachedToWindow();
		virtual void FrameResized(float,float);
		virtual void MouseDown(BPoint);
		virtual void MouseUp(BPoint);
		virtual void MouseMoved(BPoint,uint32,const BMessage*);
		virtual void MessageReceived(BMessage*);

	private:
		bool IsInDraggerBounds(BPoint pt) const;
		void Update();

		BView *PaneOne;
		BView *PaneTwo;
	  
		//State info
		bool VOneDetachable;
		bool VTwoDetachable;
		uint align;
		BPoint pos;
		BPoint thickness;
		BPoint jump;
		BPoint pad;
		BPoint MinSizeOne;
		BPoint MinSizeTwo;
		bool poslocked;
		bool alignlocked;
		bool resizeOneX;  // added by jaf; determines which child view gets
		bool resizeOneY;  // resized when SplitPane view is resized
		bool swapPanes;

		//end State info
		
		bool Draggin;
		BPoint here;
		bool attached;
		BPoint prevSize;
		
		BWindow *WinOne;
		BWindow *WinTwo;
		
//		BWindow *ConfigWindow;
};

};  // end namespace beshare

#endif
