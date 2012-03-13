/*******************************************************
*   SplitPane©
*
*   SplitPane is a usefull UI component. It alows the 
*   use to ajust two view Horizontaly or Vertacly so
*   that they are a desired size. This type of Pane 
*   shows up most comonly in Mail/News Readers.
*
*   @author  YNOP (ynop@acm.org)
*   @version beta
*   @date    Dec. 10 1999
*
*   (note:  modified by jaf@lcsaudio.com 6/20/00)
*
*******************************************************/
#include <stdio.h>
#include <AppKit.h>
#include <InterfaceKit.h>
#include <StorageKit.h>
#include <String.h>
#include <Path.h>
#include <TranslationKit.h>
#include <TranslationUtils.h>

#include "SplitPane.h"

namespace beshare {

/*******************************************************
*   Setup the main view. Add in all the niffty components
*   we have made and get things rolling
*******************************************************/
SplitPane::SplitPane(BRect frame, BView *one, BView *two,uint32 Mode):BView(frame, "", Mode,B_WILL_DRAW|B_FRAME_EVENTS){
   BRect b = Bounds();
  
   PaneOne = one;
   PaneTwo = two;

   prevSize.Set(0.0f,0.0f);  // used for resizing when resizeOne(X|Y) is true --jaf
   resizeOneX = resizeOneY = false;
   align = B_VERTICAL; // Most people use it this way
   pos.Set(b.Width()/2.0f, b.Height()/2.0f); // Center is a good start place
   thickness.Set(10.0f,10.0f);
   jump.Set(1.0f,1.0f); // 1 makes a smother slide
   VOneDetachable = false;
   VTwoDetachable = false;
   pad.Set(3.0f,3.0f);
   MinSizeOne.Set(0,0);
   MinSizeTwo.Set(0,0);
   poslocked = false; // free movement
   alignlocked = false; // free alignment
   Draggin = false;
   attached = false;
   swapPanes = false;

   WinOne = NULL;
   WinTwo = NULL;

   AddChild(one);
   AddChild(two);
}

/*******************************************************
*   When ready grap the parents color and refreash.
*******************************************************/
void SplitPane::AttachedToWindow(){
   BView::AttachedToWindow();
   attached = true;
   Update();
}

/*******************************************************
*   If we are being resized. Fix the stuff we need to fix
*******************************************************/
void SplitPane::FrameResized(float w, float h){
//   if bar is on the left side follow left
//   else if it is on the right side follow the right
//   Need to implements smart follow still
   BView::FrameResized(w,h);
   Update();
   Invalidate();
}

/*******************************************************
*   The main draw stuff here. basicly just the slider
*******************************************************/
void SplitPane::Draw(BRect /*f*/){
   SetHighColor(160,160,160);

   if(align == B_VERTICAL){
      SetHighColor(145,145,145);
      FillRect(BRect(pos.x,Bounds().top+pad.x+1,pos.x,Bounds().bottom-pad.x-1)); // 145
   
      SetHighColor(255,255,255);
      FillRect(BRect(pos.x+1,Bounds().top+pad.x+1,pos.x+2,Bounds().bottom-pad.x-1)); // 255
   
      SetHighColor(Parent()->ViewColor());
      FillRect(BRect(pos.x+2,Bounds().top+pad.x+1,pos.x+thickness.x-2,Bounds().bottom-pad.x-1));// 216
   
      SetHighColor(145,145,145);
      FillRect(BRect(pos.x+thickness.x-2,Bounds().top+pad.x+1,pos.x+thickness.x-2,Bounds().bottom-pad.x-1)) ;// 145
   
      SetHighColor(96,96,96);
      FillRect(BRect(pos.x+thickness.x-1,Bounds().top+pad.x+1,pos.x+thickness.x-1,Bounds().bottom-pad.x-1)); // 96
   }else{
      SetHighColor(145,145,145);
      FillRect(BRect(Bounds().left+pad.y+1,pos.y,Bounds().right-pad.y-1,pos.y)); // 145
   
      SetHighColor(255,255,255);
      FillRect(BRect(Bounds().left+pad.y+1,pos.y+1,Bounds().right-pad.y-1,pos.y+2)); // 255
   
      //SetHighColor(216,216,216);
      SetHighColor(Parent()->ViewColor());
      FillRect(BRect(Bounds().left+pad.y+1,pos.y+2,Bounds().right-pad.y-1,pos.y+thickness.y-2));// 216
   
      SetHighColor(145,145,145);
      FillRect(BRect(Bounds().left+pad.y+1,pos.y+thickness.y-2,Bounds().right-pad.y-1,pos.y+thickness.y-2)) ;// 145
   
      SetHighColor(96,96,96);
      FillRect(BRect(Bounds().left+pad.y+1,pos.y+thickness.y-1,Bounds().right-pad.y-1,pos.y+thickness.y-1)); // 96
   }
}

/*******************************************************
*   Keeps Modes for both panles uptodate and acctually
*   is the func that sets the location of the slider
*******************************************************/
void SplitPane::Update(){
   Window()->Lock();

   BView * p1 = swapPanes ? PaneTwo : PaneOne;
   BView * p2 = swapPanes ? PaneOne : PaneTwo;

   const BPoint & ms1 = swapPanes ? MinSizeTwo : MinSizeOne;
   const BPoint & ms2 = swapPanes ? MinSizeOne : MinSizeTwo;

   if(align == B_VERTICAL){
      if ((resizeOneX)&&(prevSize.x > 0)) pos.x += Bounds().Width()-prevSize.x;
      p1->SetResizingMode((resizeOneX ? B_FOLLOW_LEFT_RIGHT : B_FOLLOW_LEFT)|B_FOLLOW_TOP_BOTTOM);
      p2->SetResizingMode((resizeOneX ? B_FOLLOW_RIGHT : B_FOLLOW_LEFT_RIGHT)|B_FOLLOW_TOP_BOTTOM);
      if(pos.x > (Bounds().Width()-thickness.x-ms2.x)){
         if(!poslocked){
            pos.x = Bounds().Width()-thickness.x-ms2.x;
         }
      }
      if(pos.x < ms1.x){
         if(!poslocked){
            pos.x = ms1.x;
         }
      }
   }else{
      if ((resizeOneY)&&(prevSize.y > 0)) pos.y += Bounds().Height()-prevSize.y;
      p1->SetResizingMode((resizeOneY ? B_FOLLOW_TOP_BOTTOM : B_FOLLOW_TOP) | B_FOLLOW_LEFT_RIGHT);
      p2->SetResizingMode((resizeOneY ? B_FOLLOW_BOTTOM : B_FOLLOW_TOP_BOTTOM) | B_FOLLOW_LEFT_RIGHT);
      if(pos.y > (Bounds().Height()-thickness.y-ms2.y)){
         if(!poslocked){
            pos.y = Bounds().Height()-thickness.y-ms2.y;
         }
      }
      if(pos.y < ms1.y){
         if(!poslocked){
            pos.y = ms1.y;
         }
      }
   }
  
   // store our new size so we can do a diff next time we're called
   prevSize = BPoint(Bounds().Width(), Bounds().Height());

   if(p1){
      if(!WinOne){
         if(align == B_VERTICAL){
            p1->MoveTo(pad.x,Bounds().top+pad.x);
            p1->ResizeTo(pos.x-pad.x, Bounds().Height()-pad.x-pad.x); // widht x height
         }else{
            p1->MoveTo(pad.y,Bounds().top+pad.y);
            p1->ResizeTo(Bounds().Width()-pad.y-pad.y, pos.y-pad.y-pad.y); // widht x height
         }
      }
   }
   if(p2){
      if(!WinTwo){   
         if(align == B_VERTICAL){  
            p2->MoveTo(pos.x+thickness.x,Bounds().top+pad.x);
            p2->ResizeTo(Bounds().Width()-(pos.x+thickness.x)-pad.x, Bounds().Height()-pad.x-pad.x);
         }else{
            p2->MoveTo(Bounds().left+pad.y,pos.y+thickness.y);
            p2->ResizeTo(Bounds().Width()-pad.y-pad.y, Bounds().Height()-pos.y-pad.y-thickness.y);
         }
      }
   }

   Window()->Unlock();
}

/*******************************************************
*   Hook for when we click. This takes care of all the 
*   little stuff - Like where is the mouse and what is
*   going on.
*******************************************************/
void SplitPane::MouseDown(BPoint where){
   Window()->Lock();
   BMessage *currentMsg = Window()->CurrentMessage();
   if (currentMsg->what == B_MOUSE_DOWN) {
      uint32 buttons = 0;
      currentMsg->FindInt32("buttons", (int32 *)&buttons);
      uint32 modifiers = 0;
      currentMsg->FindInt32("modifiers", (int32 *)&modifiers);
      uint32 clicks = 0;
      currentMsg->FindInt32("clicks",(int32*)&clicks);
      
      if (buttons & B_TERTIARY_MOUSE_BUTTON) {
         swapPanes = !swapPanes;
         Update();
         Invalidate();
      }
      else if (buttons & B_SECONDARY_MOUSE_BUTTON){
         if(!alignlocked){
            switch(align){
            case B_VERTICAL:
               align = B_HORIZONTAL;
               break;
            case B_HORIZONTAL:
               align = B_VERTICAL;
               break;
            }
            Update();
            Invalidate();
         }
      }
      else if ((buttons & B_PRIMARY_MOUSE_BUTTON) && (!Draggin) && (IsInDraggerBounds(where))) {
         if(!poslocked){
            Draggin= true; // this is so we can drag
            here = where;
         }
         SetMouseEventMask(B_POINTER_EVENTS,B_LOCK_WINDOW_FOCUS);
      }
   }
   Window()->Unlock();
}

bool SplitPane::IsInDraggerBounds(BPoint pt) const
{
   BView * p1 = swapPanes ? PaneTwo : PaneOne;
   BView * p2 = swapPanes ? PaneOne : PaneTwo;

   // this block should go in FrameResized .. think about it
   return (align == B_VERTICAL) 
       ? ((pt.x > p1->Frame().right)&&(pt.x < p2->Frame().left))
       : ((pt.y > p1->Frame().bottom)&&(pt.y < p2->Frame().top));
}

/*******************************************************
*   If we unclick then stop dragging or whatever it is 
*   we are doing
*******************************************************/
void SplitPane::MouseUp(BPoint /*where*/){
   Draggin = false; // stop following mouse
}

/*******************************************************
*   If the mouse moves while we dragg. Then follow it
*   Also Invalidate so we update the views
*******************************************************/
void SplitPane::MouseMoved(BPoint where,uint32 /*info*/,const BMessage */*m*/){
   const BPoint & ms1 = swapPanes ? MinSizeTwo : MinSizeOne;
   const BPoint & ms2 = swapPanes ? MinSizeOne : MinSizeTwo;

   if(Draggin){
      float minVal;
      switch(align){
      case B_HORIZONTAL:
         pos.y = (where.y)-(thickness.y/2);
         minVal = ms1.y;
         break;
      case B_VERTICAL:
         pos.x = (where.x)-(thickness.x/2);
         minVal = ms1.x;
         break;
      }

      /*
      // This code figures out which jump we are closest
      // to and if needed we "snap" to that.
      int c = Bounds().IntegerWidth() / pos;
      Jump * c ... hmmm this is not right at all
      */
      
      switch(align){
         case B_HORIZONTAL:
            if (pos.y < ms1.y) pos.y = ms1.y;
            break;
         case B_VERTICAL:
            if (pos.x < ms1.x) pos.x = ms1.x;
            break;
      }

      if(align == B_VERTICAL){
         if(pos.x > (Bounds().Width() - thickness.x - ms2.x)){
            pos.x = (Bounds().Width() - thickness.x - ms2.x + 1);
         }
      }else{
         if(pos.y > (Bounds().Height() - thickness.y - ms2.y)){
            pos.y = (Bounds().Height() - thickness.y - ms2.y + 1);
         }
      }
      
      Update();

      Invalidate();
   }
}

/*******************************************************
*   If you already have a view One, but want to change
*   if for some odd reason. This should work.
*******************************************************/
void SplitPane::AddChildOne(BView *v){
   RemoveChild(PaneOne);
   PaneOne = v;
   AddChild(PaneOne);
}

/*******************************************************
*   If you already have a view Two, and want to put 
*   another view there, this is what to use.
*******************************************************/
void SplitPane::AddChildTwo(BView *v){
   RemoveChild(PaneTwo);
   PaneTwo = v;
   AddChild(PaneTwo);
}

/*******************************************************
*   Sets is we are horizontal or Vertical. We use the 
*   standard B_HORIZONTAL and B_VERTICAL flags for this
*******************************************************/
void SplitPane::SetAlignment(uint a){
   align = a;
   if(attached){
      Update();
   }
   Invalidate();
}

/*******************************************************
*   Returns wheather the slider is horizontal or vertical
*******************************************************/
uint SplitPane::GetAlignment() const {
   return align;
}

/*******************************************************
*   Sets whether to swap the positions of our panes.
*******************************************************/
void SplitPane::SetSwapped(bool sp){
   swapPanes = sp;
   if(attached){
      Update();
   }
   Invalidate();
}

/*******************************************************
*   Returns wheather the child panes are swapped.
*******************************************************/
bool SplitPane::GetSwapped() const {
   return swapPanes;
}

/*******************************************************
*   Sets the location of the bar. (we do no bounds 
*   checking for you so if its off the window thats 
*   your problem)
*******************************************************/
void SplitPane::SetBarPosition(BPoint p){
   pos = p;
   if(attached){
      Update();
   }
   Invalidate();
}

/*******************************************************
*   Returns about where the bar is ...
*******************************************************/
BPoint SplitPane::GetBarPosition() const {
   return pos;   
}

/*******************************************************
*   Sets how thick the bar should be.
*******************************************************/
void SplitPane::SetBarThickness(BPoint t){
   thickness = t;
   if(attached){
      Update();
   }
   Invalidate();
}

/*******************************************************
*   Retuns to us the thickness of the slider bar
*******************************************************/
BPoint SplitPane::GetBarThickness() const {
   return thickness;
}

/*******************************************************
*   Sets the amount of jump the bar has when it is 
*   moved. This can also be though of as snap. The bar
*   will start at 0 and jump(snap) to everry J pixels.
*******************************************************/
void SplitPane::SetJump(BPoint j){
   jump = j;
   if(attached){
      Update();
   }
}

/*******************************************************
*   Lets you know what the jump is .. see SetJump
*******************************************************/
BPoint SplitPane::GetJump() const {
   return jump;   
}

/*******************************************************
*   Do we have a View One or is it NULL
*******************************************************/
bool SplitPane::HasViewOne() const {
   if(PaneOne) return true;
   return false;   
}

/*******************************************************
*   Do we have a View Two .. or is it NULL too
*******************************************************/
bool SplitPane::HasViewTwo() const {
   if(PaneTwo) return true;
   return false;
}

/*******************************************************
*   Sets wheather View one is detachable from the 
*   slider view and from the app. This will creat a 
*   window that is detached (floating) from the app.
*******************************************************/
void SplitPane::SetViewOneDetachable(bool b){
   VOneDetachable = b;    
}

/*******************************************************
*   Sets view tow detachable or not
*******************************************************/
void SplitPane::SetViewTwoDetachable(bool b){
   VTwoDetachable = b;
}

/*******************************************************
*   Returns whether the view is detachable
*******************************************************/
bool SplitPane::IsViewOneDetachable() const {
   return VOneDetachable;
}

/*******************************************************
*   Returns if this view is detachable
*******************************************************/
bool SplitPane::IsViewTwoDetachable() const {
   return VTwoDetachable;
}

/*******************************************************
*   Tells the view if the user is alowed to open the 
*   configuration window for the slider.
*******************************************************/
void SplitPane::SetEditable(bool /*b*/){
   //ADD CODE HERE YNOP
}

/*******************************************************
*   Tells use if the split pane is user editable
*******************************************************/
bool SplitPane::IsEditable() const {
   return true; //ADD SOME MORE CODE HERE
}

/*******************************************************
*   Sets the inset that the view has.
*******************************************************/
void SplitPane::SetViewInsetBy(BPoint p){
   pad = p;
   if(attached){
      Update();
   }   
   Invalidate();
}

/*******************************************************
*   Returns to use the padding around the views
*******************************************************/
BPoint SplitPane::GetViewInsetBy() const {
   return pad;
}

/*******************************************************
*   This sets the minimum size that View one can be.
*   if the user trys to go past this .. we just stop
*   By default the minimum size is set to 0 (zero) so
*   the user can put the slider anywhere.
*******************************************************/
void SplitPane::SetMinSizeOne(const BPoint & p){
   MinSizeOne = p;
}

/*******************************************************
*   Gives us the minimum size that one can be.
*******************************************************/
BPoint SplitPane::GetMinSizeOne() const {
   return MinSizeOne;
}

/*******************************************************
*   This sets the Minimum size that the second view 
*   can be.
*******************************************************/
void SplitPane::SetMinSizeTwo(const BPoint & p){
   MinSizeTwo = p;
}

/*******************************************************
*   Lets us know what that minimum size is.
*******************************************************/
BPoint SplitPane::GetMinSizeTwo() const {
   return MinSizeTwo;
}

/*******************************************************
*   Locks the bar from being moved by the User. The
*   system can still move the bar (via SetBarPosition)
*******************************************************/
void SplitPane::SetBarLocked(bool b){
   poslocked = b;
}

/*******************************************************
*   Returns to use if the bar is in a locked state or 
*   not.
*******************************************************/
bool SplitPane::IsBarLocked() const {
   return poslocked;
}

/*******************************************************
*   Locks the alignment of the bar. The user can no 
*   longer toggle between Horizontal and Vertical
*   Slider bar. Again you can still progomaticly set 
*   the position how ever you want.
*******************************************************/
void SplitPane::SetBarAlignmentLocked(bool b){
   alignlocked = b;
}

/*******************************************************
*   Lets us know about the lock state of the bar
*******************************************************/
bool SplitPane::IsBarAlignmentLocked() const {
   return alignlocked;
}

/*******************************************************
*   Locks the alignment of the bar. The user can no 
*   longer toggle between Horizontal and Vertical
*   Slider bar. Again you can still progomaticly set 
*   the position how ever you want.
*******************************************************/
void SplitPane::SetResizeViewOne(bool x, bool y){
   resizeOneX = x;
   resizeOneY = y;
   if(attached){
      Update();
   }
   Invalidate();
}

/*******************************************************
*   Lets us know about the lock state of the bar
*******************************************************/
void SplitPane::GetResizeViewOne(bool & rx, bool & ry) const {
   rx = resizeOneX;
   ry = resizeOneY;
}


/*******************************************************
*   Gets the Total state of the bar, alignment, size,
*   position and many other things that are required
*   to fully capture the state of the SplitPane.
*   We pack all of this into a cute little BMessage
*   so that it is esally expandable and can be saved 
*   off easyaly too.  The SplitPane System does not 
*   however save the state for you. Your program must
*   grab the state and save it in its config file.
*******************************************************/
void SplitPane::GetState(BMessage & state) const {
   state.AddBool("onedetachable",VOneDetachable);
   state.AddBool("twodetachable",VTwoDetachable);
   state.AddInt32("align",align);
   state.AddPoint("pos",pos);
   state.AddPoint("thick",thickness);   
   state.AddPoint("jump",jump);
   state.AddPoint("pad",pad);
   state.AddPoint("minsizeone",MinSizeOne);
   state.AddPoint("minsizetwo",MinSizeTwo);
   state.AddBool("poslock",poslocked);
   state.AddBool("alignlock",alignlocked);
   state.AddBool("resizeonex", resizeOneX);
   state.AddBool("resizeoney", resizeOneY);
   state.AddBool("swap", swapPanes);
}

/*******************************************************
*   Sets the state of the SplitPane from a BMessage 
*   like the one recived from GetState(). 
*   This is one of three ways the user can rebuild the
*   state of the SplitPane. The second is to simply 
*   send the SplitPane the state message, it is the 
*   same as calling SetState but it ashyncronouse.
*   The third way is to use all the Get/Set methouds
*   for each element of the SplitPane, this way is 
*   long and boarding. I suggest you just send the 
*   View a message :)
*******************************************************/
void SplitPane::SetState(BMessage *state){
   BPoint pt;
   int32 i;

   if(state->FindBool("onedetachable",&VOneDetachable) != B_NO_ERROR) VOneDetachable = false;
   if(state->FindBool("towdetachable",&VTwoDetachable) != B_NO_ERROR) VTwoDetachable = false;
   if(state->FindInt32("align",&i) == B_NO_ERROR) align = i;
   if(state->FindPoint("pos",&pt) == B_NO_ERROR) pos = pt;
   if(state->FindPoint("thick",&pt) == B_NO_ERROR) thickness = pt;  
   if(state->FindPoint("jump",&pt) == B_NO_ERROR) jump = pt;
   if(state->FindPoint("pad",&pt) == B_NO_ERROR) pad = pt;
   if(state->FindBool("poslock",&poslocked) != B_NO_ERROR) poslocked = false;
   if(state->FindBool("alignlock",&alignlocked) != B_NO_ERROR) alignlocked = false;
   if(state->FindBool("resizeonex",&resizeOneX) != B_NO_ERROR) resizeOneX = false;
   if(state->FindBool("resizeoney",&resizeOneY) != B_NO_ERROR) resizeOneY = false;
   if(state->FindBool("swap",&swapPanes) != B_NO_ERROR) swapPanes = false;

   if (attached) {
      Update();
      Invalidate();
   }
}

/*******************************************************
*   Ok, hmm what does this do. NOT MUCH. if we get a 
*   STATE message then lets set the state. This is here
*   to provide a asyncronuse way of seting the state and
*   also to make life easyer.
*******************************************************/
void SplitPane::MessageReceived(BMessage *msg){
   switch(msg->what){
   case SPLITPANE_STATE:
      SetState(msg);   
      break;
   default:
      BView::MessageReceived(msg);
      break;
   }
}







};  // end namespace beshare
