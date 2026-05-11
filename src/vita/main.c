#include "data_win.h"
#include "vm.h"

#include <vitasdk.h>
#include <vitaGL.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "runner_keyboard.h"
#include "runner.h"
#include "input_recording.h"
#include "debug_overlay.h"
#include "gl_renderer.h"
#include "overlay_file_system.h"
#include "ma_audio_system.h"
#include "stb_ds.h"
#include "stb_image_write.h"

#include "utils.h"
#include "profiler.h"
__attribute__((used)) int _newlib_heap_size_user = 246 * 1024 * 1024;

typedef struct {
    uint32_t mask;
    int32_t gmlKey;
} PadMapping;

const PadMapping PAD_MAPPINGS[] = {
    { SCE_CTRL_UP,       VK_UP },
    { SCE_CTRL_DOWN,     VK_DOWN },
    { SCE_CTRL_LEFT,     VK_LEFT },
    { SCE_CTRL_RIGHT,    VK_RIGHT },
    { SCE_CTRL_START,    'C' },
    { SCE_CTRL_SELECT,   VK_ESCAPE },
    { SCE_CTRL_CROSS,    'Z' },
    { SCE_CTRL_SQUARE,   'X' },
    { SCE_CTRL_CIRCLE,   'X' },
    { SCE_CTRL_TRIANGLE, 'C' },
    { SCE_CTRL_L1,       VK_PAGEDOWN },
    { SCE_CTRL_R1,       VK_PAGEUP },
};
static const int PAD_MAPPING_COUNT = sizeof(PAD_MAPPINGS) / sizeof(PAD_MAPPINGS[0]);
static bool prevState[sizeof(PAD_MAPPINGS) / sizeof(PAD_MAPPINGS[0])] = {0};

#define STICK_CENTER 0x80 // The center of the stick (range 0x00-0xFF)
#define STICK_THRESHOLD 0x40 // The threshold for treating stick movement as a d-pad press

typedef struct {
    bool isX;
    int8_t  sign;
    int32_t gmlKey;
} StickMapping;

const StickMapping STICK_MAPPINGS[] = {
    { true, -1,  VK_LEFT  },
    { true, +1,  VK_RIGHT },
    { false, -1, VK_UP    },
    { false, +1, VK_DOWN  },
};
static const int STICK_MAPPING_COUNT = sizeof(STICK_MAPPINGS) / sizeof(STICK_MAPPINGS[0]);
static bool prevStickState[sizeof(STICK_MAPPINGS) / sizeof(STICK_MAPPINGS[0])] = {0};

#define DATA_WIN "ux0:data/butterscotch/data.win"

double vitaGetTime(void) {
    //SceRtcTick tick;
    //sceRtcGetCurrentTick(&tick);
    //return tick.tick / 1000000.0f;
    return sceKernelGetProcessTimeLow() / 1000000.0f;
}

// ===[ MAIN ]===
int main(int argc, char* argv[]) {
    printf("Loading %s...\n", DATA_WIN);

    DataWin* dataWin = DataWin_parse(
        DATA_WIN,
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
            .lazyLoadRooms = true,
        }
    );

    Gen8* gen8 = &dataWin->gen8;
    printf("Loaded \"%s\" (%d) successfully! [Bytecode Version %u / GameMaker version %u.%u.%u.%u]\n", gen8->name, gen8->gameID, gen8->bytecodeVersion, dataWin->detectedFormat.major, dataWin->detectedFormat.minor, dataWin->detectedFormat.release, dataWin->detectedFormat.build);

    // Build window title
    char windowTitle[256];
    snprintf(windowTitle, sizeof(windowTitle), "Butterscotch - %s", gen8->displayName);

    // Initialize VM
    VMContext* vm = VM_create(dataWin);

    // Initialize the file system
    char* dataWinDir = nullptr;
    {
        const char* lastSlash = strrchr(DATA_WIN, '/');
        const char* lastBackslash = strrchr(DATA_WIN, '\\');
        if (lastBackslash != nullptr && (lastSlash == nullptr || lastBackslash > lastSlash))
            lastSlash = lastBackslash;
        if (lastSlash != nullptr) {
            size_t len = (size_t) (lastSlash - DATA_WIN + 1);
            dataWinDir = safeMalloc(len + 1);
            memcpy(dataWinDir, DATA_WIN, len);
            dataWinDir[len] = '\0';
        } else {
            dataWinDir = safeStrdup("./");
        }
    }
    const char* savePath = dataWinDir;
    OverlayFileSystem* overlayFs = OverlayFileSystem_create(dataWinDir, savePath);
    free(dataWinDir);

    // Init GLFW
    scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	vglUseTripleBuffering(GL_FALSE);
    vglSetParamBufferSize(4 * 1024 * 1024);
    vglSetCircularPoolSize(1024 * 1024);
    vglInitWithCustomThreshold(0, 960, 544, 4 * 1024 * 1024, 0, 0, 0, SCE_GXM_MULTISAMPLE_NONE);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

    // Initialize the renderer
    Renderer* renderer = GLRenderer_create();

    // Initialize the audio system
    AudioSystem* audioSystem = (AudioSystem*) MaAudioSystem_create();

    // Initialize the runner
    Runner* runner = Runner_create(dataWin, vm, renderer, (FileSystem*) overlayFs, audioSystem);
    
    // Initialize the first room and fire Game Start / Room Start events
    Runner_initFirstRoom(runner);

    // Main loop
    bool debugPaused = false;
    bool debugShowCollisionMasks = false;
    double lastFrameTime = vitaGetTime();
    while (!runner->shouldExit) {
        // Clear last frame's pressed/released state, then poll new input events
        RunnerKeyboard_beginFrame(runner->keyboard);
        RunnerGamepad_beginFrame(runner->gamepads);

        SceCtrlData ctrl;
        sceCtrlPeekBufferPositive(0, &ctrl, 1);
        repeat(PAD_MAPPING_COUNT, i) {
            uint32_t mask = PAD_MAPPINGS[i].mask;
            int32_t gmlKey = PAD_MAPPINGS[i].gmlKey;

            bool isPressed = (ctrl.buttons & mask) != 0;
            bool wasPressed = prevState[i];

            if (isPressed && !wasPressed) {
                RunnerKeyboard_onKeyDown(runner->keyboard, gmlKey);
            } else if (!isPressed && wasPressed) {
                RunnerKeyboard_onKeyUp(runner->keyboard, gmlKey);
            }

            prevState[i] = isPressed;
        }

        repeat(STICK_MAPPING_COUNT, i) {
            unsigned int axisValue = STICK_MAPPINGS[i].isX ? ctrl.lx : ctrl.ly;
        
            int signedDelta = STICK_MAPPINGS[i].sign * (axisValue - STICK_CENTER);

            bool isPressed = signedDelta > STICK_THRESHOLD;
            bool wasPressed = prevStickState[i];        
            int32_t gmlKey = STICK_MAPPINGS[i].gmlKey;

            if (isPressed && !wasPressed) {
                RunnerKeyboard_onKeyDown(runner->keyboard, gmlKey);
            } else if (!isPressed && wasPressed) {
                RunnerKeyboard_onKeyUp(runner->keyboard, gmlKey);        
            }

            prevStickState[i] = isPressed;
        }

        // Run the game step if the game is paused
        bool shouldStep = true;
        if (runner->debugMode && debugPaused) {
            shouldStep = RunnerKeyboard_checkPressed(runner->keyboard, 'O');
            if (shouldStep) fprintf(stderr, "Debug: Frame advance (frame %d)\n", runner->frameCount);
        }

        double frameStartTime = 0;

        if (shouldStep) {
            // Run one game step (Begin Step, Keyboard, Alarms, Step, End Step, room transitions)
            Runner_step(runner);

            // Update audio system (gain fading, cleanup ended sounds)
            float dt = (float) (vitaGetTime() - lastFrameTime);
            if (0.0f > dt) dt = 0.0f;
            if (dt > 0.1f) dt = 0.1f; // cap delta to avoid huge fades on lag spikes
            runner->audioSystem->vtable->update(runner->audioSystem, dt);
        }

        // Query actual framebuffer size (differs from window size on Wayland with fractional scaling)
        int fbWidth = 960, fbHeight = 544;

        // Clear the default framebuffer (window background) to black
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

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
            int rInt = BGR_R(runner->backgroundColor);
            int gInt = BGR_G(runner->backgroundColor);
            int bInt = BGR_B(runner->backgroundColor);
            glClearColor(rInt / 255.0f, gInt / 255.0f, bInt / 255.0f, 1.0f);
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT);

        Runner_drawViews(runner, gameW, gameH, displayScaleX, displayScaleY, debugShowCollisionMasks);

        renderer->vtable->endFrame(renderer);

        if (runner->pendingRoom == -1) {
            vglSwapBuffers(GL_FALSE);
        }
        Runner_handlePendingRoomChange(runner);

        // Limit frame rate to room speed (skip in headless mode for max speed!!)
        if (runner->currentRoom->speed > 0) {
            double effectiveSpeed = 1.0f;
            double targetFrameTime = 1.0 / (runner->currentRoom->speed * effectiveSpeed);
            double nextFrameTime = lastFrameTime + targetFrameTime;
            // Sleep for most of the remaining time, then spin-wait for precision
            double remaining = nextFrameTime -  vitaGetTime();
            if (remaining > 0.002) {
                struct timespec ts = {
                    .tv_sec = 0,
                    .tv_nsec = (long) ((remaining - 0.001) * 1e9)
                };
                nanosleep(&ts, nullptr);
            }
            while (vitaGetTime() < nextFrameTime) {
                // Spin-wait for the remaining sub-millisecond
            }
            lastFrameTime = nextFrameTime;
        } else {
            lastFrameTime = vitaGetTime();
        }
    }


    // Cleanup
    runner->audioSystem->vtable->destroy(runner->audioSystem);
    runner->audioSystem = nullptr;
    renderer->vtable->destroy(renderer);

    Runner_free(runner);
    OverlayFileSystem_destroy(overlayFs);
    VM_free(vm);
    DataWin_free(dataWin);

    printf("Bye! :3\n");
    return 0;
}
