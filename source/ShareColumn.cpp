#include "ShareColumn.h"
#include "RemoteFileItem.h"
#include "RemoteUserItem.h"
#include "ShareStrings.h"
#include "ShareUtils.h"

namespace beshare {

ShareColumn ::
ShareColumn(int type, const char * name, const char * label, float width)
  : CLVColumn(label, width, CLV_SORT_KEYABLE|CLV_HEADER_TRUNCATE), _type(type), _attrName(name)
{
  // empty
}


const char *
ShareColumn :: 
GetFileCellText(const RemoteFileItem * item) const
{
   _buf[0] = '-';
   _buf[1] = '\0';
   const char * result = _buf;

   switch(_type)
   {
      case ATTR_FILENAME:
         result = item->GetFileName();
      break;

      case ATTR_OWNERNAME:
         result = item->GetOwner()->GetDisplayHandle();
      break;

      case ATTR_OWNERID:
         result = item->GetOwner()->GetSessionID();
      break;

      case ATTR_OWNERCONNECTION:
         result = item->GetOwner()->GetBandwidthLabel();
      break;

      case ATTR_MISC:
      {
         uint32 c;
         type_code tc;
         const Message & msg = item->GetAttributes();
         const char * attrName = GetAttributeName();
         if (msg.GetInfo(attrName, &tc, &c) == B_NO_ERROR)
         {
            switch(tc)
            {
               case B_BOOL_TYPE:      
               {
                  bool v;
                  if (msg.FindBool(attrName, &v) == B_NO_ERROR) sprintf(_buf, "%s", v?str(STR_TRUE):str(STR_FALSE));
               }
               break;

               case B_DOUBLE_TYPE:      
               {
                  double v;
                  if (msg.FindDouble(attrName, &v) == B_NO_ERROR) sprintf(_buf, "%f", v);
               }
               break;

               case B_FLOAT_TYPE:      
               {
                  float v;
                  if (msg.FindFloat(attrName, &v) == B_NO_ERROR) sprintf(_buf, "%f", v);
               }
               break;

               case B_INT64_TYPE:      
               {
                  int64 v;
                  if (msg.FindInt64(attrName, &v) == B_NO_ERROR) 
                  {
                     if (strcmp(attrName, "beshare:File Size") == 0) GetByteSizeString(v, _buf);
                                                                else sprintf(_buf, "%Li", v);
                  }
               }
               break;

               case B_INT32_TYPE:      
               {
                  int32 v;
                  if (msg.FindInt32(attrName, &v) == B_NO_ERROR) 
                  {
                     if (strcmp(attrName, "beshare:Modification Time") == 0) GetTimeString(v, _buf);
                                                                        else sprintf(_buf, "%li", v);
                  }
               }
               break;

               case B_INT16_TYPE:      
               {
                  int16 v;
                  if (msg.FindInt16(attrName, &v) == B_NO_ERROR) sprintf(_buf, "%i", v);
               }
               break;

               case B_INT8_TYPE:      
               {
                  int8 v;
                  if (msg.FindInt8(attrName, &v) == B_NO_ERROR) sprintf(_buf, "%i", v);
               }
               break;

               case B_STRING_TYPE:      
                  (void) msg.FindString(attrName, &result);
               break;

               default:
                  result = str(STR_UNKNOWN_TYPE);
               break;
            }
         }
      }
      break;
   }

   return result;
}


int 
ShareColumn ::
Compare(const RemoteFileItem * rf1, const RemoteFileItem * rf2) const
{
   switch(_type)
   {
      case ATTR_FILENAME:
         return strcasecmp(rf1->GetFileName(), rf2->GetFileName());
      break;

      case ATTR_OWNERNAME:
         return strcasecmp(rf1->GetOwner()->GetDisplayHandle(), rf2->GetOwner()->GetDisplayHandle());
      break;

      case ATTR_OWNERID:
         return atol(rf1->GetOwner()->GetSessionID())-atol(rf2->GetOwner()->GetSessionID());
      break;

      case ATTR_OWNERCONNECTION:
      {
         uint32 b1 = rf1->GetOwner()->GetBandwidth();
         uint32 b2 = rf2->GetOwner()->GetBandwidth();
         return (b1 != b2) ? ((b1 > b2) ? -1 : 1) : 0;
      }
      break;

      case ATTR_MISC:
      {
         const char * attrName = GetAttributeName();
         uint32 c;
         type_code tc;
         const Message & msg1 = rf1->GetAttributes();
         const Message & msg2 = rf2->GetAttributes();
         if (msg1.GetInfo(attrName, &tc, &c) == B_NO_ERROR)
         {
            type_code tc2;
            if ((msg2.GetInfo(attrName, &tc2, &c) == B_NO_ERROR)&&(tc == tc2))
            {
               switch(tc)
               {
                  case B_BOOL_TYPE:      
                  {
                     bool v1, v2;
                     if ((msg1.FindBool(attrName, &v1) == B_NO_ERROR)&&(msg2.FindBool(attrName, &v2) == B_NO_ERROR)) return (v1 != v2) ? ((v1 == true) ? -1 : 1) : 0;
                  }
                  break;

                  case B_DOUBLE_TYPE:      
                  {
                     double v1, v2;
                     if ((msg1.FindDouble(attrName, &v1) == B_NO_ERROR)&&(msg2.FindDouble(attrName, &v2) == B_NO_ERROR)) return (v1 != v2) ? ((v1 > v2) ? -1 : 1) : 0;
                  }
                  break;

                  case B_FLOAT_TYPE:      
                  {
                     float v1, v2;
                     if ((msg1.FindFloat(attrName, &v1) == B_NO_ERROR)&&(msg2.FindFloat(attrName, &v2) == B_NO_ERROR)) return (v1 != v2) ? ((v1 > v2) ? -1 : 1) : 0;
                  }
                  break;

                  case B_INT64_TYPE:      
                  {
                     int64 v1, v2;
                     if ((msg1.FindInt64(attrName, &v1) == B_NO_ERROR)&&(msg2.FindInt64(attrName, &v2) == B_NO_ERROR)) return (v1 != v2) ? ((v1 > v2) ? -1 : 1) : 0;
                  }
                  break;

                  case B_INT32_TYPE:      
                  {
                     int32 v1, v2;
                     if ((msg1.FindInt32(attrName, &v1) == B_NO_ERROR)&&(msg2.FindInt32(attrName, &v2) == B_NO_ERROR)) return (v1 != v2) ? ((v1 > v2) ? -1 : 1) : 0;
                  }
                  break;

                  case B_INT16_TYPE:      
                  {
                     int16 v1, v2;
                     if ((msg1.FindInt16(attrName, &v1) == B_NO_ERROR)&&(msg2.FindInt16(attrName, &v2) == B_NO_ERROR)) return (v1 != v2) ? ((v1 > v2) ? -1 : 1) : 0;
                  }
                  break;

                  case B_INT8_TYPE:      
                  {
                     int8 v1, v2;
                     if ((msg1.FindInt8(attrName, &v1) == B_NO_ERROR)&&(msg2.FindInt8(attrName, &v2) == B_NO_ERROR)) return (v1 != v2) ? ((v1 > v2) ? -1 : 1) : 0;
                  }
                  break;

                  case B_STRING_TYPE:      
                  {
                     const char * v1, * v2;
                     if ((msg1.FindString(attrName, &v1) == B_NO_ERROR)&&(msg2.FindString(attrName, &v2) == B_NO_ERROR)) return strcasecmp(v1,v2);
                  }
                  break;
               }
            }
            else return -1;
         }
         else return (msg2.GetInfo(attrName, &tc, &c) == B_NO_ERROR) ? 1 : 0;
      }
      break;
   }

   return 0;
}


};  // end namespace beshare
