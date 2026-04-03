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
#include "gl_legacy_renderer.h"
#include "glfw_file_system.h"
#include "noop_audio_system.h"
#include "stb_ds.h"
#include "stb_image_write.h"

#include "utils.h"

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
int main(int argc, char* argv[]) {
    printf("Loading %s...\n", DATAWIN_PATH);

    DataWin* dataWin = DataWin_parse(
        DATAWIN_PATH,
        (DataWinParserOptions) {
            .parseGen8 = true,
            .parseOptn = true,
            .parseLang = true,
            .parseExtn = true,
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
            .skipLoadingPreciseMasksForNonPreciseSprites = true
        }
    );

    Gen8* gen8 = &dataWin->gen8;
    printf("Loaded \"%s\" (%d) successfully! [Bytecode Version %u]\n", gen8->name, gen8->gameID, gen8->bytecodeVersion);

    // Initialize VM
    VMContext* vm = VM_create(dataWin);

    // Initialize the file system
    GlfwFileSystem* glfwFileSystem = GlfwFileSystem_create(DATAWIN_PATH);

    // Initialize the runner
    Runner* runner = Runner_create(dataWin, vm, (FileSystem*) glfwFileSystem);

    // Init GLFW
    ioPadInit(7);

    // Initialize the renderer
    Renderer* renderer = GLLegacyRenderer_create();

    renderer->vtable->init(renderer, dataWin);
    runner->renderer = renderer;

    // Initialize the audio system
    NoopAudioSystem* noopAudio = NoopAudioSystem_create();
    AudioSystem* audioSystem = (AudioSystem*) noopAudio;
    audioSystem->vtable->init(audioSystem, dataWin, (FileSystem*) glfwFileSystem);
    runner->audioSystem = audioSystem;
    
    // Initialize the first room and fire Game Start / Room Start events
    Runner_initFirstRoom(runner);

    // Main loop
    bool debugPaused = false;
    double freq = (double)sysGetTimebaseFrequency();
    double lastFrameTime = (double)__builtin_ppc_get_timebase() / freq;
    while (!runner->shouldExit) {
        // Clear last frame's pressed/released state, then poll new input events
        RunnerKeyboard_beginFrame(runner->keyboard);

        // Run the game step if the game is paused
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

        if (RunnerKeyboard_checkPressed(runner->keyboard, VK_PAGEDOWN)) {
            DataWin* dw = runner->dataWin;
            forEachIndexed(Room, room, i, dw->room.rooms, dw->room.count) {
                if (strcmp(room->name, "room_cc_joker") == 0) {
                    runner->pendingRoom = i;
                    break;
                }
            }
        }

        // Reset global interact state because I HATE when I get stuck while moving through rooms
        if (RunnerKeyboard_checkPressed(runner->keyboard, VK_PAGEUP)) {
            int32_t interactVarId = shget(runner->vmContext->globalVarNameMap, "interact");
            int32_t darkzoneVarID = shget(runner->vmContext->globalVarNameMap, "darkzone");
            int32_t charID = shget(runner->vmContext->globalVarNameMap, "char");

            runner->vmContext->globalVars[interactVarId] = RValue_makeInt32(0);
            printf("Changed global.interact [%d] value!\n", interactVarId);
            runner->vmContext->globalVars[darkzoneVarID] = RValue_makeInt32(1);
            printf("Changed global.darkzone [%d] value!\n", darkzoneVarID);
            hmput(runner->vmContext->globalArrayMap, ((int64_t) charID << 32) | (uint32_t) 0, RValue_makeInt32(1));
            printf("Changed global.char[0]!\n");
            hmput(runner->vmContext->globalArrayMap, ((int64_t) charID << 32) | (uint32_t) 1, RValue_makeInt32(2));
            printf("Changed global.char[1]!\n");
            hmput(runner->vmContext->globalArrayMap, ((int64_t) charID << 32) | (uint32_t) 2, RValue_makeInt32(3));
            printf("Changed global.char[2]!\n");
            DataWin* dw = runner->dataWin;
            forEachIndexed(Room, room, i, dw->room.rooms, dw->room.count) {
                if (strcmp(room->name, "room_dark1") == 0) {
                    runner->pendingRoom = i;
                    break;
                }
            }
            }

        // Run one game step (Begin Step, Keyboard, Alarms, Step, End Step, room transitions)
        Runner_step(runner);

        // Update audio system (gain fading, cleanup ended sounds)
        float dt = (float) ((__builtin_ppc_get_timebase()/sysGetTimebaseFrequency()) - lastFrameTime);
        if (0.0f > dt) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f; // cap delta to avoid huge fades on lag spikes
        runner->audioSystem->vtable->update(runner->audioSystem, dt);

        Room* activeRoom = runner->currentRoom;

        // Query actual framebuffer size (differs from window size on Wayland with fractional scaling)
        int fbWidth = display_width, fbHeight = display_height;

        // Clear the default framebuffer (window background) to black
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        int32_t gameW = (int32_t) gen8->defaultWindowWidth;
        int32_t gameH = (int32_t) gen8->defaultWindowHeight;

        // Compute FBO size from the bounding box of all enabled view ports
        // GMS2 sizes the application surface to the port bounds, then stretches to the window
        bool viewsEnabled = (activeRoom->flags & 1) != 0;
        if (viewsEnabled) {
        int32_t maxRight = 0;
        int32_t maxBottom = 0;
        repeat(8, vi) {
            if (!activeRoom->views[vi].enabled) continue;
                int32_t right = activeRoom->views[vi].portX + activeRoom->views[vi].portWidth;
                int32_t bottom = activeRoom->views[vi].portY + activeRoom->views[vi].portHeight;
                if (right > maxRight) maxRight = right;
                if (bottom > maxBottom) maxBottom = bottom;
            }
            if (maxRight > 0 && maxBottom > 0) {
                gameW = maxRight;
                gameH = maxBottom;
            }
        }

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

        // Render each enabled view (or a default full-screen view if views are disabled)
        bool anyViewRendered = false;

        if (viewsEnabled) {
            repeat(8, vi) {
                if (!activeRoom->views[vi].enabled) continue;

                int32_t viewX = activeRoom->views[vi].viewX;
                int32_t viewY = activeRoom->views[vi].viewY;
                int32_t viewW = activeRoom->views[vi].viewWidth;
                int32_t viewH = activeRoom->views[vi].viewHeight;
                int32_t portX = activeRoom->views[vi].portX;
                int32_t portY = activeRoom->views[vi].portY;
                int32_t portW = activeRoom->views[vi].portWidth;
                int32_t portH = activeRoom->views[vi].portHeight;
                float viewAngle = runner->viewAngles[vi];

                runner->viewCurrent = vi;
                renderer->vtable->beginView(renderer, viewX, viewY, viewW, viewH, portX, portY, portW, portH, viewAngle);

                Runner_draw(runner);

                renderer->vtable->endView(renderer);
                anyViewRendered = true;
            }
        }

        if (!anyViewRendered) {
            // No views enabled or views disabled: render with default full-screen view
            runner->viewCurrent = 0;
            renderer->vtable->beginView(renderer, 0, 0, gameW, gameH, 0, 0, gameW, gameH, 0.0f);
            Runner_draw(runner);
            renderer->vtable->endView(renderer);
        }

        // Reset view_current to 0 so non-Draw events (Step, Alarm, Create) see view_current = 0
        runner->viewCurrent = 0;

        renderer->vtable->endFrame(renderer);

        ps3glSwapBuffers();

        // Limit frame rate to room speed (skip in headless mode for max speed!!)
        if (runner->currentRoom->speed > 0) {
            double targetInterval = 1.0 / (double)runner->currentRoom->speed; // e.g. 0.0333
            double nextFrameTime = lastFrameTime + targetInterval;
            
            double now = (double)__builtin_ppc_get_timebase() / freq;

            // If we are lagging behind by more than 100ms, jump ahead 
            // to prevent the "spiral of death" catch-up logic
            if (now > nextFrameTime + 0.1) {
                nextFrameTime = now;
            }

            // Precision Sleep/Spin logic
            while (now < nextFrameTime) {
                double diff = nextFrameTime - now;
                
                // If more than 2ms left, tell the OS to sleep
                if (diff > 0.002) {
                    // sysUsleep takes MICROSECONDS (seconds * 1,000,000)
                    // We sleep for slightly less than the diff to avoid oversleeping
                    sysUsleep((uint32_t)((diff - 0.001) * 1000000.0));
                }
                
                // Update now for the spin-wait
                now = (double)__builtin_ppc_get_timebase() / freq;
            }
            
            lastFrameTime = nextFrameTime;
        } else {
            lastFrameTime = (double)__builtin_ppc_get_timebase() / freq;
        }
    }


    // Cleanup
    runner->audioSystem->vtable->destroy(runner->audioSystem);
    runner->audioSystem = nullptr;
    renderer->vtable->destroy(renderer);

    Runner_free(runner);
    GlfwFileSystem_destroy(glfwFileSystem);
    VM_free(vm);
    DataWin_free(dataWin);

    printf("Bye! :3\n");
    return 0;
}
