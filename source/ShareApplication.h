#ifndef SHAREAPPLICATION_H
#define SHAREAPPLICATION_H

#include <Application.h>
#include <Entry.h>

#include "ShareWindow.h"

namespace beshare {

class ShareWindow;

class ShareApplication : public BApplication
{
public:
	ShareApplication(const char * optConnectTo);
	~ShareApplication();

	virtual void RefsReceived(BMessage * msg);
	virtual void ReadyToRun();
	virtual void DispatchMessage(BMessage * msg, BHandler * handler);

	void SaveSettings(const BMessage & settingsMsg);

	virtual void MessageReceived(BMessage * msg);

private:
	ShareWindow* fWindow;
	BEntry 		fSettingsFileEntry;
	const char*	fOptConnectTo;
};

};  // end namespace beshare

#endif
