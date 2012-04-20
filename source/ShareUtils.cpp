#include <stdio.h>
#include <app/Message.h>
#include "ShareUtils.h"
#include "util/StringTokenizer.h"

namespace beshare {

void
GetByteSizeString(int64 v, char * buf)
{
	// special hack to give file sizes in kilobytes, etc. for readability
		  if (v > (1024LL*1024LL*1024LL*1024LL)) sprintf(buf, "%.2fTB", ((double)v)/(1024LL*1024LL*1024LL*1024LL));
	else if (v > (1024LL*1024LL*1024LL)) sprintf(buf, "%.2fGB", ((double)v)/(1024LL*1024LL*1024LL));
	else if (v > (1024LL*1024LL)) sprintf(buf, "%.2fMB", ((double)v)/(1024LL*1024LL));
	else if (v > (1024LL*10LL)) sprintf(buf, "%LiKB", v/1024LL);
	else sprintf(buf, "%Li bytes", v);
}

void 
GetTimeString(time_t when, char * buf)
{
	struct tm * now = localtime(&when);
	sprintf(buf, "%02i/%02i/%04i %02i:%02i:%02i", now->tm_mon+1, now->tm_mday, now->tm_year+1900, now->tm_hour, now->tm_min, now->tm_sec);
}

status_t SaveColorToMessage(const char * fn, const rgb_color & col, BMessage & msg)
{
	return msg.AddInt32(fn, (((uint32)col.red)<<24) | (((uint32)col.green)<<16) | (((uint32)col.blue)<<8) | (((uint32)col.alpha)<<0)); 
}

// Restores the given color from the given BMessage with the given field name.
status_t RestoreColorFromMessage(const char * fn, rgb_color & retCol, const BMessage & msg, uint32 which)
{
	uint32 val;
	if (msg.FindInt32(fn, which, (int32*)&val) == B_NO_ERROR)
	{
		retCol.red	= (val >> 24);
		retCol.green = (val >> 16);
		retCol.blue  = (val >> 8);
		retCol.alpha = (val >> 0);
		return B_NO_ERROR;
	}
	else return B_ERROR;
}

String SubstituteLabelledURLs(const String & shortName)
{
	String ret;

	const char * url	= NULL;  // if non-NULL, we've found a URL
	const char * space = NULL;  // if non-NULL, we've found the space after the URL too
	const char * left  = NULL;  // if non-NULL, we've found the left bracket for the label
	bool lastWasSpace  = true;  // If true, the last char we looked at was a space

	for (const char * sn = shortName(); *sn != '\0'; sn++)
	{
		char c = *sn;
		bool isSpace = ((c == ' ')||(c == '\t'));

		if (url)
		{
			if (space)
			{
				if (left)
				{
					if (c == ']')
					{
						// We've completed the sequence... so now dump just the label part,
						// plus any spaces that preceded the left bracket.
						String temp = (left+1);
						ret += temp.Substring(0, sn-(left+1));
						url = space = left = NULL;
					}
				}
				else if (isSpace == false)
				{
					if (c == '[') left = sn;
					else
					{
						// Oops, guess there is no label, so dump out what we got
						String temp = url;
						ret += temp.Substring(0, 1+sn-url);
						url = space = NULL;
					}
				}
			}
			else if (isSpace) space = sn;
		}
		else
		{
			if ((lastWasSpace)&&(IsLink(sn))) url = sn;
												  else ret += c;
		}

		lastWasSpace = isSpace;
	}

	if (url) ret += url;  // dump any leftovers

	return ret;
}

bool IsLink(const char * str)
{
	return ((strncmp(str, "file://",  7) == 0) || (strncmp(str, "http://",  7) == 0) ||
			  (strncmp(str, "https://", 8) == 0) || (strncmp(str, "mailto:",  7) == 0) ||
			  (strncmp(str, "ftp://",	6) == 0) || (strncmp(str, "audio://", 8) == 0) ||
			  (strncmp(str, "beshare:", 8) == 0) || (strncmp(str, "priv:",	 5) == 0) ||
			  (strncmp(str, "share:",	6) == 0));
}

};  // end namespace beshare
