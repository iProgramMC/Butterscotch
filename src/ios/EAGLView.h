#import <UIKit/UIKit.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/EAGL.h>
#import <QuartzCore/QuartzCore.h>

@interface EAGLView : UIView {
	EAGLContext *context;
	GLuint framebuffer;
	GLuint colorRenderbuffer;
	GLuint framebufferTextureID;
	const uint32_t* renderFrameBuffer;
	int fbWidth;
	int fbHeight;
	uint32_t* renderFramebufferCopy;
	int rfbCopyWidth;
	int rfbCopyHeight;
}

+ (id)alloc;
- (void)dealloc;
- (void)setRenderFrameBuffer:(const uint32_t*)fb withWidth:(int)width andHeight:(int)height;
- (void)startAnimationWithUpdatePeriod:(uint64_t)updatePeriod;
- (void)stopAnimation;
- (void)drawFrame;

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event;

@end
