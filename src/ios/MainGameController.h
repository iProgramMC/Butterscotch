#pragma once
#import <UIKit/UIKit.h>

@interface MainGameController : UIViewController {
}

- (BOOL) prefersStatusBarHidden;

@end

MainGameController* GetMainGameController();
