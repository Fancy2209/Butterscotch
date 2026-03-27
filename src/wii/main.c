#include "data_win.h"
#include "vm.h"

#include <SDL_events.h>
#include <SDL_timer.h>
#include <SDL_video.h>
#include <gccore.h>
#include <getopt.h>
#include <ogc/gx_struct.h>
#include <stdio.h>
#include <time.h>
#include <malloc.h>

#include "runner_keyboard.h"
#include "runner.h"
#include "gl_renderer.h"
#include "wii_file_system.h"
#include "noop_audio_system.h"
#include "stb_ds.h"
#include "stb_image_write.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <ogc/system.h>

#include "utils.h"

GXRModeObj *rmode = NULL;

// ===[ MAIN ]===
int main(int argc, char* argv[]) {

    printf("Loading %s...\n", "/apps/Butterscotch/data.win");
    SYS_STDIO_Report(true);
    DataWin* dataWin = DataWin_parse(
        "/apps/Butterscotch/data.win",
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
    WiiFileSystem* wiiFileSystem = WiiFileSystem_create("/apps/Butterscotch/data.win");

    // Initialize the runner
    Runner* runner = Runner_create(dataWin, vm, (FileSystem*) wiiFileSystem);
    runner->debugMode = true;


    // Init GLFW
    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        DataWin_free(dataWin);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("BTSCH", 0, 0, (int) gen8->defaultWindowWidth, (int) gen8->defaultWindowHeight, SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (window == nullptr) {
        fprintf(stderr, "Failed to create GLFW window\n");
        SDL_Quit();
        DataWin_free(dataWin);
        return 1;
    }

    SDL_GLContext* context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(0); // Disable v-sync, we control timing ourselves

    
    // Initialize the renderer
    Renderer* renderer = GLRenderer_create();
    renderer->vtable->init(renderer, dataWin);
    runner->renderer = renderer;

    // Initialize the audio system
    NoopAudioSystem* noopAudio = NoopAudioSystem_create();
    AudioSystem* audioSystem = (AudioSystem*) noopAudio;
    audioSystem->vtable->init(audioSystem, dataWin, (FileSystem*) wiiFileSystem);
    runner->audioSystem = audioSystem;

    // Initialize the first room and fire Game Start / Room Start events
    Runner_initFirstRoom(runner);

    // Main loop
    bool debugPaused = false;
    double lastFrameTime = SDL_GetTicks64();
    SDL_Event sdl_event;
    while (/*SYS_MainLoop() && */!runner->shouldExit) {
        // Clear last frame's pressed/released state, then poll new input events
        RunnerKeyboard_beginFrame(runner->keyboard);
        while(SDL_PollEvent(&sdl_event)) {
            switch(sdl_event.type) {
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP:
            }
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
            float dt = (float) (SDL_GetTicks64() - lastFrameTime);
            if (0.0f > dt) dt = 0.0f;
            if (dt > 0.1f) dt = 0.1f; // cap delta to avoid huge fades on lag spikes
            runner->audioSystem->vtable->update(runner->audioSystem, dt);
        }

        Room* activeRoom = runner->currentRoom;

        // Query actual framebuffer size (differs from window size on Wayland with fractional scaling)
        int fbWidth, fbHeight;
        SDL_GL_GetDrawableSize(window, &fbWidth, &fbHeight);

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

        SDL_GL_SwapWindow(window);

        // Limit frame rate to room speed (skip in headless mode for max speed!!)
        if (runner->currentRoom->speed > 0) {
            double targetFrameTime = 1.0 / (runner->currentRoom->speed);
            double nextFrameTime = lastFrameTime + targetFrameTime;
            // Sleep for most of the remaining time, then spin-wait for precision
            double remaining = nextFrameTime - SDL_GetTicks64();
            if (remaining > 0.002) {
               SDL_Delay(remaining);
            }
            while (SDL_GetTicks64() < nextFrameTime) {
                // Spin-wait for the remaining sub-millisecond
            }
            lastFrameTime = nextFrameTime;
        } else {
            lastFrameTime = SDL_GetTicks64();
        }
    }
    // Cleanup
    runner->audioSystem->vtable->destroy(runner->audioSystem);
    runner->audioSystem = nullptr;
    renderer->vtable->destroy(renderer);

    SDL_DestroyWindow(window);
    SDL_Quit();

    Runner_free(runner);
    WiiFileSystem_destroy(wiiFileSystem);
    VM_free(vm);
    DataWin_free(dataWin);

    printf("Bye! :3\n");
    return 0;
}
