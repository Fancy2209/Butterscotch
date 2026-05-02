#include "data_win.h"
#include "ps3gl.h"
#include "rsxutil.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#include "runner_keyboard.h"
#include "runner.h"
#include "input_recording.h"
#include "debug_overlay.h"
#include "gl_legacy_renderer.h"
#include "glfw_file_system.h"
#include "noop_audio_system.h"
#include "stb_ds.h"
#include "stb_image_write.h"

#include "utils.h"
#include "profiler.h"

#include <io/pad.h>
#include <sys/systime.h>
#include <sys/thread.h>

typedef struct {
    uint8_t digital;
    uint8_t mask;
    int32_t gmlKey;
} PadMapping;

const PadMapping PAD_MAPPINGS[] = {
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_UP,       VK_UP },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_DOWN,     VK_DOWN },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_LEFT,     VK_LEFT },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_RIGHT,    VK_RIGHT },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_START,    'C' },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_SELECT,   VK_ESCAPE },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_CROSS,    'Z' },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_SQUARE,   'X' },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_TRIANGLE, 'C' },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_L1,       VK_PAGEDOWN },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_R1,       VK_PAGEUP },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_L2,       VK_F10 },
};
static const int PAD_MAPPING_COUNT = sizeof(PAD_MAPPINGS) / sizeof(PAD_MAPPINGS[0]);
static bool prevState[sizeof(PAD_MAPPINGS) / sizeof(PAD_MAPPINGS[0])] = {0};

#define DATAWIN_PATH "/dev_hdd0/BUTTERSCOTCH/data.win"

// ===[ MAIN ]===
static double freq = 0; 
#define PS3_GET_TIME ((double)__builtin_ppc_get_timebase()/freq)
bool shouldExit = false;

// ===[ MAIN ]===
int main(int argc, char* argv[]) {
    freq = sysGetTimebaseFrequency();

    printf("Loading %s...\n", DATAWIN_PATH);

    DataWin* dataWin = DataWin_parse(
        DATAWIN_PATH,
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
            //.eagerlyLoadedRooms = args.eagerRooms
        }
    );

    Gen8* gen8 = &dataWin->gen8;
    printf("Loaded \"%s\" (%d) successfully! [Bytecode Version %u / GameMaker version %u.%u.%u.%u]\n", gen8->name, gen8->gameID, gen8->bytecodeVersion, dataWin->detectedFormat.major, dataWin->detectedFormat.minor, dataWin->detectedFormat.release, dataWin->detectedFormat.build);

    // Initialize VM
    VMContext* vm = VM_create(dataWin);

    Profiler_setEnabled(&vm->profiler, false);
#ifdef ENABLE_VM_OPCODE_PROFILER
    vm->opcodeProfilerEnabled = true;
    if (vm->opcodeProfilerEnabled) {
        vm->opcodeVariantCounts = safeCalloc(256 * 256, sizeof(uint64_t));
        vm->opcodeRValueTypeCounts = safeCalloc(256 * 256, sizeof(uint64_t));
    }
#endif

    // Initialize the file system
    GlfwFileSystem* glfwFileSystem = GlfwFileSystem_create(DATAWIN_PATH);

    // Init GLFW
    ps3glInit();
    ioPadInit(7);

    // Initialize the renderer
    Renderer* renderer = GLLegacyRenderer_create();


    // Initialize the audio system
    AudioSystem* audioSystem = (AudioSystem*) NoopAudioSystem_create();

    // Initialize the runner
    Runner* runner = Runner_create(dataWin, vm, renderer, (FileSystem*) glfwFileSystem, audioSystem);
    runner->debugMode = false;
    //runner->osType = OS_PS3;

    // Initialize the first room and fire Game Start / Room Start events
    Runner_initFirstRoom(runner);

    // Main loop
    bool debugPaused = false;
    bool debugShowCollisionMasks = false;
    double lastFrameTime = PS3_GET_TIME;
    while (!shouldExit && !runner->shouldExit) {
        // Clear last frame's pressed/released state, then poll new input events
        RunnerKeyboard_beginFrame(runner->keyboard);
        RunnerGamepad_beginFrame(runner->gamepads);


        // Run the game step if the game is paused
        bool shouldStep = true;
        if (runner->debugMode && debugPaused) {
            shouldStep = RunnerKeyboard_checkPressed(runner->keyboard, 'O');
            if (shouldStep) fprintf(stderr, "Debug: Frame advance (frame %d)\n", runner->frameCount);
        }

        double frameStartTime = 0;

        padInfo padinfo;
        ioPadGetInfo(&padinfo);

        if (padinfo.status[0])
        {
            padData paddata;
            ioPadGetData(0, &paddata);

            for (int i = 0; i < PAD_MAPPING_COUNT; i++)
            {
                uint8_t byte = (uint8_t)paddata.button[PAD_MAPPINGS[i].digital];
                uint8_t mask = PAD_MAPPINGS[i].mask;
                int32_t gmlKey = PAD_MAPPINGS[i].gmlKey;

                bool isPressed = (byte & mask) != 0;
                bool wasPressed = prevState[i];

                if (isPressed && !wasPressed)
                {
                    RunnerKeyboard_onKeyDown(runner->keyboard, gmlKey);
                }
                else if (!isPressed && wasPressed)
                {
                    RunnerKeyboard_onKeyUp(runner->keyboard, gmlKey);
                }

                prevState[i] = isPressed;
            }
        }

        if (shouldStep) {

            // Run one game step (Begin Step, Keyboard, Alarms, Step, End Step, room transitions)
            Runner_step(runner);

            // Update audio system (gain fading, cleanup ended sounds)
            float dt = (float) (PS3_GET_TIME - lastFrameTime);
            if (0.0f > dt) dt = 0.0f;
            if (dt > 0.1f) dt = 0.1f; // cap delta to avoid huge fades on lag spikes
            runner->audioSystem->vtable->update(runner->audioSystem, dt);
        }

        Room* activeRoom = runner->currentRoom;

        // Query actual framebuffer size (differs from window size on Wayland with fractional scaling)
        int fbWidth = display_width, fbHeight = display_height;

        // Clear the default framebuffer (window background) to black
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

        ps3glSwapBuffers();

        double now = PS3_GET_TIME;

        // Limit frame rate to room speed
        if (runner->currentRoom->speed > 0) {
            double targetFrameTime = 1.0 / runner->currentRoom->speed;
            double nextFrameTime = lastFrameTime + targetFrameTime;

            if (now < nextFrameTime) {
                while (PS3_GET_TIME < nextFrameTime) {}
                lastFrameTime = nextFrameTime;
            } else {
                // Frame took too long → resync
                lastFrameTime = now;
            }
        } else {
            lastFrameTime = now;
        }
    }


    // Cleanup
    runner->audioSystem->vtable->destroy(runner->audioSystem);
    runner->audioSystem = nullptr;
    renderer->vtable->destroy(renderer);

    Runner_free(runner);
    GlfwFileSystem_destroy(glfwFileSystem);
#ifdef ENABLE_VM_OPCODE_PROFILER
    VM_printOpcodeProfilerReport(vm);
#endif
    VM_free(vm);
    DataWin_free(dataWin);

    printf("Bye! :3\n");
    return 0;
}
