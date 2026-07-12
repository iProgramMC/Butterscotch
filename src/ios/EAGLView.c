#import <objc/runtime.h>
#import "EAGLView.h"
#import "MainGameController.h"
#include <assert.h>
#include <mach/mach_time.h>
#include <dlfcn.h>

#include "runner.h"
#include "gettime.h"
#include "desktop/platformdefs.h"

extern void updateGame();

extern enum GraphicsAPI gfx;

extern float fGameScale;

static Runner* g_runner;

static EAGLView* pEAGLView;

static GLint fbWidth  = 0;
static GLint fbHeight = 0;

static GLuint controlsTextureID = 0;

enum {
	BUTTON_L,
	BUTTON_U,
	BUTTON_D,
	BUTTON_R,
	BUTTON_Z,
	BUTTON_X,
	BUTTON_C,
	BUTTON_COUNT
};

static bool lastButtonStates[BUTTON_COUNT];
static bool buttonStates[BUTTON_COUNT];

static void getButtonIndicesFromPosition(int x, int y, int* indices, int* indexCount)
{
	CGRect rect = [[UIScreen mainScreen] bounds];
	
	x = (int)(x * 320.0f / rect.size.width);
	y = (int)(y * 480.0f / rect.size.height);
	
	*indexCount = 0;
	
	if (y < 340) return; // not in the bottom rectangle. normally 360
	
	//normally I'd also exclude 120-200
	
	if (x < 160) {
		if (x <= 40) indices[(*indexCount)++] = BUTTON_L;
		if (x >= 80) indices[(*indexCount)++] = BUTTON_R;
		if (y <= 400) indices[(*indexCount)++] = BUTTON_U;
		if (y >= 440) indices[(*indexCount)++] = BUTTON_D;
	}
	else {
		if (y < 420) indices[(*indexCount)++] = BUTTON_Z;
		else if (x < 260) indices[(*indexCount)++] = BUTTON_X;
		else indices[(*indexCount)++] = BUTTON_C;
	}
	
	assert((*indexCount) < 2);
}

static int buttonIndexToGml(int keyId)
{
	switch (keyId) {
		case BUTTON_L: return VK_LEFT;
		case BUTTON_R: return VK_RIGHT;
		case BUTTON_U: return VK_UP;
		case BUTTON_D: return VK_DOWN;
		case BUTTON_Z: return 'Z';
		case BUTTON_X: return 'X';
		case BUTTON_C: return 'C';
	}
	return -1;
}

#define MAX_TOUCHES 10

static void* touchPtrs[MAX_TOUCHES];
static int touchCount = 0;

int NearestPO2(int i) {
	for (int j = 1; j < 1024 * 1024; j *= 2) {
		if (i < j)
			return j;
	}
	
	assert(!"this shouldn't happen");
	return -1;
}

@implementation EAGLView

+ (Class)layerClass
{
	return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame
{
	self = [super initWithFrame:frame];
	
	return self;
}

- (void)loadControlsTexture
{
	UIImage *image = [UIImage imageNamed:@"controls"];

	CGImageRef cgImage = image.CGImage;
	size_t width = CGImageGetWidth(cgImage);
	size_t height = CGImageGetHeight(cgImage);

	GLubyte *data = (GLubyte *)calloc(width * height * 4, sizeof(GLubyte));

	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGContextRef cgContext = CGBitmapContextCreate(
		data,
		width,
		height,
		8,
		width * 4,
		colorSpace,
		kCGImageAlphaPremultipliedLast
	);

	CGContextTranslateCTM(cgContext, 0, height);
	CGContextScaleCTM(cgContext, 1.0f, -1.0f);
	CGContextDrawImage(cgContext, CGRectMake(0, 0, width, height), cgImage);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		(GLsizei)width,
		(GLsizei)height,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		data
	);
	
	controlsTextureID = texture;

	CGContextRelease(cgContext);
	CGColorSpaceRelease(colorSpace);
	free(data);
}

- (void)setRenderFrameBuffer:(const uint32_t*)fb withWidth:(int)width andHeight:(int)height
{
	renderFrameBuffer = fb;
	fbWidth = width;
	fbHeight = height;
	
	int glWidth = NearestPO2(fbWidth), glHeight = NearestPO2(fbHeight);
	if (rfbCopyWidth != glWidth || rfbCopyHeight != glHeight)
	{
		if (renderFramebufferCopy)
			free(renderFramebufferCopy);
		
		size_t rfbSize = sizeof(uint32_t) * glWidth * glHeight;
		renderFramebufferCopy = malloc(rfbSize);
		memset(renderFramebufferCopy, 0, rfbSize);
		rfbCopyWidth = glWidth;
		rfbCopyHeight = glHeight;
	}
}

- (void)drawFrame
{
	[EAGLContext setCurrentContext:context];
	
	if (!renderFramebufferCopy) {
		return;
	}

	glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);

	glClear(GL_COLOR_BUFFER_BIT);
	
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, framebufferTextureID);
	
	float xs = 1, ys = 1;
	
	if (renderFramebufferCopy) {
		int glWidth = rfbCopyWidth, glHeight = rfbCopyHeight;
		
		CGRect bounds = [[UIScreen mainScreen] bounds];
		
		// TODO: magic value, fix it!
		xs = (float)(glWidth) / bounds.size.width;
		ys = (float)(glHeight) / bounds.size.height;
		
		// copy line by line
		for (int y = 0; y < fbHeight; y++) {
			//memcpy(renderFramebufferCopy + y * glWidth, renderFrameBuffer + y * fbWidth, fbWidth * sizeof(uint32_t));
			
			uint32_t* dstline = renderFramebufferCopy + y * glWidth;
			const uint32_t* srcline = renderFrameBuffer + y * fbWidth;
			for (int x = 0; x < fbWidth; x++) {
				uint32_t swapped = srcline[x];
				swapped = (swapped & 0xFF00FF00) | ((swapped & 0xFF) << 16) | ((swapped & 0xFF0000) >> 16);
				dstline[x] = swapped;
			}
		}
		
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, renderFramebufferCopy);
	}
	
	// Test: draw a simple oscillating triangle
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	GLfloat vertices[] = { 
		// tri 1
		-1, 0,    0, 0.5f / ys,
		-1, 1,    0, 0,
		1, 1,     1.0f / xs, 0,
		
		// tri 2
		-1, 0,    0, 0.5f / ys,
		1, 1,     1.0f / xs, 0,
		1, 0,     1.0f / xs, 0.5f / ys,
	};
	
	// count, type, stride, pointer
	glVertexPointer(2, GL_FLOAT, 4 * sizeof(float), vertices);
	glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), vertices + 2);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	glBindTexture(GL_TEXTURE_2D, controlsTextureID);
	
	float cxs = 320.0f / 512.0f;
	float cys = 240.0f / 256.0f;
	
	GLfloat vertices2[] = {
		// tri 1
		-1, -1, 0, 0,    // lower left
		1, -1, cxs, 0,   // lower right
		1, 0, cxs, cys, // upper right
		
		// tri 2
		-1, -1, 0, 0,    // lower left
		1, 0, cxs, cys, // upper right
		-1, 0, 0, cys,  // upper left
	};
	
	// count, type, stride, pointer
	glVertexPointer(2, GL_FLOAT, 4 * sizeof(float), vertices2);
	glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), vertices2 + 2);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindRenderbufferOES(GL_RENDERBUFFER_OES, colorRenderbuffer);
	[context presentRenderbuffer:GL_RENDERBUFFER_OES];
}

- (void)performGameLoopOneIteration
{
	static uint64_t finishedLast = 0;
	uint64_t start = nowNanos();
	updateGame();
	uint64_t endUpdate = nowNanos();
	[self drawFrame];
	uint64_t end = nowNanos();
	
	fprintf(stderr, "took %lld us (draw %lld us, update %lld us. between game loops %lld us)\n",
		(end-start)/1000, (end-endUpdate)/1000, (endUpdate-start)/1000, (start-finishedLast)/1000);
	fflush(stderr);
	
	finishedLast = end;
}

- (void)startAnimationWithUpdatePeriod:(uint64_t)updatePeriod
{
	CGFloat updatePeriodInSeconds = (CGFloat)((double)updatePeriod / 1000000000);
	
	[NSTimer scheduledTimerWithTimeInterval:updatePeriodInSeconds
		target:self
		selector:@selector(performGameLoopOneIteration)
		userInfo:nil
		repeats:YES];
	
	[self loadControlsTexture];
}

- (void)stopAnimation
{
}

+ (id)alloc
{
	id that = [super alloc];
	pEAGLView = that;
	return that;
}

- (void)dealloc
{
	if (framebuffer) glDeleteFramebuffersOES(1, &framebuffer);
	if (colorRenderbuffer) glDeleteRenderbuffersOES(1, &colorRenderbuffer);
	[context release];
	[super dealloc];
	
	pEAGLView = nil;
}

- (void)updateTouchControls
{
	memset(buttonStates, 0, sizeof buttonStates);

	for (int i = 0; i < touchCount; i++)
	{
		UITouch *touch = (UITouch*) touchPtrs[i];
		CGPoint p = [touch locationInView:self];

		fprintf(stderr, "\ttouch: %d, %d\n", (int) p.x, (int) p.y);

		int buttons[2];
		int count = 0;
		getButtonIndicesFromPosition((int)p.x, (int)p.y, buttons, &count);
		
		for (int i = 0; i < count; i++) {
			buttonStates[buttons[i]] = true;
		}
	}
	
	memcpy(lastButtonStates, buttonStates, sizeof buttonStates);
	for (int i = 0; i < BUTTON_COUNT; i++)
	{
		if (lastButtonStates[i] && !buttonStates[i]) {
			fprintf(stderr, "xdddd up %d\n", i);
			RunnerKeyboard_onKeyUp(g_runner->keyboard, buttonIndexToGml(i));
		}
		if (!lastButtonStates[i] && buttonStates[i]) {
			fprintf(stderr, "xdddd down %d\n", i);
			RunnerKeyboard_onKeyDown(g_runner->keyboard, buttonIndexToGml(i));
		}
	}
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	for (UITouch *touch in touches)
	{
		if (touchCount >= MAX_TOUCHES) continue; // drop
		
		bool found = false;
		for (int i = 0; i < touchCount; i++) {
			if (touchPtrs[i] == touch) {
				found = true;
				break;
			}
		}
		
		if (!found)
			touchPtrs[touchCount++] = touch;
	}
	
	[self updateTouchControls];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self updateTouchControls];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	for (UITouch *touch in touches)
	{
		int place = -1;
		for (int i = 0; i < touchCount; i++) {
			if (touchPtrs[i] == touch) {
				place = i;
				break;
			}
		}
		
		if (place >= 0) {
			touchPtrs[place] = touchPtrs[--touchCount];
			touchPtrs[touchCount] = NULL;
		}
	}
	
	[self updateTouchControls];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	[self touchesEnded:touches withEvent:event];
}

- (BOOL)platformInit
{
	CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
	eaglLayer.opaque = YES;
	
#ifdef ENABLE_MODERN_GL
	if (gfx == MODERN_GL)
		context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
#endif
#ifdef ENABLE_SW_RENDERER
	if (gfx == SOFTWARE)
		context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
#endif

	if (!context || ![EAGLContext setCurrentContext:context]) {
		fprintf(stderr, "Failed to create and set the OpenGL ES context.\n");
		[self release];
		return NO;
	}
	
	glGenFramebuffersOES(1, &framebuffer);
	glGenRenderbuffersOES(1, &colorRenderbuffer);

	glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);
	glBindRenderbufferOES(GL_RENDERBUFFER_OES, colorRenderbuffer);
	
	// set up framebuffer texture
	glGenTextures(1, &framebufferTextureID);
	glBindTexture(GL_TEXTURE_2D, framebufferTextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	[context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:eaglLayer];
	glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, colorRenderbuffer);

	GLint width, height;
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &width);
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &height);
	glViewport(0, 0, width, height);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	[self setMultipleTouchEnabled:YES];
	[self becomeFirstResponder];

	return YES;
}

@end

void Runner_setNextFrame(uint32_t* framebuffer, int width, int height) {
    [pEAGLView setRenderFrameBuffer:framebuffer withWidth:width andHeight:height];
}

bool platformInit(int32_t reqW, int32_t reqH, const char *title, bool headless)
{
	(void) reqW;
	(void) reqH;
	(void) title;
	(void) headless;
	
	fbWidth = reqW;
	fbHeight = reqH;
	
	return [pEAGLView platformInit];
}

void platformInitFunctions(Runner* runner)
{
    g_runner = runner;
	
	[pEAGLView startAnimationWithUpdatePeriod:1000000000/30]; // 30 fps
}

void platformExit(void)
{
	// nothing
}

void platformSwapBuffers(void)
{
	// nothing
}

void *platformGetProcAddress(const char *name)
{
    return dlsym(RTLD_NEXT, name);
}

bool platformHandleEvents(void)
{
	return false;
}

/* TODO: touchscreen mouse support */
void platformGetMousePos(double *xPos, double *yPos)
{
    *xPos = 0.0;
    *yPos = 0.0;
}

bool platformGetWindowSize(int32_t* outW, int32_t* outH) {
    if (!outW || !outH) return false;
    if (fbWidth <= 0 || fbHeight <= 0) return false;
    *outW = (int)((float)fbWidth * fGameScale);
    *outH = (int)((float)fbHeight * fGameScale);
    return true;
}

bool platformGetScaledWindowSize(int32_t* outW, int32_t* outH) {
    if (!outW || !outH) return false;
    CGRect bounds = [[UIScreen mainScreen] bounds];
    if (bounds.size.width <= 0 || bounds.size.height <= 0) return false;
    *outW = bounds.size.width;
    *outH = bounds.size.height;
    return true;
}

void platformSetWindowSize(int32_t width, int32_t height)
{
    (void)width; (void)height;
}

void platformSetWindowTitle(const char* title)
{
    (void)title;
}

void platformSleepUntil(uint64_t time) {
    int64_t remaining = time - nowNanos();
    if (remaining > 2000000) {
        remaining -= 1000000;
        struct timespec ts;
        ts.tv_sec  = 0;
        ts.tv_nsec = remaining;
        nanosleep(&ts, NULL);
    }
    while (nowNanos() < time) {
        // Spin-wait for the remaining sub-millisecond
        YIELD();
    }
}
