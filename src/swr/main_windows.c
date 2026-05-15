#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdbool.h>

#include "runner.h"
#include "runner_keyboard.h"
#include "data_win.h"
#include "vm.h"
#include "vm_builtins.h"
#include "overlay_file_system.h"
#include "audio_system.h"
#include "sw_renderer.h"
#include "fb_convert.h"
#ifdef USE_MINIAUDIO
#include "audio/miniaudio/ma_audio_system.h"
#else
#include "noop_audio_system.h"
#endif

// ========== Configuration ==========
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

static const char* pDataWinPath = "./data/data.win";
static const char* pDataWinDir = "./data";
static const char* pSaveDataDir = "./data";

static bool bLazilyLoadRooms = false;
static bool bDebugMode = true;
static bool bTraceFrames = false;
static bool bTraceFPS = true;
static YoYoOperatingSystem nOsType = OS_WINDOWS;

static int nBeginningRoom = -1;
static int nProfilerFramesBetween = 0;
static int nSetSeed = 0;
static int nExitAtFrame = -1;
static float fFastForwardSpeed = 1000.0f;
static float fSpeedMultiplier = 1.0f;

#ifdef ENABLE_VM_OPCODE_PROFILER
static bool bOpcodeProfilerEnabled = false;
#endif
// ========== Configuration END ==========

static HWND hWnd;
static DataWin* pDataWin;
static Gen8* pGen8;
static VMContext* pVMContext;
static FileSystem* pOverlayFS;
static AudioSystem* pAudioSystem;
static Runner* pRunner;
static Renderer* pRenderer;
static bool bWantToQuit = false;
static bool bDebugPaused = false;
static bool bDebugShowCollisionMasks = false;
static bool bShowFPS = true;
static double lastFrameTime;

static bool keysPressed[256];
static bool keysPressedNextFrame[256];
static bool keysReleasedNextFrame[256];

static double swrGetTime()
{
	return GetTickCount() / 1000.0;
}

static uint64_t swrGetTimePrecise()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

static uint64_t swrGetTimePreciseFrequency()
{
	static bool initted = false;
	static uint64_t value = 0;
	if (initted)
		return value;
	
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	value = li.QuadPart;
	initted = true;
	
	return value;
}

static uint8_t winKeyToGMLKey(uint8_t wParam)
{
	// GML uses key constants which match Win32's
	return wParam;
}

static void keyPressed(uint8_t key)
{
	keysPressed[key] = true;
	keysPressedNextFrame[key] = true;
}

static void keyReleased(uint8_t key)
{
	keysPressed[key] = false;
	keysReleasedNextFrame[key] = true;
}

static uint32_t* nextFrameBuffer = NULL;
static int nextFrameBufferWidth = 0;
static int nextFrameBufferHeight = 0;

static void swrDrawFullFrameBufferToDevice(HDC hdc, uint32_t* framebuffer, int width, int height)
{
	static uint16_t* altFramebuffer16 = NULL;
	static uint8_t* altFramebuffer24 = NULL;
	static uint8_t* altFramebuffer8 = NULL;
	
	static int framebufferPreference = 32;

	int rc = 0;

	BITMAPINFO bmi = {0};
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = width;
	bmi.bmiHeader.biHeight      = -height;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biCompression = BI_RGB;

	if (framebufferPreference == 32)
	{
		bmi.bmiHeader.biBitCount = 32;
		rc = SetDIBitsToDevice(hdc, 0, 0, width, height, 0, 0, 0, height, framebuffer, &bmi, DIB_RGB_COLORS);
		
		if (rc != 0)
			return;
		
		MessageBoxA(hWnd, "Couldn't draw a 32-bit frame buffer, trying 24.", "Butterscotch", MB_OK);
		framebufferPreference = 24;
	}
	
	if (framebufferPreference == 24)
	{
		// try a 24-bit framebuffer
		bmi.bmiHeader.biBitCount = 24;
		
		altFramebuffer24 = swrConvert32to24(altFramebuffer24, framebuffer, width, height);
		rc = SetDIBitsToDevice(hdc, 0, 0, width, height, 0, 0, 0, height, altFramebuffer24, &bmi, DIB_RGB_COLORS);
		
		if (rc != 0)
			return;
		
		MessageBoxA(hWnd, "Couldn't draw a 24-bit frame buffer, trying 16.", "Butterscotch", MB_OK);
		free(altFramebuffer24);
		altFramebuffer24 = NULL;
		framebufferPreference = 16;
	}
	
	if (framebufferPreference == 16)
	{
		// try 16-bit framebuffer
		bmi.bmiHeader.biBitCount = 16;
		
		altFramebuffer16 = swrConvert32to16(altFramebuffer16, framebuffer, width, height);
		rc = SetDIBitsToDevice(hdc, 0, 0, width, height, 0, 0, 0, height, altFramebuffer16, &bmi, DIB_RGB_COLORS);
		
		if (rc != 0)
			return;
		
		MessageBoxA(hWnd, "Couldn't draw a 16-bit frame buffer, trying 8.", "Butterscotch", MB_OK);
		free(altFramebuffer16);
		altFramebuffer16 = NULL;
		framebufferPreference = 8;
	}
	
	// try 8-bit framebuffer
	altFramebuffer8 = swrConvert32to8(altFramebuffer8, framebuffer, width, height);
	rc = SetDIBitsToDevice(hdc, 0, 0, width, height, 0, 0, 0, height, altFramebuffer8, swrSetup8BitBitmapInfo(width, height), DIB_RGB_COLORS);
	
	if (rc == 0)
	{
		char buffer[512];
		snprintf(buffer, sizeof buffer,
			"Oops! Couldn't call SetDIBitsToDevice for either 32-bit, 24-bit, 16-bit, or 8-bit frame buffers. "
			"The last error is %d.", (int) GetLastError()
		);
		MessageBoxA(hWnd, buffer, "Butterscotch", MB_OK | MB_ICONERROR);
		exit(1);
	}
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_DESTROY:
			bWantToQuit = true;
			PostQuitMessage(0);
			return 0;
		
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			
			if (nextFrameBufferWidth != 0 && nextFrameBufferHeight != 0)
				swrDrawFullFrameBufferToDevice(hdc, nextFrameBuffer, nextFrameBufferWidth, nextFrameBufferHeight);
			
			EndPaint(hWnd, &ps);
			break;
		}

		case WM_KEYDOWN:
			if (wParam >= 2 && wParam < 256)
				keyPressed(winKeyToGMLKey(wParam));
			break;
		
		case WM_KEYUP:
			if (wParam >= 2 && wParam < 256)
				keyReleased(winKeyToGMLKey(wParam));
			break;
		
		case WM_ACTIVATE:
			if (wParam == WA_INACTIVE && (HWND) lParam == hWnd)
			{
				memset(keysPressedNextFrame, 0, sizeof keysPressedNextFrame);
				memset(keysReleasedNextFrame, 1, sizeof keysReleasedNextFrame);
			}
			break;
	}
	
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Runner_setNextFrame(uint32_t* framebuffer, int width, int height)
{
	if (!nextFrameBuffer) {
		nextFrameBuffer = safeCalloc(sizeof(uint32_t), width * height);
	}
	
	memcpy(nextFrameBuffer, framebuffer, sizeof(uint32_t) * width * height);
	nextFrameBufferWidth = width;
	nextFrameBufferHeight = height;
}

static int swrCompareRects(const void* rc1v, const void* rc2v)
{
	const RECT *rc1 = rc1v, *rc2 = rc2v;
	if (rc1->top != rc2->top) return (rc1->top < rc2->top) ? -1 : 1;
	if (rc1->left != rc2->left) return (rc1->left < rc2->left) ? -1 : 1;
	if (rc1->bottom != rc2->bottom) return (rc1->bottom < rc2->bottom) ? -1 : 1;
	if (rc1->right != rc2->right) return (rc1->right < rc2->right) ? -1 : 1;
	return 0;
}

static void swrDrawFrameBufferToDevice(HDC hdc, uint32_t* framebuffer, int width, int height)
{
	SWRectangle* updateRects;
	int rectCount;
	bool rectOverflow;
	
	updateRects = SWRenderer_getUpdateRects(pRenderer, &rectCount, &rectOverflow);
	
	RECT fullRect = { 0, 0, width, height };
	
	if (rectOverflow)
	{
		InvalidateRect(hWnd, &fullRect, FALSE);
		return;
	}
	
	// WIN32S: This isn't supported, try something else
	size_t bufferSize = sizeof(RGNDATAHEADER) + sizeof(RECT) * rectCount;
	RGNDATA* rgnData = safeCalloc(1, bufferSize);
	rgnData->rdh.dwSize = sizeof(rgnData->rdh);
	rgnData->rdh.iType = RDH_RECTANGLES;
	rgnData->rdh.nCount = rectCount;
	rgnData->rdh.nRgnSize = bufferSize;
	
	RECT extentRect = { 9999, 9999, -9999, -9999 };
	RECT* rects = (RECT*) rgnData->Buffer;
	int actualRectCount = 0;
	for (int i = 0; i < rectCount; i++)
	{
		SWRectangle *rc = &updateRects[i];
		RECT *drc = &rects[actualRectCount];
		drc->left = rc->x1;
		drc->top = rc->y1;
		drc->right = rc->x2;
		drc->bottom = rc->y2;

		if (drc->left < 0) drc->left = 0;
		if (drc->top < 0) drc->top = 0;
		if (drc->right > width) drc->right = width;
		if (drc->bottom > height) drc->bottom = height;

		if (drc->left == drc->right) continue;

		//swap degenerated rectangles if needed
		if (drc->left > drc->right) { int tmp = drc->right; drc->right = drc->left; drc->left = tmp; }
		if (drc->top > drc->bottom) { int tmp = drc->bottom; drc->bottom = drc->top; drc->top = tmp; }
		
		//calculate the actual extent rectangle
		extentRect.left = swrMin(extentRect.left, drc->left);
		extentRect.top = swrMin(extentRect.top, drc->top);
		extentRect.right = swrMax(extentRect.right, drc->right);
		extentRect.bottom = swrMax(extentRect.bottom, drc->bottom);
		
		actualRectCount++;
	}
	
	if (extentRect.left < 0) extentRect.left = 0;
	if (extentRect.top < 0) extentRect.top = 0;
	if (extentRect.right > WINDOW_WIDTH) extentRect.right = WINDOW_WIDTH;
	if (extentRect.bottom > WINDOW_HEIGHT) extentRect.bottom = WINDOW_HEIGHT;
	
	if (!actualRectCount)
	{
		printf("Nothing to draw...\n");
		free(rgnData);
		return;
	}
	
	rgnData->rdh.rcBound = extentRect;
	rgnData->rdh.nCount = actualRectCount;
	bufferSize = sizeof(RGNDATAHEADER) + sizeof(RECT) * actualRectCount;
	rgnData->rdh.nRgnSize = bufferSize-sizeof(RGNDATAHEADER);
	
	// Sort rectangles by position
	qsort(rects, actualRectCount, sizeof(RECT), swrCompareRects);
	
	HRGN fullRgn = ExtCreateRegion(NULL, bufferSize, rgnData);
	if (fullRgn)
	{
		InvalidateRgn(hWnd, fullRgn, FALSE);
		DeleteObject(fullRgn);
	}
	else
	{
		printf("Couldn't create full RGN...  Last Error: %d.  Buffer Size: %zu   RectCount: %d\n", GetLastError(), bufferSize, actualRectCount);
		InvalidateRect(hWnd, &fullRect, FALSE);
	}
	
	free(rgnData);
}

static void swrFlushFrameBuffer(uint64_t stepTicks, uint64_t renderTicks)
{
	HDC hdc = GetDC(hWnd);
	
	uint64_t start = swrGetTimePrecise();
	
	swrDrawFrameBufferToDevice(hdc, nextFrameBuffer, nextFrameBufferWidth, nextFrameBufferHeight);
	
	uint64_t flushTicks = swrGetTimePrecise() - start;
	
	if (bShowFPS)
	{
		static int fps = 0, fpsCount = 0;
		static int tickLastCounted = 0;
		
		if (GetTickCount() - tickLastCounted >= 1000)
		{
			if (tickLastCounted == 0)
				tickLastCounted = GetTickCount();
			else
				tickLastCounted += 1000;
			
			fps = fpsCount;
			fpsCount = 0;
		}
		
		fpsCount++;
		
		uint64_t total = stepTicks + renderTicks + flushTicks;
		
		int stepPercentage = (int)(stepTicks * 10000 / total);
		int renderPercentage = (int)(renderTicks * 10000 / total);
		int flushPercentage = (int)(flushTicks * 10000 / total);
		
		char buffer[128];
		snprintf(
			buffer,
			sizeof buffer, 
			"FPS: %d  STEP: %d.%02d%%, RENDER: %d.%02d%%, FLUSH: %d.%02d%%",
			fps,
			stepPercentage / 100, stepPercentage % 100,
			renderPercentage / 100, renderPercentage % 100,
			flushPercentage / 100, flushPercentage % 100
		);
		
		COLORREF oldColor = SetTextColor(hdc, RGB(255,255,255));
		int oldBkMode = SetBkMode(hdc, TRANSPARENT);
		TextOutA(hdc, 0, 0, buffer, (int) strlen(buffer));
		SetTextColor(hdc, oldColor);
		SetBkMode(hdc, oldBkMode);
		
		if (bTraceFPS)
			printf("%s\n", buffer);
	}

	ReleaseDC(hWnd, hdc);
}

static bool swrIsKeyPressed(int keyCode)
{
	if (keyCode < 0 || keyCode >= 256)
		return false;
	
	return keysPressed[keyCode];
}

void RunnerSWR_setWindowTitle(void* pWindowHandle, const char *pTitle)
{
	HWND hWnd = (HWND) pWindowHandle;
	char windowTitle[256];
	snprintf(windowTitle, sizeof(windowTitle), "Butterscotch[SW] - %s", pTitle);
	SetWindowTextA(hWnd, windowTitle);
}

bool RunnerSWR_windowHasFocus(void* pWindowHandle)
{
	HWND hWnd = (HWND) pWindowHandle;
	return GetForegroundWindow() == hWnd;
}

void initializeGame()
{
	pDataWin = DataWin_parse(
		pDataWinPath,
		(DataWinParserOptions) {
			.parseGen8 = true,
			.parseOptn = true,
			.parseLang = true,
			.parseExtn = false,
			.parseSond = true,
			.parseAgrp = true,
			.parseSprt = true,
			.parseBgnd = true,
			.parsePath = true,
			.parseScpt = true,
			.parseGlob = true,
			.parseShdr = true,
			.parseFont = true,
			.parseTmln = true,
			.parseObjt = true,
			.parseRoom = true,
			.parseTpag = true,
			.parseCode = true,
			.parseVari = true,
			.parseFunc = true,
			.parseStrg = true,
			.parseTxtr = true,
#ifdef USE_MINIAUDIO
			.parseAudo = true,
#else
			.parseAudo = false,
#endif
			.skipLoadingPreciseMasksForNonPreciseSprites = true,
			.lazyLoadRooms = bLazilyLoadRooms,
			.eagerlyLoadedRooms = NULL
		}
	);

	pGen8 = &pDataWin->gen8;
	fprintf(stderr, "Loaded \"%s\" (%d) successfully! [Bytecode Version %u / GameMaker version %u.%u.%u.%u]\n", pGen8->name, pGen8->gameID, pGen8->bytecodeVersion, pDataWin->detectedFormat.major, pDataWin->detectedFormat.minor, pDataWin->detectedFormat.release, pDataWin->detectedFormat.build);

	RunnerSWR_setWindowTitle(hWnd, pGen8->displayName);
	
	VMContext *vm = VM_create(pDataWin);
	pVMContext = vm;
	
	Profiler_setEnabled(&pVMContext->profiler, nProfilerFramesBetween > 0);
#ifdef ENABLE_VM_OPCODE_PROFILER
	vm->opcodeProfilerEnabled = bOpcodeProfilerEnabled;
	if (vm->opcodeProfilerEnabled) {
		vm->opcodeVariantCounts = safeCalloc(256 * 256, sizeof(uint64_t));
		vm->opcodeRValueTypeCounts = safeCalloc(256 * 256, sizeof(uint64_t));
	}
#endif

	if (nSetSeed) {
		srand((unsigned int) nSetSeed);
		vm->hasFixedSeed = true;
		fprintf(stderr, "Using fixed RNG seed: %d\n", nSetSeed);
	}
	
	pOverlayFS = (FileSystem*) OverlayFileSystem_create(pDataWinDir, pSaveDataDir);
	
#ifdef USE_MINIAUDIO
	pAudioSystem = (AudioSystem*) MaAudioSystem_create();
#else
	pAudioSystem = (AudioSystem*) NoopAudioSystem_create();
#endif
	
	pRenderer = SWRenderer_create(WINDOW_WIDTH, WINDOW_HEIGHT);
	
	pRunner = Runner_create(pDataWin, pVMContext, pRenderer, pOverlayFS, pAudioSystem);
	pRunner->debugMode = bDebugMode;
	pRunner->osType = nOsType;
	pRunner->setWindowTitle = RunnerSWR_setWindowTitle;
	pRunner->windowHasFocus = RunnerSWR_windowHasFocus;
	pRunner->nativeWindow = (void*) hWnd;

	Runner_initFirstRoom(pRunner);
	
	lastFrameTime = swrGetTime();
}

void updateGame()
{
	VMContext* vm = pVMContext;
	Runner* runner = pRunner;
	AudioSystem* audioSystem = pAudioSystem;
	Gen8* gen8 = pGen8;
	Renderer* renderer = pRenderer;
	
	RunnerKeyboard_beginFrame(runner->keyboard);
	
	for (int i = 0; i < 256; i++)
	{
		if (keysPressedNextFrame[i])
			RunnerKeyboard_onKeyDown(runner->keyboard, i);
		if (keysReleasedNextFrame[i])
			RunnerKeyboard_onKeyUp(runner->keyboard, i);
	}
	
	memset(keysPressedNextFrame, 0, sizeof keysPressedNextFrame);
	memset(keysReleasedNextFrame, 0, sizeof keysReleasedNextFrame);

	// Debug key bindings
	if (runner->debugMode) {
		// Pause
		if (RunnerKeyboard_checkPressed(runner->keyboard, 'P')) {
			bDebugPaused = !bDebugPaused;
			fprintf(stderr, "Debug: %s\n", bDebugPaused ? "Paused" : "Resumed");
		}

		// Go to next room
		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_PAGEUP)) {
			DataWin* dw = runner->dataWin;
			if ((int32_t) dw->gen8.roomOrderCount > runner->currentRoomOrderPosition + 1) {
				int32_t nextIdx = dw->gen8.roomOrder[runner->currentRoomOrderPosition + 1];
				runner->pendingRoom = nextIdx;
				runner->audioSystem->vtable->stopAll(runner->audioSystem);
				fprintf(stderr, "Debug: Going to next room -> %s\n", dw->room.rooms[nextIdx].name);
			}
		}

		// Go to previous room
		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_PAGEDOWN)) {
			DataWin* dw = runner->dataWin;
			if (runner->currentRoomOrderPosition > 0) {
				int32_t prevIdx = dw->gen8.roomOrder[runner->currentRoomOrderPosition - 1];
				runner->pendingRoom = prevIdx;
				runner->audioSystem->vtable->stopAll(runner->audioSystem);
				fprintf(stderr, "Debug: Going to previous room -> %s\n", dw->room.rooms[prevIdx].name);
			}
		}

		// Dump runner state to console
		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F12)) {
			fprintf(stderr, "Debug: Dumping runner state at frame %d\n", runner->frameCount);
			Runner_dumpState(runner);
		}

		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F11)) {
			fprintf(stderr, "Debug: Dumping runner state at frame %d\n", runner->frameCount);
			char* json = Runner_dumpStateJson(runner);
			printf("%s\n", json);
			free(json);
		}

		// Toggle the collision mask debug overlay
		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F2)) {
			bDebugShowCollisionMasks = !bDebugShowCollisionMasks;
			fprintf(stderr, "Debug: Collision mask overlay %s!\n", bDebugShowCollisionMasks ? "enabled" : "disabled");
		}

		// Reset global interact state because I HATE when I get stuck while moving through rooms
		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F10)) {
			int32_t interactVarId = shget(runner->vmContext->globalVarNameMap, "interact");

			runner->vmContext->globalVars[interactVarId] = RValue_makeInt32(0);
			printf("Changed global.interact [%d] value!\n", interactVarId);
		}
	}

	// Run the game step if the game is paused
	bool shouldStep = true;
	if (runner->debugMode && bDebugPaused) {
		shouldStep = RunnerKeyboard_checkPressed(runner->keyboard, 'O');
		if (shouldStep) fprintf(stderr, "Debug: Frame advance (frame %d)\n", runner->frameCount);
	}

	double frameStartTime = 0;
	
	// PROFILING
	uint64_t start;
	uint64_t runner_step_ticks = 0, render_ticks = 0;

	if (shouldStep) {
		frameStartTime = swrGetTime();
		if (bTraceFrames) {
			fprintf(stderr, "Frame %d (Start)\n", runner->frameCount);
		}

		// Run one game step (Begin Step, Keyboard, Alarms, Step, End Step, room transitions)
		start = swrGetTimePrecise();
		Runner_step(runner);
		runner_step_ticks = swrGetTimePrecise() - start;

		if (nProfilerFramesBetween > 0 && runner->frameCount > 0 && runner->frameCount % nProfilerFramesBetween == 0) {
			char* profilerReport = Profiler_createReport(vm->profiler, 20, nProfilerFramesBetween);
			if (profilerReport != nullptr) {
				fprintf(stderr, "%s\n", profilerReport);
				free(profilerReport);
			}
			Profiler_reset(vm->profiler);
		}

		// Update audio system (gain fading, cleanup ended sounds)
		float dt = (float) (swrGetTime() - lastFrameTime);
		if (0.0f > dt) dt = 0.0f;
		if (dt > 0.1f) dt = 0.1f; // cap delta to avoid huge fades on lag spikes
		runner->audioSystem->vtable->update(runner->audioSystem, dt);
	}

	// Query actual framebuffer size (differs from window size on Wayland with fractional scaling)
	int fbWidth, fbHeight;
	fbWidth = WINDOW_WIDTH;
	fbHeight = WINDOW_HEIGHT;
	
	start = swrGetTimePrecise();
	SWRenderer_clearFrameBuffer(renderer, 0);
	
	int32_t gameW = (int32_t) gen8->defaultWindowWidth;
	int32_t gameH = (int32_t) gen8->defaultWindowHeight;

	// The application surface (FBO) is sized to defaultWindowWidth x defaultWindowHeight.
	// It is a bit hard to understand, but here's how it works:
	// The Port X/Port Y controls the position of the game viewport within the application surface.
	// The Port W/Port H controls the size of the game viewport within the application surface.
	// Think of it like if you had an image (or... well, a framebuffer) and you are "pasting" it over the application surface.
	// And the Port W/Port H are scaled by the window size too (set by the GEN8 chunk)
	float displayScaleX;
	float displayScaleY;

	Runner_computeViewDisplayScale(runner, gameW, gameH, &displayScaleX, &displayScaleY);

	renderer->vtable->beginFrame(renderer, gameW, gameH, fbWidth, fbHeight);

	// Clear FBO with room background color
	if (runner->drawBackgroundColor) {
		//int rInt = BGR_R(runner->backgroundColor);
		//int gInt = BGR_G(runner->backgroundColor);
		//int bInt = BGR_B(runner->backgroundColor);
		SWRenderer_clearFrameBuffer(renderer, runner->backgroundColor);
	}

	Runner_drawViews(runner, gameW, gameH, displayScaleX, displayScaleY, bDebugShowCollisionMasks);

	renderer->vtable->endFrame(renderer);

	render_ticks = swrGetTimePrecise() - start;

	if (nExitAtFrame >= 0 && runner->frameCount >= nExitAtFrame) {
		printf("Exiting at frame %d (--exit-at-frame)\n", runner->frameCount);
		DestroyWindow(hWnd);
	}

	if (shouldStep)
	{
		double frameElapsedMs = (swrGetTime() - frameStartTime) * 1000.0;
		if (bTraceFrames) {
			fprintf(stderr, "Frame %d (End, %.2f ms)\n", runner->frameCount, frameElapsedMs);
		}
		
		if (frameElapsedMs > 33) {
			fprintf(stderr, "Frame %d (overtime, %.2f ms)\n", runner->frameCount, frameElapsedMs);
		}
	}

	// Only swap when there isn't a room change to match the original runner.
	if (runner->pendingRoom == -1) {
		swrFlushFrameBuffer(runner_step_ticks, render_ticks);
	}
	
	//DEBUG: enter a different room from the intro
	if (runner->pendingRoom != -1 && nBeginningRoom != -1) {
		static bool a = false;
		if (!a) {
			a = true;
			runner->pendingRoom = nBeginningRoom;
		}
	}
	Runner_handlePendingRoomChange(runner);

	// Limit frame rate to room speed
	if (runner->currentRoom->speed > 0) {
		static bool fastForwardActive = false;
		static bool fastForwardTabPrev = false;
		double now = swrGetTime();
		
		bool fastForwardTabNow = swrIsKeyPressed(VK_TAB);
		if (fFastForwardSpeed > 0.0 && fastForwardTabNow && !fastForwardTabPrev) {
			fastForwardActive = !fastForwardActive;
			lastFrameTime = now;
		}
		
		fastForwardTabPrev = fastForwardTabNow;
		double effectiveSpeed = (fFastForwardSpeed > 0.0 && fastForwardActive) ? fFastForwardSpeed : fSpeedMultiplier;
		double targetFrameTime = 1.0 / (runner->currentRoom->speed * effectiveSpeed);
		
		double nextFrameTime = lastFrameTime + targetFrameTime;
		double lagFactor = now - nextFrameTime;
		
		if (lagFactor > 0.25f) {
			nextFrameTime = now + targetFrameTime;
		}
		
		// Sleep for most of the remaining time, then spin-wait for precision
		double remaining = nextFrameTime - swrGetTime();
		if (remaining > 0.002) {
		#ifdef _WIN32
			Sleep((DWORD) ((remaining - 0.001) * 1000));
		#else
			struct timespec ts = {
				.tv_sec = 0,
				.tv_nsec = (long) ((remaining - 0.001) * 1e9)
			};
			nanosleep(&ts, nullptr);
		#endif
		}
		while (swrGetTime() < nextFrameTime) {
			// Spin-wait for the remaining sub-millisecond
		}
		lastFrameTime = nextFrameTime;
	} else {
		lastFrameTime = swrGetTime();
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	static WNDCLASS windowClass;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = TEXT("Butterscotch");
	windowClass.hbrBackground = GetStockBrush(WHITE_BRUSH);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hIcon = NULL;
	
	if (!RegisterClass(&windowClass)) {
		fprintf(stderr, "Failed to register window class. Last error: %lu\n", GetLastError());
		return 1;
	}
	
	RECT rc;
	rc.left = rc.top = 0;
	rc.right = WINDOW_WIDTH;
	rc.bottom = WINDOW_HEIGHT;
	
	DWORD dwWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
	
	AdjustWindowRect(&rc, dwWindowStyle, FALSE);
	
	hWnd = CreateWindow(
		TEXT("Butterscotch"), // Class Name
		TEXT("Butterscotch"), // Caption
		dwWindowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		NULL, // Parent
		NULL, // Menu
		hInstance,
		NULL  // LPParam
	);
	
	if (!hWnd) {
		fprintf(stderr, "Failed to create window. Last error: %lu\n", GetLastError());
		return 1;
	}
	
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	
	initializeGame();
	
	MSG msg = {};
	while (!pRunner->shouldExit && !bWantToQuit)
	{
		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		updateGame();
	}
	
	return 0;
}

#endif
