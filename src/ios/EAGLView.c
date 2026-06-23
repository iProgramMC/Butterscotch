#import "EAGLView.h"
#import "MainGameController.h"
#include <assert.h>
#include <mach/mach_time.h>
#include <dlfcn.h>

#include "runner.h"
#include "gettime.h"
#include "desktop/platformdefs.h"

static uint64_t lastUpdate;
static uint64_t timeBaseNumer, timeBaseDenom;

extern void updateGame();

extern enum GraphicsAPI gfx;

static Runner* g_runner;

static EAGLView* pEAGLView;

static GLint fbWidth  = 0;
static GLint fbHeight = 0;

int NearestPO2(int i) {
	for (int j = 1; j < 1024 * 1024; j *= 2) {
		if (i < j)
			return j;
	}
	
	assert(!"this shouldn't happen");
	return -1;
}

int GetTickCount() {
	return (int)(mach_absolute_time() * timeBaseNumer / timeBaseDenom);
}

@implementation EAGLView

+ (Class)layerClass
{
	return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame
{
	struct mach_timebase_info tb;
	mach_timebase_info(&tb);
	timeBaseNumer = tb.numer;
	timeBaseDenom = tb.denom;
	lastUpdate = mach_absolute_time();
	
	self = [super initWithFrame:frame];
	return self;
}

- (void)setRenderFrameBuffer:(const uint32_t*)fb withWidth:(int)width andHeight:(int)height
{
	renderFrameBuffer = fb;
	fbWidth = width;
	fbHeight = height;
	
	if (renderFramebufferCopy)
		free(renderFramebufferCopy);
	
	int glWidth = NearestPO2(fbWidth), glHeight = NearestPO2(fbHeight);
	renderFramebufferCopy = malloc(sizeof(uint32_t) * glWidth * glHeight);
}

- (void)drawFrame
{
	[EAGLContext setCurrentContext:context];
	
	uint64_t curVal = mach_absolute_time();
	uint64_t diff = curVal - lastUpdate;
	lastUpdate = diff;
	
	uint64_t elapsed_ns = diff * timeBaseNumer / timeBaseDenom;
	int diffMs = (int)(elapsed_ns / 1000000);
	
	fprintf(stderr, "Difference: %d ms (%lld ns).  Corresponds to %.1lf fps.\n", diffMs, diff, 1000.f / diffMs);

	if (!renderFramebufferCopy) {
		return;
	}

	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);

	glClear(GL_COLOR_BUFFER_BIT);
	
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, framebufferTextureID);
	
	float xs = 1, ys = 1;
	
	if (renderFramebufferCopy) {
		int glWidth = NearestPO2(fbWidth), glHeight = NearestPO2(fbHeight);
		xs = (float)(glWidth) / fbWidth;
		ys = (float)(glHeight) / fbHeight;
		
		// copy line by line
		for (int y = 0; y < fbHeight; y++) {
			memcpy(renderFramebufferCopy + y * glWidth, renderFrameBuffer + y * fbWidth, fbWidth * sizeof(uint32_t));
		}
		
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, renderFramebufferCopy);
	}
	
	// Test: draw a simple oscillating triangle
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	GLfloat vertices[] = { 
		// tri 1
		-1, -1,    0, 1.0f / ys,
		-1, 1,     0, 0,
		1, 1,      1.0f / xs, 0,
		
		// tri 2
		-1, -1,    0, 1.0f / ys,
		1, 1,      1.0f / xs, 0,
		1, -1,     1.0f / xs, 1.0f / ys,
	};
	
	// count, type, stride, pointer
	glVertexPointer(2, GL_FLOAT, 4 * sizeof(float), vertices);
	glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), vertices + 2);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindRenderbufferOES(GL_RENDERBUFFER_OES, colorRenderbuffer);
	[context presentRenderbuffer:GL_RENDERBUFFER_OES];
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
}

- (void)performGameLoopOneIteration
{
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
	updateGame();

	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
	[self drawFrame];
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
}

- (void)startAnimationWithUpdatePeriod:(uint64_t)updatePeriod
{
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
	CGFloat updatePeriodInSeconds = (CGFloat)((double)updatePeriod / 1000000000);
	
	[NSTimer scheduledTimerWithTimeInterval:updatePeriodInSeconds
		target:self
		selector:@selector(performGameLoopOneIteration)
		userInfo:nil
		repeats:YES];
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
}

- (void)stopAnimation
{
}

+ (id)alloc
{
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
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

- (void)platformInit
{
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
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
		exit(1);
		return;
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

	[context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:eaglLayer];
	glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, colorRenderbuffer);

	GLint width, height;
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &width);
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &height);
	glViewport(0, 0, width, height);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
}

@end

void Runner_setNextFrame(uint32_t* framebuffer, int width, int height) {
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
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
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
	[pEAGLView platformInit];
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
}

void platformInitFunctions(Runner* runner)
{
    g_runner = runner;
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
	if (pEAGLView == nil){
		fprintf(stderr, "This shit is null?\n");
		fflush(stderr);
	}
	
	[pEAGLView startAnimationWithUpdatePeriod:1000000000/30]; // 30 fps
	
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
}

void platformExit(void)
{
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
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
	fprintf(stderr, "%s   %s:%d\n", __func__, __FILE__, __LINE__);
	fflush(stderr);
	
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
    *outW = fbWidth;
    *outH = fbHeight;
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
