#ifndef REFLOWING_TEXT_VIEW_H
#define REFLOWING_TEXT_VIEW_H

#include <app/AppDefs.h>
#include <app/Application.h>
#include <app/Roster.h>
#include <app/MessageFilter.h>
#include <app/Message.h>
#include <app/MessageRunner.h>
#include <app/Messenger.h>
#include <interface/StringView.h>
#include <interface/TextView.h>
#include <interface/Window.h>

#include "util/Queue.h"
#include "util/String.h"
#include "BeShareNameSpace.h"

namespace beshare {

/* Subsclass of BTextView that automatically reflows the text when it is resized */
class ReflowingTextView : public BTextView
{
public:
   ReflowingTextView(BRect frame, const char *name, BRect textRect, uint32 resizeMask, uint32 flags = B_WILL_DRAW | B_PULSE_NEEDED);
   virtual ~ReflowingTextView();

   virtual void AttachedToWindow();
  
   virtual void FrameResized(float w, float h);
 
   virtual void MessageReceived(BMessage * msg);

   virtual void MouseMoved(BPoint where, uint32 code, const BMessage * msg);

   virtual void MouseDown(BPoint where);

   void FixTextRect();

   void Clear();

   void AddURLRegion(uint32 start, uint32 len, const String & optHiddenURL);

   void SetCommandURLTarget(const BMessenger & target, const BMessage & queryMsg, const BMessage & privMsg);

private:
   void UpdateToolTip();

   /* A simple little data class that remembers stuff we need to remember about hyperlinks in the text view */
   class URLLink
   {
   public:
      URLLink() : _start(0), _len(0) {/* empty */}
      URLLink(uint32 start, uint32 len, const String & optURL) : _start(start), _len(len), _url(optURL) {/* empty */}

      inline uint32 GetStart() const {return _start;}
      inline uint32 GetLength() const {return _len;}
      const String & GetURL() const {return _url;}

   private:
      uint32 _start;
      uint32 _len;
      String _url;
   };

   const URLLink * GetURLAt(BPoint pt) const;

   Queue<URLLink> _urls;
   BMessenger _commandTarget;
   BMessage _queryMessage;
   BMessage _privMessage;
   BMessageRunner * _showTipRunner;
   bool _runnerWillHide;  // If true, the runner is a hide-runner.
   bool _canShow;
   BWindow * _urlTip;
   BStringView * _urlStringView;
   int32 _currentLinkStart;
   String _currentURLString;
};

};  // end namespace beshare

#endif
