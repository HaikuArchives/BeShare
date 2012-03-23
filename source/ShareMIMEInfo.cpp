#include <app/Roster.h>
#include "ShareStrings.h"

#include "ShareMIMEInfo.h"

namespace beshare {

#define ICON_BITMAP_RECT BRect(0.0f,0.0f,15.0f,15.0f)
#define ICON_BITMAP_SPACE B_COLOR_8_BIT

ShareMIMEInfo :: ShareMIMEInfo(const char * label, const char * mimeString) : BMenu(label), _mimeString(mimeString), _icon(ICON_BITMAP_RECT, ICON_BITMAP_SPACE), _iconValid(false)
{
   BMimeType mt(mimeString);
   if (mt.InitCheck() == B_NO_ERROR)
   {
      BMessage attrInfo;
      if (mt.GetAttrInfo(&attrInfo) == B_NO_ERROR)
      {
         const char * attrName;
         for (int i=0; attrInfo.FindString("attr:name", i, &attrName) == B_NO_ERROR; i++)
         {
            const char * attrDesc;
            if (attrInfo.FindString("attr:public_name", i, &attrDesc) == B_NO_ERROR)
            {
               _attrToDesc.Put(attrName, attrDesc);
            }
         }
      }
      if (FindIcon(mt, _icon) == B_NO_ERROR) _iconValid = true;
   }
}


status_t
ShareMIMEInfo :: FindIcon(const BMimeType & mt, BBitmap & icon)
{
   // Get the icon for this MIME type.
   if (mt.GetIcon(&icon, B_MINI_ICON) == B_NO_ERROR) return B_NO_ERROR;

   // If we couldn't get an icon from the MIME type, maybe we can 
   // get an icon from the type's preferred handler application.
   char buf[B_MIME_TYPE_LENGTH];
   if (mt.GetPreferredApp(buf) == B_NO_ERROR)
   {
      BMimeType appType(buf);
      if ((appType.InitCheck() == B_NO_ERROR)&&(appType.GetIconForType(mt.Type(), &icon, B_MINI_ICON) == B_NO_ERROR)) return B_NO_ERROR;
   }

   // Still nothing?  Okay, let's try to get an icon from one of the supporting apps...
   BMessage sapps;
   if (mt.GetSupportingApps(&sapps) == B_NO_ERROR) 
   {
      const char * sappName;
      for (int i=0; (sapps.FindString("applications", i, &sappName) == B_NO_ERROR); i++)
      {
         BMimeType sappType(sappName);
         if ((sappType.InitCheck() == B_NO_ERROR)&&(sappType.GetIconForType(mt.Type(), &icon, B_MINI_ICON) == B_NO_ERROR)) return B_NO_ERROR;
      }
   }

   // Okay, we couldn't find any icon for that MIME type.  Maybe we can
   // get an icon for its supertype, though.
   BMimeType superType;
   return (mt.GetSupertype(&superType) == B_NO_ERROR) ? FindIcon(superType, icon) : B_ERROR;
}


const char * 
ShareMIMEInfo::GetAttributeDescription(const char* attributeName) const
{
	const String* ret = _attrToDesc.Get(attributeName);
	return ret ? ret->Cstr() : NULL;
}



};  // end namespace beshare
