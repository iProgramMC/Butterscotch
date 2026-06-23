#import "AppDelegate.h"
#import "MainGameController.h"

@interface AppDelegate() {
	MainGameController* mainVC;
}
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
	CGRect screenBounds = [[UIScreen mainScreen] bounds];
	window = [[UIWindow alloc] initWithFrame:screenBounds];

	mainVC = [[MainGameController alloc] init];
	mainVC.view.frame = window.bounds;
	mainVC.view.autoresizingMask =
		UIViewAutoresizingFlexibleWidth |
		UIViewAutoresizingFlexibleHeight;
	
#ifdef IPHONE_OS_3
	[window addSubview:mainVC.view];
#else
	window.rootViewController = mainVC;
#endif

	[window makeKeyAndVisible];

	return YES;
}

- (void)dealloc {
	[mainVC release];
	[window release];
	[super dealloc];
}

@end