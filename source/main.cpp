#include "ShareApplication.h"
#include "system/SetupSystem.h"

using namespace beshare;

int main(int argc, char ** argv)
{
	CompleteSetupSystem css;
	ShareApplication beshareApp((argc>1)?argv[1]:NULL); 
	beshareApp.Run();
	return 0;
}
