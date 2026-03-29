#include "../data_win.h"
#include "ps3gl.h"
#include "rsxutil.h"
#include "../vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#include "../runner_keyboard.h"
#include "../runner.h"
#include "../input_recording.h"
#include "gl_legacy_renderer.h"
#include "glfw_file_system.h"
#include "../noop_audio_system.h"
#include "stb_ds.h"
#include "stb_image_write.h"

#include "utils.h"

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
    ps3glInit();

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
    double lastFrameTime = __builtin_ppc_get_timebase();
    while (!runner->shouldExit) {
        // Clear last frame's pressed/released state, then poll new input events
        RunnerKeyboard_beginFrame(runner->keyboard);

        // Run the game step if the game is paused
        bool shouldStep = true;
        double frameStartTime = 0;

        if (shouldStep) {
            // Run one game step (Begin Step, Keyboard, Alarms, Step, End Step, room transitions)
            Runner_step(runner);

            // Update audio system (gain fading, cleanup ended sounds)
            float dt = (float) (__builtin_ppc_get_timebase() - lastFrameTime);
            if (0.0f > dt) dt = 0.0f;
            if (dt > 0.1f) dt = 0.1f; // cap delta to avoid huge fades on lag spikes
            runner->audioSystem->vtable->update(runner->audioSystem, dt);
        }

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

        float scaleX = (float)fbWidth / gameW;
        float scaleY = (float)fbHeight / gameH;

        // Keep aspect ratio
        float scale = (scaleX < scaleY) ? scaleX : scaleY;

        // Final size
        int vpW = (int)(gameW * scale);
        int vpH = (int)(gameH * scale);

        // Centering
        int vpX = (fbWidth - vpW) / 2;
        int vpY = (fbHeight - vpH) / 2;

        renderer->vtable->beginFrame(renderer, vpX, vpY, vpW, vpH);

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
        lastFrameTime = __builtin_ppc_get_timebase();
    }


    // Cleanup
    runner->audioSystem->vtable->destroy(runner->audioSystem);
    runner->audioSystem = NULL;
    renderer->vtable->destroy(renderer);

    Runner_free(runner);
    GlfwFileSystem_destroy(glfwFileSystem);
    VM_free(vm);
    DataWin_free(dataWin);

    printf("Bye! :3\n");
    return 0;
}
