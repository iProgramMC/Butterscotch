#import <UIKit/UIKit.h>
#import "MainGameController.h"
#import "EAGLView.h"

extern int initializeGame();

MainGameController* g_pMainGameController;
MainGameController* GetMainGameController() {
	return g_pMainGameController;
}

@interface MainGameController() {
	EAGLView* glView;
}

@end

@implementation MainGameController

- (void)loadView
{
	g_pMainGameController = self;
	
	CGRect screenBounds = [[UIScreen mainScreen] bounds];
	UIView *mainView = [[UIView alloc] initWithFrame:screenBounds];
	mainView.backgroundColor = [UIColor groupTableViewBackgroundColor];
	self.view = mainView;
	[mainView release];
}

- (void)viewDidLoad
{
	CGRect screenBounds = self.view.bounds;
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
    glView = [[EAGLView alloc] initWithFrame:self.view.bounds];
    [self.view addSubview:glView];
	
	//[glView setRenderFrameBuffer:(const uint32_t*)PixBuff withWidth:PixWidth andHeight:PixHeight];
    //[glView startAnimationWithUpdatePeriod:1000000000/60];
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
	initializeGame();
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
}

- (void)dealloc
{
	g_pMainGameController = NULL;
	[glView release];
	[super dealloc];
}

@end
