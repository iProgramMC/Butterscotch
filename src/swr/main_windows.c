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
#ifdef ENABLE_MINIAUDIO
#include "glfw/ma_audio_system.h"
#else
#include "noop_audio_system.h"
#endif

// ========== Configuration ==========
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

static const char* pDataWinPath = "../build-win/data/data.win";
static const char* pDataWinDir = "../build-win/data";
static const char* pSaveDataDir = "../build-win/data";

static bool bLazilyLoadRooms = false;
static bool bDebugMode = true;
static bool bTraceFrames = false;
static YoYoOperatingSystem nOsType = OS_WINDOWS;

static int nBeginningRoom = -1; // 287
static int nProfilerFramesBetween = 0;
static int nSetSeed = 0;
static int nExitAtFrame = -1;
static float fFastForwardSpeed = 0.0f;
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
static double lastFrameTime;

static bool keysPressed[256];

static double swrGetTime()
{
	return GetTickCount() / 1000.0;
}

static uint8_t winKeyToGMLKey(uint8_t wParam)
{
	return wParam;
}

static void keyPressed(uint8_t key)
{
	// GML uses key constants which match Win32's
	if (!keysPressed[key]) {
		fprintf(stderr,"Sending onKeyDown: %d\n", key);
		RunnerKeyboard_onKeyDown(pRunner->keyboard, key);
	}

	keysPressed[key] = true;
}

static void keyReleased(uint8_t key)
{
	// GML uses key constants which match Win32's
	if (keysPressed[key])
		RunnerKeyboard_onKeyUp(pRunner->keyboard, key);

	keysPressed[key] = false;
}

static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_DESTROY:
			bWantToQuit = true;
			PostQuitMessage(0);
			return 0;
		
		case WM_KEYDOWN:
			if (wParam >= 2 && wParam < 256)
				keyPressed(winKeyToGMLKey(wParam));
			break;
		
		case WM_KEYUP:
			if (wParam >= 2 && wParam < 256)
				keyReleased(winKeyToGMLKey(wParam));
			break;
		
		case WM_ACTIVATE:
			if (wParam == WA_INACTIVE && lParam == hWnd)
				memset(keysPressed, 0, sizeof keysPressed);
			break;
	}
	
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static uint32_t* nextFrameBuffer = NULL;
static int nextFrameBufferWidth = 0;
static int nextFrameBufferHeight = 0;

void Runner_setNextFrame(uint32_t* framebuffer, int width, int height)
{
	nextFrameBuffer = framebuffer;
	nextFrameBufferWidth = width;
	nextFrameBufferHeight = height;
}

static void swrSwapBuffers()
{
	int width = nextFrameBufferWidth;
	int height = nextFrameBufferHeight;
	uint32_t* pixels = nextFrameBuffer;
	
	HDC hdc = GetDC(hWnd);
	
	BITMAPINFO bmi = {0};
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = width;
	bmi.bmiHeader.biHeight      = -height;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	SetDIBitsToDevice(
		hdc,
		0, 0, // dest X/Y
		width, height,
		0, 0,
		0, height,
		nextFrameBuffer,
		&bmi,
		DIB_RGB_COLORS
	);

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
			.parseAudo = true,
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
	
#ifdef ENABLE_MINIAUDIO
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

	if (shouldStep) {
		if (bTraceFrames) {
			frameStartTime = swrGetTime();
			fprintf(stderr, "Frame %d (Start)\n", runner->frameCount);
		}

		// Run one game step (Begin Step, Keyboard, Alarms, Step, End Step, room transitions)
		Runner_step(runner);

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

	// Capture screenshot if this frame matches a requested frame
/*
	bool shouldScreenshot = hmget(args.screenshotFrames, runner->frameCount);

	if (shouldScreenshot) {
		GLuint readFbo = (strcmp(args.renderer, "legacy-gl") == 0) ? 0 : ((GLRenderer*) renderer)->fbo;
		captureScreenshot(readFbo, args.screenshotPattern, runner->frameCount, gameW, gameH);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	// Dump all surfaces if this frame matches a requested frame
	bool shouldDumpSurfaces = hmget(args.screenshotSurfacesFrames, runner->frameCount);

	if (shouldDumpSurfaces) {
		GLRenderer* gl = (GLRenderer*) renderer;
		dumpAllSurfaces(gl, args.screenshotSurfacesPattern, runner->frameCount);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}
*/
	if (nExitAtFrame >= 0 && runner->frameCount >= nExitAtFrame) {
		printf("Exiting at frame %d (--exit-at-frame)\n", runner->frameCount);
		DestroyWindow(hWnd);
	}

	if (shouldStep && bTraceFrames) {
		double frameElapsedMs = (swrGetTime() - frameStartTime) * 1000.0;
		fprintf(stderr, "Frame %d (End, %.2f ms)\n", runner->frameCount, frameElapsedMs);
	}

	// Only swap when there isn't a room change to match the original runner.
	if (runner->pendingRoom == -1) {
		swrSwapBuffers();
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
		bool fastForwardTabNow = swrIsKeyPressed(VK_TAB);
		if (fFastForwardSpeed > 0.0 && fastForwardTabNow && !fastForwardTabPrev) {
			fastForwardActive = !fastForwardActive;
			lastFrameTime = swrGetTime();
		}
		fastForwardTabPrev = fastForwardTabNow;
		double effectiveSpeed = (fFastForwardSpeed > 0.0 && fastForwardActive) ? fFastForwardSpeed : fSpeedMultiplier;
		double targetFrameTime = 1.0 / (runner->currentRoom->speed * effectiveSpeed);
		double nextFrameTime = lastFrameTime + targetFrameTime;
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

int main(int argc, char* argv[])
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
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
	
	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);
	
	initializeGame();
	
	MSG msg = {};
	while (!pRunner->shouldExit && !bWantToQuit)
	{
		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE) > 0)
		{
			fprintf(stderr, "Handling message: %d\n", msg.message);
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		updateGame();
	}
	
	return 0;
}

#endif
