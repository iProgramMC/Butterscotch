#include "data_win.h"
#include "vm.h"
#include "platformdefs.h"

#include "runner_keyboard.h"
#include "runner.h"
#include "input_recording.h"
#include "debug_overlay.h"
#if defined(ENABLE_LEGACY_GL) || defined(ENABLE_MODERN_GL) || ((defined(USE_GLFW3) || defined(USE_GLFW2)) && defined(ENABLE_SW_RENDERER))
#include <glad/glad.h>
#endif
#if defined(ENABLE_LEGACY_GL) || defined(ENABLE_MODERN_GL)
#include "gl_renderer.h"
#ifdef ENABLE_LEGACY_GL
#include "gl_legacy_renderer.h"
#endif
#endif
#ifdef ENABLE_SW_RENDERER
#include "sw_renderer.h"
#endif
#include "overlay_file_system.h"
#if defined(USE_OPENAL)
#include "al_audio_system.h"
#elif defined(USE_MINIAUDIO)
#include "ma_audio_system.h"
#endif
#include "noop_audio_system.h"
#include "stb_ds.h"
#include "stb_image_write.h"

#include "utils.h"
#include "profiler.h"
#include "gettime.h"

// Config
static const char* pRendererStr = "software"; // or "modern-gl"
static const char* pDataWinPath = "/var/mobile/Documents/Butterscotch/undertale/data.win";
static const char* pSaveFolder = "/var/mobile/Documents/Butterscotch/undertale-save";

static YoYoOperatingSystem nOsType = OS_WINDOWS;
static bool bDebug = false;
static bool bHeadless = false;
static bool bTraceFrames = false;

static float fSpeedMultiplier = 1.0f;
static float fFastForwardSpeed = 1000.0f;

float fGameScale = 0.5f;

// Globals
static DataWin* pDataWin;
static Gen8* pGen8;
static VMContext* pVM;
static Renderer* pRenderer;
static AudioSystem* pAudioSystem;
static Runner* pRunner;
static char windowTitle[256];
static int32_t inputFrameCount = 0;

// More globals
InputRecording* globalInputRecording = nullptr;
enum GraphicsAPI gfx;

void resetMainLoopLocals();

// Resolves the window size for the specified operating system.
// The "--window-size" argument takes precedence over the default resolution for each platform.
static void resolveWindowSize(uint32_t gen8Width, uint32_t gen8Height, int32_t* outW, int32_t* outH) {
    switch (nOsType) {
        case OS_PS4:
        case OS_XBOXONE:
        case OS_PS3:
        case OS_XBOX360:
            *outW = 1920;
            *outH = 1080;
            break;
        case OS_SWITCH:
            *outW = 1280;
            *outH = 720;
            break;
        case OS_PSVITA:
            *outW = 960;
            *outH = 544;
            break;
        default:
            *outW = (int32_t) gen8Width;
            *outH = (int32_t) gen8Height;
            break;
    }
}

static bool platformInitialized = false;

// 1 = exit, 0 = continue
int initializeGame()
{
    char* currentDataWinPath = safeStrdup(pDataWinPath);
	
	DataWinParserOptions options = {0};
	options.parseGen8 = true;
	options.parseOptn = true;
	options.parseLang = true;
	options.parseExtn = true;
	options.parseSond = true;
	options.parseAgrp = true;
	options.parseSprt = true;
	options.parseBgnd = true;
	options.parsePath = true;
	options.parseScpt = true;
	options.parseGlob = true;
	options.parseShdr = true;
	options.parseFont = true;
	options.parseTmln = true;
	options.parseObjt = true;
	options.parseRoom = true;
	options.parseTpag = true;
	options.parseCode = true;
	options.parseVari = true;
	options.parseFunc = true;
	options.parseStrg = true;
	options.parseTxtr = true;
	options.parseAudo = true;
	options.skipLoadingPreciseMasksForNonPreciseSprites = true;
	DataWin* dataWin = DataWin_parse(currentDataWinPath, options);

	Gen8* gen8 = &dataWin->gen8;
	printf("Loaded \"%s\" (%d) successfully! [WAD Version %u / GameMaker version %u.%u.%u.%u]\n", gen8->name, gen8->gameID, gen8->wadVersion, dataWin->detectedFormat.major, dataWin->detectedFormat.minor, dataWin->detectedFormat.release, dataWin->detectedFormat.build);

	// Build window title
	snprintf(windowTitle, sizeof(windowTitle), "Butterscotch - %s", gen8->displayName);

	// Initialize VM
	VMContext* vm = VM_create(dataWin);

	// Initialize the file system
	char* dataWinDir = nullptr;
	{
		const char* lastSlash = strrchr(pDataWinPath, '/');
		const char* lastBackslash = strrchr(pDataWinPath, '\\');
		if (lastBackslash != nullptr && (lastSlash == nullptr || lastBackslash > lastSlash))
			lastSlash = lastBackslash;
		if (lastSlash != nullptr) {
			size_t len = (size_t) (lastSlash - pDataWinPath + 1);
			dataWinDir = safeMalloc(len + 1);
			memcpy(dataWinDir, pDataWinPath, len);
			dataWinDir[len] = '\0';
		} else {
			dataWinDir = safeStrdup("./");
		}
	}
	const char* savePath = pSaveFolder != nullptr ? pSaveFolder : dataWinDir;
	OverlayFileSystem* overlayFs = OverlayFileSystem_create(dataWinDir, savePath);
	free(dataWinDir);

	if (strcmp(pRendererStr, "modern-gl") == 0)
		gfx = MODERN_GL;
	else if (strcmp(pRendererStr, "legacy-gl") == 0)
		gfx = LEGACY_GL;
	else if (strcmp(pRendererStr, "software") == 0)
		gfx = SOFTWARE;
	else {
		fprintf(stderr, "Unknown renderer: %s!\n", pRendererStr);
		return 1;
	}

#ifndef ENABLE_LEGACY_GL
	if (gfx == LEGACY_GL) {
		fprintf(stderr, "The legacy gl renderer is not available in this build!\n");
		return 0;
	}
#endif
#ifndef ENABLE_MODERN_GL
	if (gfx == MODERN_GL) {
		fprintf(stderr, "The modern gl renderer is not available in this build!\n");
		return 0;
	}
#endif
#ifndef ENABLE_SW_RENDERER
	if (gfx == SOFTWARE) {
		fprintf(stderr, "The software renderer is not available in this build!\n");
		return 0;
	}
#endif

	int32_t windowW, windowH;
	resolveWindowSize(gen8->defaultWindowWidth, gen8->defaultWindowHeight, &windowW, &windowH);

	if (!platformInitialized) {
		if (!platformInit(windowW, windowH, windowTitle, bHeadless)) {
			DataWin_free(dataWin);
			//freeCommandLineArgs(&args);
			return 1;
		}

/*
#if defined(ENABLE_LEGACY_GL) || defined(ENABLE_MODERN_GL) || ((defined(USE_GLFW3) || defined(USE_GLFW2) || defined(USE_IOS)) && defined(ENABLE_SW_RENDERER) )
#if defined(USE_GLFW3) || defined(USE_GLFW2) || defined(USE_IOS)
		if (gfx == LEGACY_GL || gfx == MODERN_GL || gfx == SOFTWARE)
#else
		if (gfx == LEGACY_GL || gfx == MODERN_GL)
#endif
		{
			// Load OpenGL function pointers via GLAD
#ifdef ENABLE_GLES
			if (!(gfx == SOFTWARE ? gladLoadGLES1Loader((GLADloadproc)platformGetProcAddress) : gladLoadGLES2Loader((GLADloadproc)platformGetProcAddress))) {
#else
			if (!gladLoadGLLoader((GLADloadproc)platformGetProcAddress)) {
#endif
				fprintf(stderr, "Failed to initialize GLAD\n");
				platformExit();
				DataWin_free(dataWin);
				//freeCommandLineArgs(&args);
				return 1;
			}
		}
#endif
*/

		// Install the OpenGL debug message callback
#if !defined(ENABLE_GLES) && (defined(ENABLE_MODERN_GL) || defined(ENABLE_LEGACY_GL))
		if (gfx == MODERN_GL)
			installGLDebugCallback();
#endif

		platformInitialized = true;
	} else {
		// game_change path: reuse the existing window/GL context, just retitle and resize for the new game.
		platformSetWindowTitle(gen8->displayName);
		platformSetWindowSize(windowW, windowH);
	}

	// Initialize the renderer
	Renderer* renderer = nullptr;
#ifdef ENABLE_SW_RENDERER
	if (gfx == SOFTWARE)
		renderer = SWRenderer_create();
#endif
#ifdef ENABLE_LEGACY_GL
	if (gfx == LEGACY_GL)
		renderer = GLLegacyRenderer_create();
#endif
#ifdef ENABLE_MODERN_GL
	if (gfx == MODERN_GL)
		renderer = GLRenderer_create();
#endif
	if (!renderer) {
		fprintf(stderr, "Failed to initialize a renderer\n");
		platformExit();
		DataWin_free(dataWin);
		//freeCommandLineArgs(&args);
		return 1;
	}

	// Initialize the audio system
	AudioSystem* audioSystem = nullptr;
#if defined(USE_OPENAL)
	audioSystem = (AudioSystem*) AlAudioSystem_create();
#elif defined(USE_MINIAUDIO)
	audioSystem = (AudioSystem*) MaAudioSystem_create(dataWin);
#else
	audioSystem = (AudioSystem*) NoopAudioSystem_create();
#endif

	// Initialize the runner
	Runner* runner = Runner_create(dataWin, vm, renderer, (FileSystem*) overlayFs, audioSystem);

	runner->debugMode = bDebug;
	runner->osType = nOsType;
	runner->setWindowSize = platformSetWindowSize;
	runner->getWindowSize = platformGetWindowSize;
	runner->setWindowTitle = platformSetWindowTitle;
	//Runner_setGameArgs(runner, currentGameArgs, (int32_t) arrlen(currentGameArgs));
	platformInitFunctions(runner);

	// Initialize the first room and fire Game Start / Room Start events
	Runner_initFirstRoom(runner);

	pDataWin = dataWin;
	pGen8 = gen8;
	pVM = vm;
	pRenderer = renderer;
	pAudioSystem = audioSystem;
	pRunner = runner;
	
	resetMainLoopLocals();
	return 0;
}

static bool debugPaused = false;
static bool debugShowCollisionMasks = false;
static bool freeCamActive = false;
static bool actuallyShuttingDown = false;
static uint64_t lastFrameTime = 0;
static uint64_t lastFrameStartTime = 0; // for delta_time
static bool shouldWindowClose = false;

void resetMainLoopLocals()
{
	debugPaused = false;
	debugShowCollisionMasks = false;
	freeCamActive = false;
	actuallyShuttingDown = false;
	lastFrameTime = nowNanos();
	lastFrameStartTime = lastFrameTime; // for delta_time
	pRunner->gameStartTime = lastFrameTime;
	shouldWindowClose = false;
}

// 1 = exit, 0 = continue
int updateGame()
{
	Runner* runner = pRunner;
	VMContext* vm = pVM;
	Renderer* renderer = pRenderer;
	AudioSystem* audioSystem = pAudioSystem;
	Gen8* gen8 = pGen8;
	
	if (runner->shouldExit || shouldWindowClose) {
		actuallyShuttingDown = true;
		return 1;
	}

	if (runner->pendingWorkingDirectory != nullptr && runner->pendingLaunchParameters != nullptr) {
		// Break from the game loop, we'll handle this later
		return 1;
	}

	uint64_t frameStartNow = nowNanos();
	runner->deltaTime = (frameStartNow - lastFrameStartTime) / 1000;
	lastFrameStartTime = frameStartNow;

	// Clear last frame's pressed/released state, then poll new input events
	RunnerKeyboard_beginFrame(runner->keyboard);
	RunnerGamepad_beginFrame(runner->gamepads);
	RunnerMouse_beginFrame(runner->mouse);
	if (platformHandleEvents()) {
		shouldWindowClose = true;
		return 0;
	}

	// Debug key bindings
	if (runner->debugMode) {
		// Pause
		if (RunnerKeyboard_checkPressed(runner->keyboard, 'P')) {
			debugPaused = !debugPaused;
			fprintf(stderr, "Debug: %s\n", debugPaused ? "Paused" : "Resumed");
		}
	}

	// Run the game step if the game is paused
	bool shouldStep = true;
	if (runner->debugMode && debugPaused) {
		shouldStep = RunnerKeyboard_checkPressed(runner->keyboard, 'O');
		if (shouldStep) fprintf(stderr, "Debug: Frame advance (frame %d)\n", runner->frameCount);
	}

	uint64_t frameStartTime = 0;

	if (shouldStep) {
		if (bTraceFrames) {
			frameStartTime = nowNanos();
			fprintf(stderr, "Frame %d (Start)\n", runner->frameCount);
		}

		// Process input recording/playback (must happen after platformHandleEvents, before Runner_step)
		InputRecording_processFrame(globalInputRecording, runner->keyboard, inputFrameCount++);

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
			debugShowCollisionMasks = !debugShowCollisionMasks;
			fprintf(stderr, "Debug: Collision mask overlay %s!\n", debugShowCollisionMasks ? "enabled" : "disabled");
		}

		// Enable free cam
		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F3)) {
			runner->freeCamPanX = 0.0f;
			runner->freeCamPanY = 0.0f;
			runner->freeCamZoom = 1.0f;

			freeCamActive = !freeCamActive;
			fprintf(stderr, "Debug: Free cam %s!\n", freeCamActive ? "enabled" : "disabled");
		}

		if (freeCamActive) {
			if (RunnerKeyboard_check(runner->keyboard, VK_UP)) {
				runner->freeCamPanY -= (float) (0.000005f * runner->deltaTime);
			}

			if (RunnerKeyboard_check(runner->keyboard, VK_DOWN)) {
				runner->freeCamPanY += (float) (0.000005f * runner->deltaTime);
			}

			if (RunnerKeyboard_check(runner->keyboard, VK_LEFT)) {
				runner->freeCamPanX -= (float) (0.000005f * runner->deltaTime);
			}

			if (RunnerKeyboard_check(runner->keyboard, VK_RIGHT)) {
				runner->freeCamPanX += (float) (0.000005f * runner->deltaTime);
			}
		}

		// Reset global interact state because I HATE when I get stuck while moving through rooms
		if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F10)) {
			int32_t interactVarId = shget(runner->vmContext->globalVarNameMap, "interact");

			runner->vmContext->globalVars[interactVarId] = RValue_makeInt32(0);
			printf("Changed global.interact [%d] value!\n", interactVarId);
		}

		bool* currentKeyDown = safeCalloc(GML_KEY_COUNT, sizeof(bool));
		bool* currentKeyPressed = safeCalloc(GML_KEY_COUNT, sizeof(bool));
		bool* currentKeyReleased = safeCalloc(GML_KEY_COUNT, sizeof(bool));

		if (freeCamActive) {
			// THIS IS A HACK!! We don't want to pass keys to the runner, but we DO want to keep it so we can hold the arrow keys to move the camera
			memcpy(currentKeyDown, runner->keyboard->keyDown, sizeof(runner->keyboard->keyDown));
			memcpy(currentKeyPressed, runner->keyboard->keyPressed, sizeof(runner->keyboard->keyPressed));
			memcpy(currentKeyReleased, runner->keyboard->keyReleased, sizeof(runner->keyboard->keyReleased));

			memset(runner->keyboard->keyDown, 0, sizeof(runner->keyboard->keyDown));
			memset(runner->keyboard->keyPressed, 0, sizeof(runner->keyboard->keyPressed));
			memset(runner->keyboard->keyReleased, 0, sizeof(runner->keyboard->keyReleased));
		}

		// Run one game step (Begin Step, Keyboard, Alarms, Step, End Step, room transitions)
		Runner_step(runner);

		if (freeCamActive) {
			memcpy(runner->keyboard->keyDown, currentKeyDown, sizeof(runner->keyboard->keyDown));
			memcpy(runner->keyboard->keyPressed, currentKeyPressed, sizeof(runner->keyboard->keyPressed));
			memcpy(runner->keyboard->keyReleased, currentKeyReleased, sizeof(runner->keyboard->keyReleased));
		}

		free(currentKeyDown);
		free(currentKeyPressed);
		free(currentKeyReleased);

		// Update audio system (gain fading, cleanup ended sounds)
		float dt = (float) (runner->deltaTime / 1000000.0);
		if (0.0f > dt) dt = 0.0f;
		if (dt > 0.1f) dt = 0.1f; // cap delta to avoid huge fades on lag spikes
		runner->audioSystem->vtable->update(runner->audioSystem, dt);

		// Dump full runner state if this frame was requested


		// Dump runner state as JSON if this frame was requested


		// Clear the default framebuffer (window background) to black
#ifdef ENABLE_SW_RENDERER
		if (gfx == SOFTWARE)
			SWRenderer_clearFrameBuffer(renderer, 0);
#endif
#if defined(ENABLE_LEGACY_GL) || defined(ENABLE_MODERN_GL)
		if (gfx == LEGACY_GL || gfx == MODERN_GL) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClear(GL_COLOR_BUFFER_BIT);
		}
#endif

		// Query actual framebuffer size
		int32_t fbWidth, fbHeight;
		platformGetWindowSize(&fbWidth, &fbHeight);

		if (!runner->appSurfaceEnabled) {
			runner->applicationWidth = fbWidth;
			runner->applicationHeight = fbHeight;
			runner->usingAppSurface = false;
		} else {
			if (runner->applicationWidth <= 0 || runner->applicationHeight <= 0) {
				runner->applicationWidth = (int32_t) gen8->defaultWindowWidth;
				runner->applicationHeight = (int32_t) gen8->defaultWindowHeight;
			}
			runner->usingAppSurface = true;
		}

		int32_t gameW = runner->applicationWidth;
		int32_t gameH = runner->applicationHeight;

		// Widescreen hack: render into a surface grown toward the requested aspect to fake a different aspect
		// ratio. The game's logical applicationWidth/Height is left untouched (so the reads above stay the real
		// size and this never compounds frame-to-frame); only the local gameW/gameH used for the projection/FBO
		// grow. A wider-than-native target grows width (reveal left/right); a taller one grows height (reveal
		// top/bottom). Runner_drawViews reads widescreenExtraWidth/Height to expand each view to match.
		runner->widescreenExtraWidth = 0;
		runner->widescreenExtraHeight = 0;
		/*if (args.widescreenAspect > 0.0f && runner->usingAppSurface && gameW > 0 && gameH > 0) {
			float nativeAspect = (float) gameW / (float) gameH;
			if (args.widescreenAspect > nativeAspect) {
				int32_t targetW = (int32_t) ((float) gameH * args.widescreenAspect + 0.5f);
				if (targetW > gameW) {
					runner->widescreenExtraWidth = targetW - gameW;
					gameW = targetW;
				}
			} else if (args.widescreenAspect < nativeAspect) {
				int32_t targetH = (int32_t) ((float) gameW / args.widescreenAspect + 0.5f);
				if (targetH > gameH) {
					runner->widescreenExtraHeight = targetH - gameH;
					gameH = targetH;
				}
			}
		}*/

		Runner_drawPre(runner, fbWidth, fbHeight);

		// Calculate viewport (letterboxing) in screen coordinates for mouse mapping
		int32_t winW, winH;
		platformGetScaledWindowSize(&winW, &winH);

		Runner_beginFrame(runner, gameW, gameH, winW, winH, fbWidth, fbHeight);

		double mx, my;
		platformGetMousePos(&mx, &my);
		Runner_updateMousePosition(runner, winW, winH, mx, my);

		// Clear FBO with room background color
#ifdef ENABLE_SW_RENDERER
		if (gfx == SOFTWARE) {
			if (runner->drawBackgroundColor)
				SWRenderer_clearFrameBuffer(renderer, runner->backgroundColor);
			else
				SWRenderer_clearFrameBuffer(renderer, 0);
		}
#endif
#if defined(ENABLE_LEGACY_GL) || defined(ENABLE_MODERN_GL)
		if (gfx == MODERN_GL || gfx == LEGACY_GL) {
			if (runner->drawBackgroundColor) {
				int rInt = BGR_R(runner->backgroundColor);
				int gInt = BGR_G(runner->backgroundColor);
				int bInt = BGR_B(runner->backgroundColor);
				glClearColor(rInt / 255.0f, gInt / 255.0f, bInt / 255.0f, 1.0f);
			} else
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

			glClear(GL_COLOR_BUFFER_BIT);
		}
#endif

		Runner_drawViews(runner, gameW, gameH, debugShowCollisionMasks);
		renderer->vtable->endFrameInit(renderer);
		Runner_drawPost(runner, fbWidth, fbHeight);
		renderer->vtable->endFrameEnd(renderer);
		Runner_drawGUI(runner, fbWidth, fbHeight, gameW, gameH);

		if (shouldStep && bTraceFrames) {
			double frameElapsedMs = (nowNanos() - frameStartTime) / 1000000.0;
			fprintf(stderr, "Frame %d (End, %.2f ms)\n", runner->frameCount, frameElapsedMs);
		}

		// Only swap when there isn't a room change to match the original runner.
		if (runner->pendingRoom == -1)
			platformSwapBuffers();
		Runner_handlePendingRoomChange(runner);
	}

	// Limit frame rate to room speed (skip in headless mode for max speed!!)
	if (!bHeadless && runner->currentRoom->speed > 0) {
		static bool fastForwardActive = false;
		static bool fastForwardTabPrev = false;
		bool fastForwardTabNow = RunnerKeyboard_checkPressed(runner->keyboard, '\t');
		if (fFastForwardSpeed > 0.0 && fastForwardTabNow && !fastForwardTabPrev) {
			fastForwardActive = !fastForwardActive;
			lastFrameTime = nowNanos();
		}
		fastForwardTabPrev = fastForwardTabNow;
		double effectiveSpeed = (fFastForwardSpeed > 0.0 && fastForwardActive) ? fFastForwardSpeed : fSpeedMultiplier;
		uint64_t targetFrameTime = 1000000000 / (runner->currentRoom->speed * effectiveSpeed);
		uint64_t nextFrameTime = lastFrameTime + targetFrameTime;
		platformSleepUntil(nextFrameTime);
	}
	lastFrameTime = nowNanos();
	
	return 0;
}
