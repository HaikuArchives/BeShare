#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include <Application.h>
#include <Clipboard.h>
#include <MessageFilter.h>
#include <Box.h>
#include <Font.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <Input.h>
#include <Path.h>

#include <Beep.h>

#include "ShareStrings.h"
#include "ChatWindow.h"
#include "Colors.h"
#include "regex/StringMatcher.h"
#include "util/StringTokenizer.h"

#include "ReflowingTextView.h"
#include "ShareUtils.h"
#include "ColumnListView.h"

namespace beshare {

#define CHAT_HISTORY_LENGTH 500   // remember the last 500 items you typed

#define TEXT_ENTRY_HEIGHT 20

// All of our colors
static rgb_color _defaultColors[NUM_COLORS] = { 
   { 255, 255, 255, 255 }, // COLOR_BG
   { 245, 255, 255, 255 }, // COLOR_SCROLLBG
   { 216, 216, 216, 255 }, // COLOR_BORDERS
   {   0,   0,   0, 255 }, // COLOR_TEXT
   {   0,   0, 128, 255 }, // COLOR_SYSTEM
   { 255, 128,   0, 255 }, // COLOR_WARNING
   { 255,   0,   0, 255 }, // COLOR_ERROR
   { 128,   0, 128, 255 }, // COLOR_ACTION
   {   0, 150, 150, 255 }, // COLOR_PRIVATE
   { 255, 128,   0, 255 }, // COLOR_NAMESAID
   { 202,  72, 144, 255 }, // COLOR_PING
   {   0, 128,   0, 255 }, // COLOR_LOCAL
   {   0,   0,   0, 255 }, // COLOR_REMOTE
   {   0,   0, 255, 255 }, // COLOR_URL
   { 178, 178, 178, 255 }, // COLOR_SELECTION
   { 128,   0,   0, 255 }, // COLOR_WATCH
   { 216, 216, 255, 255 }, // COLOR_DOWNLOAD
   { 255, 216, 216, 255 }, // COLOR_UPLOAD
   { 216, 255, 216, 255 }  // COLOR_PAUSEDUPLOAD
};
                                  
ChatWindow :: ChatWindow(BRect defRect, const char * title, window_look look, window_feel feel, uint32 flags) : BWindow(defRect, title, look, feel, flags), _isInitialized(false), _isScrolling(false), _fontSize(0.0f), _chatText(NULL), _customColorsEnabled(true), _logFile(NULL)
{
   // Patch up the default colors to match the system defaults
   _defaultColors[COLOR_BORDERS]      = ui_color(B_PANEL_BACKGROUND_COLOR);
#ifdef B_BEOS_VERSION_DANO
   _defaultColors[COLOR_BG]           = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
   _defaultColors[COLOR_TEXT]         = ui_color(B_DOCUMENT_TEXT_COLOR);
   _defaultColors[COLOR_SCROLLBG]     = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
   _defaultColors[COLOR_SCROLLBG].red = (tint_color(ui_color(B_DOCUMENT_BACKGROUND_COLOR), 1.07F)).red;
#endif

   (void) GetAppSubdir("logs", _logsDir, true);

   for (uint32 i=0; i<NUM_COLORS; i++) _colors[i] = _defaultColors[i];
}


ChatWindow :: ~ChatWindow()
{
   CloseLogFile();
}

const rgb_color & 
ChatWindow :: 
GetColor(uint32 which, int forceDelta) const
{
   bool useDefault = (_customColorsEnabled == false);
        if (forceDelta < 0) useDefault = false;
   else if (forceDelta > 0) useDefault = true;

   return useDefault ? _defaultColors[which] : _colors[which];
}

void
ChatWindow ::
LogHelp(const char * cmd, int tokenID, int descID, ChatWindow * optEchoTo)
{
   String s("   /");
   s += cmd;
   s += " ";
   if (tokenID >= 0) s += str(tokenID);
   s += " - ";
   s += str(descID);
   LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, optEchoTo);
}


class PasteMessageFilter : public BMessageFilter
{
public:
   PasteMessageFilter(uint32 command, BTextControl * control) : BMessageFilter(B_PASTE), _command(command), _control(control) {/* empty */}

   virtual filter_result Filter(BMessage *message, BHandler **)
   {
      filter_result ret = B_DISPATCH_MESSAGE;

      if (message->what == B_PASTE)
      {
         if (be_clipboard->Lock())
         {
            BMessage * data = be_clipboard->Data();
            const char * text;
            ssize_t textLen;
            if (data->FindData("text/plain", B_MIME_TYPE, (const void **) &text, &textLen) == B_NO_ERROR)
            {
               if ((strchr(text, '\r'))||(strchr(text, '\n')))
               {
                  String temp(_control->Text());
                  if (temp.Length() == 0) temp += "\n";

                  // gotta do this shit because text isn't terminated, sigh
                  char * s = new char[textLen+1];
                  memcpy(s, text, textLen);
                  s[textLen] = '\0';
                  temp += s;
                  delete [] s;

                  BMessage sendMsg(_command);
                  sendMsg.AddString("text", temp());
                  _control->Window()->PostMessage(&sendMsg);
                  ret = B_SKIP_MESSAGE;
               }
            }
            be_clipboard->Unlock();
         }
      }
      return ret;
   }

private:
   uint32 _command;
   BTextControl * _control;
};

void ChatWindow :: MessageReceived(BMessage * msg)
{
   switch(msg->what)
   {
      case CHATWINDOW_COMMAND_REQUEST_COLOR:
      case CHATWINDOW_COMMAND_REQUEST_DEFAULT_COLOR:
      {
         int32 which, replyWhat;
         BMessenger replyTo;
         if ((msg->FindInt32("which",     &which)     == B_NO_ERROR)&&
             (msg->FindInt32("replywhat", &replyWhat) == B_NO_ERROR)&&
             (msg->FindMessenger("replyto", &replyTo) == B_NO_ERROR))
         {
            BMessage reply(replyWhat);
            reply.AddInt32("which", which);
            SaveColorToMessage("rgb", GetColor(which, (msg->what == CHATWINDOW_COMMAND_REQUEST_DEFAULT_COLOR) ? 1 : 0), reply);
            replyTo.SendMessage(&reply);
         }
      }
      break;

      case CHATWINDOW_COMMAND_COLOR_CHANGED:
      {
         int32 color;
         for (int i=0; msg->FindInt32("color", i, &color) == B_NO_ERROR; i++)
         {
            rgb_color temp;
            if (RestoreColorFromMessage("rgb", temp, *msg, i) == B_NO_ERROR) SetColor(color, temp);
         }
         UpdateColors();
      }
      break;         

      case CHATWINDOW_COMMAND_UPDATE_COLORS:
         UpdateColors();
      break;

      case CHATWINDOW_COMMAND_INVALIDATE_TEXT_VIEW:
         _chatText->Invalidate();   // thats it...
      break;
      
      case CHATWINDOW_COMMAND_SET_CUSTOM_TITLE:
      {
         const char * title;
         if (msg->FindString("title", &title) != B_NO_ERROR) title = "";
         SetCustomWindowTitle(title);
         UpdateTitleBar();
      }
      break;

      case CHATWINDOW_COMMAND_SET_COMMAND_TARGET:
      {
         BMessenger target;
         BMessage queryMessage;
         BMessage privMessage;
         if ((msg->FindMessenger("target", &target) == B_NO_ERROR)&&(msg->FindMessage("querymessage", &queryMessage) == B_NO_ERROR)&&(msg->FindMessage("privmessage", &privMessage) == B_NO_ERROR)) _chatText->SetCommandURLTarget(target, queryMessage, privMessage);
         break;
      }

      case CHATWINDOW_COMMAND_SEND_CHAT_TEXT:
      {
         String text;
         {
            const char * temp;
            if (msg->FindString("text", &temp) == B_NO_ERROR) text = temp;
            else
            {
               text = _textEntry->Text();
               (void) ExpandAlias(text.Trim(), text);
            }
         }

         if (text.Length() > 0)
         {
            if ((_chatHistory.GetNumItems() == 0)||(_chatHistory.GetItemPointer(_chatHistory.GetNumItems()-1)->Equals(text) == false))
            {
               _chatHistory.AddTail(text);
               if (_chatHistory.GetNumItems() > CHAT_HISTORY_LENGTH) _chatHistory.RemoveHead();
            }
            _chatHistoryPosition = -1;  // reset to bottom of list for next time                      
            SendChatText(text, this);
         }
         _textEntry->SetText("");
         break;
      }
      
      default:
         BWindow::MessageReceived(msg);
      break;
   }
}


void 
ChatWindow ::
ChatTextReceivedBeep(bool isPersonal, bool mentionsName)
{
   if ((isPersonal)||(mentionsName)) DoBeep(isPersonal ? SYSTEM_SOUND_PRIVATE_MESSAGE_RECEIVED : SYSTEM_SOUND_USER_NAME_MENTIONED);
}

void
ChatWindow ::
LogMessage(LogMessageType type, const char * inText, const char * optSessionID, const rgb_color * inOptTextColor, bool isPersonal, ChatWindow * optEchoTo)
{
   if ((optEchoTo)&&(optEchoTo != this)) 
   {  
      optEchoTo->LogMessage(type, inText, optSessionID, inOptTextColor, isPersonal, NULL);
      return;
   }

   for (LogDestinationType d = DESTINATION_DISPLAY; d<NUM_DESTINATIONS; d = (LogDestinationType)(((int)d)+1))
   {
      const char * text = inText;
      const rgb_color * optTextColor = inOptTextColor;

      if (OkayToLog(type, d, isPersonal))
      {
         const char * preamble = "???"; 
         const rgb_color & textColor = GetColor(COLOR_TEXT);
         rgb_color color = GetColor(COLOR_TEXT);
         String temp;
         int startRed = -1;
         int redLen = 0;
         
         if (optTextColor == NULL) optTextColor = &textColor;

         switch(type)
         {
            case LOG_INFORMATION_MESSAGE:
            case LOG_USER_EVENT_MESSAGE:
            case LOG_UPLOAD_EVENT_MESSAGE:
               preamble = str(STR_SYSTEM);
               color = GetColor(COLOR_SYSTEM);
            break;

            case LOG_WARNING_MESSAGE:
               preamble = str(STR_WARNING);
               color = GetColor(COLOR_WARNING);
            break;

            case LOG_ERROR_MESSAGE:
               preamble = str(STR_ERROR);
               color = GetColor(COLOR_ERROR);
            break;

            case LOG_LOCAL_USER_CHAT_MESSAGE:
               if ((strncmp(text, "/me ", 4) == 0)||(strncmp(text, "/me\'", 4) == 0))
               {
                  preamble = str(STR_ACTION);
                  
                  GetLocalUserName(temp);
                  temp += &text[3];
                  text = temp();
                  color = GetColor(COLOR_ACTION);
               }     
               else
               {
                  String t2;
                  if (ShowUserIDs(d))
                  {
                     temp = "(";
                     GetLocalSessionID(t2);
                     temp += t2;
                     temp += ") ";
                  }

                  GetLocalUserName(t2);
                  temp += t2;
                  if ((optSessionID)&&(ShowMessageTargets()))
                  {
                     temp += "-> (";
                     temp += optSessionID;
                     temp += " / ";
                     GetUserNameForSession(optSessionID, t2);
                     temp += t2; 
                     temp += ") ";
                  }
                  preamble = temp();
                  color = GetColor(COLOR_LOCAL);
               }
            break;

            case LOG_REMOTE_USER_CHAT_MESSAGE:
               if (optSessionID)
               {
                  if ((strncmp(text, "/me ", 4) == 0)||(strncmp(text, "/me\'", 4) == 0))
                  {
                     preamble = str(STR_ACTION);
                     GetUserNameForSession(optSessionID, temp);
                     temp += &text[3];
                     text = temp();
                     color = GetColor(COLOR_ACTION);
                  }
                  else
                  {
                     if (ShowUserIDs(d))
                     {
                        temp = "(";
                        temp += optSessionID;
                        temp += ") ";
                     }

                     String t2;
                     GetUserNameForSession(optSessionID, t2);
                     temp += t2;
                     preamble = temp();
                     color = GetColor(COLOR_REMOTE);
                  }
               }

               {
                  // See if we were mentioned as part of the comment.
                  // Only look for the first part of our username (before any
                  // spaces or punctuation), as this is what people will often
                  // shorten our name to.
                  String shortName;
                  GetLocalUserName(shortName);
                  shortName = SubstituteLabelledURLs(shortName).Trim();

                  const unsigned char * orig = (const unsigned char *) shortName();
                  const unsigned char * temp = orig;
                  while(((*temp >= 'A')&&(*temp <= 'Z'))||
                        ((*temp >= 'a')&&(*temp <= 'z'))||
                        ((*temp >= '0')&&(*temp <= '9'))||
                        (*temp == '_')||
                        (*temp >= 0x80)) temp++;
                  if (temp > orig) shortName = shortName.Substring(0, temp-orig);
                   
                  String iText(text);
                  iText = iText.ToUpperCase();
                  shortName = shortName.ToUpperCase();
                  startRed = iText.IndexOf(shortName.Prepend(" "));  // only counts if it's the start of a word!
                  
                  if (startRed >= 0) startRed++;
                                else startRed = (iText.StartsWith(shortName)) ? 0 : -1;  // or if it's the start of text
                  if (startRed >= 0) 
                  {
                     redLen = shortName.Length();

                     char afterName = iText()[startRed+redLen];  // parens important here; avoids potential assertion failure
                     if ((muscleInRange(afterName, 'A', 'Z'))&&(afterName != 'S'))  // allows pluralization without apostrophes though
                     {
                        // Oops, don't trigger after all, to avoid the tim/time problem
                        startRed = -1;
                        redLen = -1;
                     }
                     else
                     {
                        // See if we can't extend the red back out to the original name length....
                        String temp;
                        GetLocalUserName(temp);
                        temp = temp.ToUpperCase(); 
                        const char * c1 = &iText()[startRed+redLen];
                        const char * c2 = &temp()[redLen];
                        while((c1)&&(c2)&&(*c1 == *c2))
                        {
                           c1++;
                           c2++;
                           redLen++;
                        }
                     }
                  }
               }
               if (d == DESTINATION_DISPLAY) ChatTextReceivedBeep(isPersonal, (startRed >= 0));
            break;

            case NUM_LOG_MESSAGE_TYPES:
               // won't happen, this clause is only here to avoid a compiler warning
            break;
         }

         text_run_array style;
         style.count = 1;
         style.runs[0].offset = 0;
         style.runs[0].font = (_fontName.Length() > 0) ? _customBoldFont : *be_bold_font;
         if (_fontSize != 0.0f) style.runs[0].font.SetSize(_fontSize);
         style.runs[0].color = GetColor(COLOR_TEXT);

         // Figure out whether we should scroll down BEFORE inserting text;
         // this way if the user is inserting lots of text it won't affect our decision.
         bool scrollDown = IsScrollBarNearBottom();

         if (ShowTimestamps(d))
         {
            time_t n = time(NULL);
            struct tm * now = localtime(&n);
            char buf[128];
            sprintf(buf, "[%i/%i %i:%02i] ", now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min);
            InsertChatText(d, buf, strlen(buf), &style);
         }

         style.runs[0].color = color;
         InsertChatText(d, preamble, strlen(preamble), &style);

         // now back to black
         style.runs[0].color = *optTextColor;
         style.runs[0].font = (_fontName.Length() > 0) ? _customPlainFont : *be_plain_font;
         if (_fontSize != 0.0f) style.runs[0].font.SetSize(_fontSize);

         InsertChatText(d, ": ", 2, &style);
         if ((startRed >= 0)&&(redLen > 0))
         {
            InsertChatText(d, text, startRed, &style);
            style.runs[0].color = GetColor(COLOR_NAMESAID);
            InsertChatText(d, text+startRed, redLen, &style);
            style.runs[0].color = *optTextColor;
            int slen = strlen(text)-(startRed+redLen);
            if (slen > 0) InsertChatText(d, text+startRed+redLen, slen, &style);
         }
         else InsertChatText(d, text, strlen(text), &style);

         InsertChatText(d, "\n", 1, &style);

         if ((scrollDown)&&(_isScrolling == false)) ScrollToBottom();
      }
   }
}


void ChatWindow :: ScrollToBottom()
{
   BScrollBar * sb = _chatScrollView->ScrollBar(B_VERTICAL);
   if (sb)
   {
      float min, max, smallStep, bigStep;
      sb->GetRange(&min, &max);
      sb->GetSteps(&smallStep, &bigStep);
      sb->SetValue(max-smallStep);
   }
}

void ChatWindow :: ScrollToTop()
{
   if (_isScrolling) return;
   BScrollBar * sb = _chatScrollView->ScrollBar(B_VERTICAL);
   if (sb)
   {
      float min, max;
      sb->GetRange(&min, &max);
      sb->SetValue(min);
   }
}

String ChatWindow :: ProcessURL(const String & url) const
{
   String next(url);

   if ((next.StartsWithIgnoreCase("beshare:") == false)&&(next.StartsWithIgnoreCase("share:") == false)&&(next.StartsWithIgnoreCase("priv:") == false))
   {
      // Remove any non-alphanumeric chars from the end of the URL.
      while(next.Length() > 1)
      {
         char last = next[next.Length()-1];
         if (((last >= '0')&&(last <= '9')) ||
             ((last >= 'a')&&(last <= 'z')) ||
             ((last >= 'A')&&(last <= 'Z')) ||
             (last == '/')||(last == '&')) break;
         else next = next.Substring(0, next.Length()-1);
      }
   }

   return next;
}

int32 ChatWindow :: FindMatchingBracket(const String & s, uint32 & count) const
{
   uint32 slen = s.Length();
   for (uint32 i=0; i<slen; i++)
   {
           if (s[i] == '[') count++;
      else if ((s[i] == ']')&&(--count == 0)) return i;
   }
   return -1;
}

// This callback handles the actual insertion of text into an output destination.
// The ChatWindow version only handles insertion of text into the BTextView.
void ChatWindow :: InsertChatText(LogDestinationType dest, const char * t, int textLen, text_run_array * optStyle)
{
   switch(dest)
   {
      case DESTINATION_DISPLAY:
      {
         if (textLen <= 0) return;

         text_run_array defaultStyle;
         defaultStyle.count = 1;
         defaultStyle.runs[0].offset = 0;
         defaultStyle.runs[0].font = (_fontName.Length() > 0) ? _customPlainFont : *be_plain_font;
         if (_fontSize != 0.0f) defaultStyle.runs[0].font.SetSize(_fontSize);
         defaultStyle.runs[0].color = GetColor(COLOR_TEXT);
         if (optStyle == NULL) optStyle = &defaultStyle;

         Queue<String> urls;
         Queue<String> labels;  // in this list, "" means no label
         Queue<uint32> labelLengths;

         // Get only the specified substring...
         String text(t);
         if ((int)text.Length() > textLen) text = text.Substring(0, textLen);

         // Chop the text into words (ok because spaces aren't allowed in URLs) and check each one
         // URLS of this form:  whatever://blahblah [description here]
         // are a special case:  the [description here] will replace the whatever://blahblah in
         // the text, and the whatever://blahblah will only be visible via tooltip.
         StringTokenizer tok(text(), "\r\n");
         const char * line;
         while((line = tok.GetNextToken()) != NULL)
         {
            bool lastWasURL = false;
            uint32 inLabelCount = 0;
            StringTokenizer subTok(line, " ");
            const char * n;
            while((n = subTok.GetNextToken()) != NULL)
            {
               String next(n);

               if (inLabelCount > 0)
               {
                  int32 rbIndex = FindMatchingBracket(next, inLabelCount);
                  if (rbIndex >= 0) labels.Tail() += next.Substring(0, rbIndex);
                  else 
                  {
                     labels.Tail() += next;
                     labels.Tail() += ' ';
                  }
               }
               else if (IsLink(next()))
               {
                  urls.AddTail(ProcessURL(next));
                  labels.AddTail("");
                  labelLengths.AddTail(0);
                  lastWasURL = true;
               }
               else if ((lastWasURL)&&(next.StartsWith("[")))
               {
                  lastWasURL = false;

                  inLabelCount++;
                  String s = next.Substring(1);
                  int rbIndex = FindMatchingBracket(s, inLabelCount);
                  if (rbIndex >= 0) 
                  {
                     labels.Tail() = s.Substring(0, rbIndex);
                  }
                  else 
                  {
                     labels.Tail() += s;
                     labels.Tail() += ' ';
                  }
               }
            }
            if (inLabelCount > 0) text += ']';  // oops, user forgot to close his bracket :^P
         }

         if (urls.GetNumItems() > 0)
         {
            // Put the URLs in blue underline...
            text_run_array urlStyle;
            urlStyle.count = 1;
            urlStyle.runs[0].offset = 0;
            urlStyle.runs[0].font = optStyle->runs[0].font;
            if (_fontSize != 0.0f) urlStyle.runs[0].font.SetSize(_fontSize);
            urlStyle.runs[0].font.SetFace(urlStyle.runs[0].font.Face() | B_BOLD_FACE);
            urlStyle.runs[0].color = GetColor(COLOR_URL);

            String url;
            String label;
            while((urls.RemoveHead(url) == B_NO_ERROR)&&(labels.RemoveHead(label) == B_NO_ERROR))
            {
               label = label.Trim();

               // Find the next URL...
               int urlIndex = text.IndexOf(url);
               if (urlIndex > 0)
               {
                  // output everything before the URL in the normal style...
                  InsertChatTextAux(text(), urlIndex, optStyle);
                  text = text.Substring(urlIndex);
               }

               // Then output the URL itself
               const String & urlString = (label.Length() > 0) ? label : url;
               _chatText->AddURLRegion(_chatText->TextLength(), urlString.Length(), url);
               InsertChatTextAux(urlString(), urlString.Length(), &urlStyle);
               text = text.Substring(url.Length());

               // Skip past the [label portion], if any
               if (label.Length() > 0)
               {
                  uint32 count = 0;
                  int rbIndex = FindMatchingBracket(text, count);
                  if (rbIndex >= 0) text = text.Substring(rbIndex+1);
                               else text = "";
               }
            }

            if (text.Length() > 0) InsertChatTextAux(text(), text.Length(), optStyle);
         }
         else InsertChatTextAux(t, textLen, optStyle);
      }
      break;
 
      case DESTINATION_LOG_FILE:
      {
         if (_logFile == NULL)
         {
            if (_logsDir.InitCheck() == B_NO_ERROR)
            {
               time_t t = time(NULL);
               struct tm * now = localtime(&t);
               char fileName[256];
               sprintf(fileName, "%s_%02i_%02i_%i_%02i:%02i:%02i.log", GetLogFileNamePrefix(), now->tm_mon+1, now->tm_mday, now->tm_year+1900, now->tm_hour+1, now->tm_min, now->tm_sec);

               BPath path(&_logsDir, fileName);
               _logFile = fopen(path.Path(), "w+");
            }
         }
         if (_logFile) 
         {
            String temp(t);
            if (textLen < (int)temp.Length()) temp = temp.Substring(0, textLen);
            temp = SubstituteLabelledURLs(temp);
            fprintf(_logFile, "%s", temp());
            if (temp.IndexOf('\n') >= 0) fflush(_logFile);
         }
      }
      break;

      default:
         // do nothing
      break;
   }
}

void
ChatWindow ::
InsertChatTextAux(const char * text, int textLen, text_run_array * optStyle)
{
   if (text)
   {
      if (optStyle) _chatText->Insert(_chatText->TextLength(), text, textLen, optStyle);
               else _chatText->Insert(_chatText->TextLength(), text, textLen);
   }
}

void ChatWindow :: InsertDroppedText(const char * u)
{
   BTextView * tv = _textEntry->TextView();
   const char * t = tv->Text();
   if ((tv->TextLength() > 0)&&(t[tv->TextLength()-1] != ' ')) tv->Insert(tv->TextLength(), " ", 1);
   tv->Insert(tv->TextLength(), u, strlen(u));
   tv->Insert(tv->TextLength(), " ", 1);
   _textEntry->MakeFocus();

   int32 tl = tv->TextLength();
   tv->Select(tl, tl);
}
 
void 
ChatWindow ::
DispatchMessage(BMessage * msg, BHandler * target)
{
   switch(msg->what)
   {
      case B_KEY_DOWN:
      {
         int8 c;
         int32 mod;
         if ((msg->FindInt8("byte", &c) == B_NO_ERROR)&&(msg->FindInt32("modifiers", &mod) == B_NO_ERROR)&&((c == B_ENTER)||((mod & B_LEFT_COMMAND_KEY) == 0)))
         {
            BTextView * tv = dynamic_cast<BTextView *>(target);
            
            if ((tv)&&(tv->IsEditable()))
            {
               if (tv == _textEntry->TextView()) 
               {
                  UserChatted();
                  if (c == B_ENTER) 
                  {
                     PostMessage(CHATWINDOW_COMMAND_SEND_CHAT_TEXT);
                     ScrollToBottom();
                  }
               }

               if ((_textEntry->Text()[0])&&(c == B_TAB)&&(tv == _textEntry->TextView())&&((mod & (B_CONTROL_KEY|B_SHIFT_KEY)) == 0))  // don't override the Twitcher!
               {
                  String completed;
                  if (DoTabCompletion(_textEntry->Text(), completed, NULL) == B_NO_ERROR)
                  {
                     _textEntry->SetText(completed());
                     _textEntry->TextView()->Select(completed.Length(), completed.Length());  // move cursor to EOL
                  }
                  else DoBeep(SYSTEM_SOUND_AUTOCOMPLETE_FAILURE);

                  msg = NULL;  // don't do anything further with the message!
               }
               else 
               {
                  int lineDiff = 0;
                  float scrollDiff = 0.0; // for scrolling the chat-window, like in Terminal - added by Hugh
                  float scrollLine = _chatText->LineHeight();
                  float scrollPage = _chatText->Bounds().Height() - scrollLine;  // scroll one line less for better orientation
                  
                  switch(c)
                  {
                     case B_UP_ARROW:
                             if (mod & B_SHIFT_KEY)   lineDiff   = -CHAT_HISTORY_LENGTH;
                        else if (mod & B_CONTROL_KEY) scrollDiff = -scrollLine;   // scroll up one line
                        else                          lineDiff   = -1;
                     break;

                     case B_DOWN_ARROW:
                             if (mod & B_SHIFT_KEY)   lineDiff   = CHAT_HISTORY_LENGTH;
                        else if (mod & B_CONTROL_KEY) scrollDiff = scrollLine;   // scroll down one line
                        else                          lineDiff   = 1;
                     break;

                     case B_PAGE_UP:
                             if (mod & B_SHIFT_KEY)   lineDiff   = -10;   // necessary?
                        else if (mod & B_CONTROL_KEY) scrollDiff = -scrollPage;   // scroll up one page (-1 line)
                     break;

                     case B_PAGE_DOWN:
                             if (mod & B_SHIFT_KEY)   lineDiff   = 10;   // necessary?
                        else if (mod & B_CONTROL_KEY) scrollDiff = scrollPage;   // scroll down one page
                     break;

                     case B_HOME:
                        if (mod & B_CONTROL_KEY)
                        {
                           _chatText->ScrollTo(0.0, 0.0);
                           msg = NULL;
                        }
                     break;

                     case B_END:
                        if (mod & B_CONTROL_KEY)
                        {
                           _chatText->ScrollTo(0.0, _chatText->TextRect().bottom - _chatText->Bounds().Height());
                           msg = NULL;
                        }
                     break;
                  }
                        
                  if ((lineDiff)&&(_chatHistory.GetNumItems() > 0))
                  {
                     int numItems = _chatHistory.GetNumItems();
                     if (_chatHistoryPosition == -1)
                     {
                        if (lineDiff < 0) _chatHistoryPosition = numItems;
                                     else lineDiff = 0;  // at bottom + down arrow = no effect
                     }

                     if (lineDiff)
                     {
                        _chatHistoryPosition += lineDiff;
                        if (_chatHistoryPosition < 0) _chatHistoryPosition = 0;
                        if (_chatHistoryPosition >= numItems) 
                        {
                           _chatHistoryPosition = -1;
                           _textEntry->SetText("");
                        }
                        else
                        {
                           const String * s = _chatHistory.GetItemPointer(_chatHistoryPosition);
                           _textEntry->SetText(s->Cstr());
                           _textEntry->TextView()->Select(s->Length(), s->Length());
                        }
                        msg = NULL;  // don't do anything further with the message!
                     }
                  }
                  else if (scrollDiff)
                  {
                     // check bounds
                     float view_y = _chatText->Bounds().top;               
                     float max_y  = _chatText->TextRect().bottom - _chatText->Bounds().Height();

                     view_y += scrollDiff;   // move view

                     // limit movement to bounds
                     if (view_y < 0.0)   view_y = 0.0;
                     if (view_y > max_y) view_y = max_y;

                     _chatText->ScrollTo(0.0, view_y);

                     msg = NULL;
                  }
               }
            }
            else
            {
                    if ((c == B_ENTER)||((tv == _chatText)&&(c == B_END))) {ScrollToBottom(); msg = NULL;}
               else if ((tv == _chatText)&&(c == B_HOME)) {ScrollToTop(); msg = NULL;}
               else if (c >= ' ')
               {
                  if ((_isScrolling == false)&&(!_textEntry->IsFocus())) ScrollToBottom();
                  _textEntry->MakeFocus();

                  String s(_textEntry->Text());
                  s += c;
                  _textEntry->SetText(s());
                  _textEntry->TextView()->Select(s.Length(), s.Length());
                  msg = NULL;  // don't do anything further with the message!
                  UserChatted();
               }
            }
         }
      }
      break;

      case B_MOUSE_DOWN:
         if ((target == _chatText)&&(_chatText->IsFocus() == false)) _chatText->MakeFocus();
      break;

      case B_SIMPLE_DATA:
         if ((target == _chatText)||(target == _textEntry->TextView()))
         {
            BMessage * tempMsg = msg;  // hold a separate pointer for ourself
            const char * u = NULL;
            if (tempMsg->FindString("be:url", &u) == B_NO_ERROR)
            {
               // If there is a beshare-specific string, use that instead
               String temp;
               const char * bu;
               if (tempMsg->FindString("beshare:link", &bu) == B_NO_ERROR) 
               {
                  u = bu;
                  const char * hr;

                  // If they supplied a human-readable label tag, use that too
                  if (tempMsg->FindString("beshare:desc", &hr) == B_NO_ERROR)
                  {
                     temp = bu;
                     temp += ' ';
                     temp += hr;
                     u = temp();
                  }
               }
   
               InsertDroppedText(u);
               msg = NULL;  // don't pass to superclass!
            }
   
            String linkStr, humanReadableNames, atName;
            entry_ref ref;
            for (int32 i=0; tempMsg->FindRef("refs", i, &ref) == B_NO_ERROR; i++)
            {
               switch(i)
               {
                  case 0:  linkStr += "beshare:"; break;
                  default: linkStr += ',';        break;
               }
   
               String next = ref.name;
               if (humanReadableNames.Length() > 0) humanReadableNames += ", ";
               humanReadableNames += next;
               next.Replace('@', '?');  // @ signs would confuse us!
               next = GetQualifiedSharedFileName(ref.name); 
               int32 atIdx = next.IndexOf('@');
               if (atIdx >= 0)
               {
                  atName = next.Substring(atIdx);   // we won't add the @user till the end!
                  next = next.Substring(0, atIdx);
               }
               EscapeRegexTokens(next);

               linkStr += next;
            }
            if (linkStr.Length() > 0)
            {
               linkStr += atName;
               linkStr.Replace(' ', '?');  // hyperlinks can't have spaces in them
               linkStr += " [";
               linkStr += humanReadableNames;
               linkStr += ']';
               InsertDroppedText(linkStr());
               msg = NULL;  // don't pass to superclass!
            }
         }
      break;
   }

   if (msg) BWindow::DispatchMessage(msg, target);

   if (_isInitialized)
   {
      bool isScrolling = (IsScrollBarNearBottom() == false);
      if (isScrolling != _isScrolling)
      {
         _isScrolling = isScrolling;
         _chatText->SetViewColor(GetColor(isScrolling ? COLOR_SCROLLBG : COLOR_BG));
         PostMessage(CHATWINDOW_COMMAND_INVALIDATE_TEXT_VIEW);
      }
   }
}


void 
ChatWindow ::
SetCommandURLTarget(const BMessenger & target, const BMessage & queryMsg, const BMessage & privMsg)
{
   BMessage setQueryTarget(CHATWINDOW_COMMAND_SET_COMMAND_TARGET);
   setQueryTarget.AddMessenger("target", target);
   setQueryTarget.AddMessage("querymessage", &queryMsg);
   setQueryTarget.AddMessage("privmessage", &privMsg);
   PostMessage(&setQueryTarget);
}


void
ChatWindow ::
DoBeep(const char * which) const
{
#ifdef B_BEOS_VERSION_5
   (void) system_beep(which);
#else
   beep();
#endif
}

void
ChatWindow ::
UserChatted()
{
   // empty
}

bool ChatWindow :: IsScrollBarNearBottom() const 
{
   bool scrollDown = false;
   if (_chatScrollView)
   {
      BScrollBar * sb = _chatScrollView->ScrollBar(B_VERTICAL);
      if (sb) 
      {
         float min, max, smallStep, bigStep;
         sb->GetRange(&min, &max);
         sb->GetSteps(&smallStep, &bigStep);
         scrollDown = (sb->Value() >= max-(smallStep*10));
      }
   }
   return scrollDown;
}

void ChatWindow :: SetFontSize(const String & cmdString)
{
   float fs = (cmdString.Length() > 9) ? atof(cmdString()+10) : 0.0f;
   if (fs < 0.0f) fs = 0.0f;
   SetFontSize(fs);
   if (fs > 0.0f)
   {
      char buf[64];
      sprintf(buf, " %.0f", fs);
      String s(str(STR_FONT_SIZE_SET_TO));
      s += buf;
      LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, this);
   }
   else LogMessage(LOG_INFORMATION_MESSAGE, str(STR_FONT_SIZE_RESET_TO_DEFAULT), NULL, NULL, false, this);
}

void ChatWindow :: SetFont(const String & fontString, bool doLog)
{
   String fs = (fontString.StartsWith("/font")) ? fontString.Substring(5) : fontString;
   fs = fs.Trim();
   if (fs.Length() > 0)
   {
      String useFamily, useStyle;
      int32 which = -1;
      int32 numFamilies = count_font_families(); 
      for (int32 i=0; i<numFamilies; i++) 
      {
         font_family family; 
         uint32 flags; 
         if (get_font_family(i, &family, &flags) == B_NO_ERROR) 
         { 
            if (count_font_styles(family) > 0)
            {
               String fStr(family); 
               if (fStr.IndexOfIgnoreCase(fs) >= 0)
               {
                  font_style style; 
                  if (get_font_style(family, 0, &style, &flags) == B_NO_ERROR)
                  {
                     which = i;
                     useFamily = family;
                     if (fStr.EqualsIgnoreCase(fs)) break;  // if exact match, stop searching here!
                  }
               }
            }
         }
      }

      if (which >= 0)
      {
         _customPlainFont = *be_plain_font;
         _customPlainFont.SetFamilyAndStyle(useFamily(), useStyle());
         _customBoldFont = *be_bold_font;
         _customBoldFont.SetFamilyAndStyle(useFamily(), "Bold");
         _fontName = useFamily;
         if (doLog) 
         {
            String s(str(STR_FONT_SET_TO));
            s += ' ';
            s += _fontName;
            LogMessage(LOG_INFORMATION_MESSAGE, s(), NULL, NULL, false, this);
         }
      }
      else if (doLog) 
      {
         String s = str(STR_COULDNT_FIND_FONT);
         s += ": ";
         s += fs;
         LogMessage(LOG_ERROR_MESSAGE, s(), NULL, NULL, false, this);
      }
   }
   else 
   {
      _fontName = "";
      if (doLog) LogMessage(LOG_INFORMATION_MESSAGE, str(STR_FONT_RESET_TO_DEFAULT), NULL, NULL, false, this);
   }
}

bool ChatWindow :: ColorsDiffer(const rgb_color & c1, const rgb_color & c2) const
{
   return ((c1.red != c2.red)||(c1.green != c2.green)||(c1.blue != c2.blue)||(c1.alpha != c2.alpha));
}

void ChatWindow :: SetViewColor(BView * view, const rgb_color & c)
{
   if (ColorsDiffer(c, view->ViewColor()))
   {
      view->SetViewColor(c);
      view->SetLowColor(c);
      view->Invalidate();
   }
}

void ChatWindow :: UpdateColors()
{
   UpdateTextViewColors(_textEntry->TextView());
   SetViewColor(_chatText, GetColor(IsScrollBarNearBottom() ? COLOR_BG : COLOR_SCROLLBG));

   for (int32 i=_borderViews.GetNumItems()-1; i>=0; i--) SetViewColor(_borderViews[i], GetColor(COLOR_BORDERS));
}

void ChatWindow :: UpdateTextViewColors(BTextView * view)
{
   BFont font;
   uint32 mode;
   rgb_color fontColor;
   view->GetFontAndColor(&font, &mode, &fontColor);

   if ((ColorsDiffer(view->ViewColor(), GetColor(COLOR_BG)))||
       (ColorsDiffer(view->HighColor(), GetColor(COLOR_TEXT)))||
       (ColorsDiffer(view->LowColor(),  GetColor(COLOR_TEXT)))||
       (ColorsDiffer(fontColor,         GetColor(COLOR_TEXT))))
   {
      view->SetViewColor(GetColor(COLOR_BG));
      view->SetLowColor(GetColor(COLOR_BG));
      view->SetHighColor(GetColor(COLOR_TEXT));
      view->SetFontAndColor(&font, mode, &GetColor(COLOR_TEXT));
      view->Invalidate();
   }
}

void ChatWindow :: SetColor(uint32 which, const rgb_color & c) 
{
   _colors[which] = c;
   if ((c.red == 0)&&(c.green == 0)&&(c.blue == 0)) _colors[which].green = 1; // if I don't put this, the text doesn't show up???
}

void ChatWindow :: UpdateColumnListViewColors(ColumnListView * view) 
{
   if ((ColorsDiffer(view->TextColor(), GetColor(COLOR_TEXT)))||
       (ColorsDiffer(view->BgColor(),   GetColor(COLOR_BG)))||
       (ColorsDiffer(view->ViewColor(), GetColor(COLOR_BG)))||
       (ColorsDiffer(view->ItemSelectColor(true), GetColor(COLOR_SELECTION)))||
       (ColorsDiffer(view->ItemSelectColor(false), GetColor(COLOR_SELECTION))))
   {
      view->SetTextColor(GetColor(COLOR_TEXT));
      view->SetBgColor(GetColor(COLOR_BG));
      view->SetViewColor(GetColor(COLOR_BG));
      view->SetItemSelectColor(true, GetColor(COLOR_SELECTION));
      view->SetItemSelectColor(false, GetColor(COLOR_SELECTION));
      view->Invalidate();
   }
}

BView * ChatWindow :: AddBorderView(BView * v)
{
   _borderViews.AddTail(v);
   return v;
}

status_t
ChatWindow :: GetAppSubdir(const char * subDirName, BDirectory & subDir, bool createIfNecessary) const
{
   app_info appInfo;
   be_app->GetAppInfo(&appInfo);
   BEntry appEntry(&appInfo.ref);
   appEntry.GetParent(&appEntry);  // get the directory it's in
   BPath path(&appEntry);
   BPath subPath(&appEntry);
   subPath.Append(subDirName);

   // If the directory is already there, use it
   if (subDir.SetTo(subPath.Path()) == B_NO_ERROR) return B_NO_ERROR;

   // Directory not there?  Shall we create it then?
   if (createIfNecessary)
   {
      BDirectory appDir(path.Path());
      if ((appDir.InitCheck() == B_NO_ERROR)&&(appDir.CreateDirectory(subDirName, &subDir) == B_NO_ERROR)) return B_NO_ERROR;
   }
   return B_ERROR;  // oops, couldn't get it
}

void ChatWindow :: CloseLogFile()
{
   if (_logFile) fclose(_logFile);
   _logFile = NULL;
}

void ChatWindow :: ReadyToRun()
{
   BRect chatViewBounds = GetChatView()->Bounds();

   BRect chatTextBounds(0, 0, chatViewBounds.Width(), chatViewBounds.Height()-(TEXT_ENTRY_HEIGHT+5.0f));
   BBox * box = new BBox(chatTextBounds, NULL, B_FOLLOW_ALL_SIDES);
   GetChatView()->AddChild(box);        

   _chatText = new ReflowingTextView(BRect(2,2,chatTextBounds.Width()-(2+B_V_SCROLL_BAR_WIDTH), chatTextBounds.Height()-2), NULL, chatTextBounds, B_FOLLOW_ALL_SIDES);

   _chatText->MakeEditable(false);
   _chatText->SetStylable(true);

   _chatScrollView = new BScrollView(NULL, _chatText, B_FOLLOW_ALL_SIDES, 0L, false, true, B_FANCY_BORDER);
   box->AddChild(_chatScrollView);

   String chat(str(STR_CHAT_VERB));
   chat += ':';
   _textEntry = new BTextControl(BRect(0, chatViewBounds.Height()-TEXT_ENTRY_HEIGHT, chatViewBounds.Width(), chatViewBounds.Height()), NULL, chat(), NULL, NULL, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
   AddBorderView(_textEntry);

   _textEntry->TextView()->AddFilter(new PasteMessageFilter(CHATWINDOW_COMMAND_SEND_CHAT_TEXT, _textEntry));
   _textEntry->SetDivider(_textEntry->StringWidth(chat())+4.0f);
         
   GetChatView()->AddChild(_textEntry);

   UpdateColors();

   _textEntry->SetTarget(this);
   _textEntry->MakeFocus();
   _isInitialized = true;
}

#ifdef B_BEOS_VERSION_DANO
status_t ChatWindow::UISettingsChanged(const BMessage* /*changes*/, uint32 /*flags*/)
{
   // Patch up the default colors to match the system defaults
   _defaultColors[COLOR_BORDERS]      = ui_color(B_PANEL_BACKGROUND_COLOR);
   _defaultColors[COLOR_BG]           = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
   _defaultColors[COLOR_TEXT]         = ui_color(B_DOCUMENT_TEXT_COLOR);
   _defaultColors[COLOR_SCROLLBG]     = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
   _defaultColors[COLOR_SCROLLBG].red = (tint_color(ui_color(B_DOCUMENT_BACKGROUND_COLOR), 1.07F)).red;

   UpdateColors();
   return B_NO_ERROR;
}
#endif

};  // end namespace beshare
