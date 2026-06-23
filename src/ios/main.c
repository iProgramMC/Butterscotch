#import <UIKit/UIKit.h>
#import "AppDelegate.h"

int main(int argc, char *argv[])
{
	//freopen("/var/mobile/debug.log", "w", stderr);
    freopen("/tmp/bsout", "w", stderr);
    freopen("/tmp/bsout", "w", stdout);
    setbuf(stdout, NULL);
	
	int retVal;
	@autoreleasepool
	{
		retVal = UIApplicationMain(argc, argv, nil, @"AppDelegate");
	}
	
	return retVal;
}
