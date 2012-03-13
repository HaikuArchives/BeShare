#ifndef SHARE_MIME_INFO
#define SHARE_MIME_INFO

#include <interface/Bitmap.h>
#include <interface/Menu.h>
#include <storage/Mime.h>

#include "util/String.h"
#include "util/Hashtable.h"
#include "BeShareNameSpace.h"

namespace beshare {

class ShareWindow;

class ShareMIMEInfo : public BMenu
{
public:
   ShareMIMEInfo(const char * label, const char * mimeString);

   const char * GetAttributeDescription(const char * attributeName) const;
   const char * GetMIMEString() const {return _mimeString();} 

   const BBitmap * GetIcon() const {return _iconValid ? &_icon : NULL;}

private:
   static status_t FindIcon(const BMimeType & mimeType, BBitmap & returnIcon);

   String _mimeString;
   Hashtable<String, String> _attrToDesc;
   BBitmap _icon;
   bool _iconValid;
};

};  // end namespace beshare

#endif
