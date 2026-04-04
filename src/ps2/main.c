#include <kernel.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <stdio.h>
#include <malloc.h>
#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <gsFontM.h>
#include <libpad.h>
#include <libmc.h>
#include <timer.h>
#include <unistd.h>
#include <sbv_patches.h>

#include "runner.h"
#include "runner_keyboard.h"
#include "vm.h"
#include "../data_win.h"
#include "../json_reader.h"
#include "ps2_file_system.h"
#ifndef DISABLE_AUDIO
#include "ps2_audio_system.h"
#endif
#include "gs_renderer.h"
#include "ps2_utils.h"
#include "utils.h"
#include "irx.h"

// The maximum memory of a normal PS2 console
// Developer consoles may have more memory, but because ps2sdk does not have a way to know
// how much memory the console really has, we will use this value instead
static int MAX_MEMORY_BYTES = 33554432;

// 256-byte aligned buffer for libpad
static char padBuf[256] __attribute__((aligned(64)));

// Controller button to GML key mapping
typedef struct {
    uint16_t padButton;
    int32_t gmlKey;
} PadMapping;

static PadMapping* padMappings = nullptr;
static int padMappingCount = 0;

// Previous frame's button state for detecting press/release edges
static uint16_t prevButtons = 0xFFFF; // All buttons released (buttons are active-low)

static void initIop() {
    SifInitRpc(0);

#if defined(DTL_SUPPORT)
    // Required to get console output on DTL systems.
    while (!SifIopReset("rom0:UDNL", 0));
#else
    while (!SifIopReset("", 0));
#endif // DTL_SUPPORT

    while (!SifIopSync());
    SifInitRpc(0);
    sbv_patch_enable_lmb();
}

int main(int argc, char* argv[]) {
    initIop();

    PS2Utils_extractDeviceKey(argv[0]);

    fprintf(stderr, "argv0 is %s, device key is %s\n", argv[0], deviceKey.key);

    PS2Utils_loadFSDrivers();

    fprintf(stderr, "Loaded FS drivers!\n");

    const char* dataWinPath = PS2Utils_createDevicePath("DATA.WIN");

    printf("Butterscotch PS2 - Loading %s\n", dataWinPath);

    // ===[ Initialize gsKit ]===
    // This must happen first so we can show the loading screen during other init steps
    GSGLOBAL* gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->PSM = GS_PSM_CT16;
    gsGlobal->PSMZ = GS_PSMZ_16;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;
    gsGlobal->ZBuffering = GS_SETTING_OFF;

    gsGlobal->PrimAAEnable = GS_SETTING_OFF;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);
    // Use ONE SHOT mode
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    // ===[ Initialize Controller ]===
    fprintf(stderr, "Initializing controller...\n");

    int ret;
    ret = SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, nullptr, nullptr);
    if (0 > ret) {
        printf("Failed to load sio2man: %d\n", ret);
        return 1;
    }
    ret = SifExecModuleBuffer(mcman_irx, size_mcman_irx, 0, nullptr, nullptr);
    if (0 > ret) {
        printf("Failed to load mcman: %d\n", ret);
        return 1;
    }
    ret = SifExecModuleBuffer(mcserv_irx, size_mcserv_irx, 0, nullptr, nullptr);
    if (0 > ret) {
        printf("Failed to load mcserv: %d\n", ret);
        return 1;
    }
    ret = mcInit(MC_TYPE_MC);
    if (0 > ret) {
        printf("Failed to init libmc: %d\n", ret);
        return 1;
    }
    ret = SifExecModuleBuffer(padman_irx, size_padman_irx, 0, nullptr, nullptr);
    if (0 > ret) {
        printf("Failed to load padman: %d\n", ret);
        return 1;
    }

    padInit(0);
    padPortOpen(0, 0, padBuf);

#ifndef DISABLE_AUDIO
    // ===[ Load Audio IOP Modules ]===
    ret = SifExecModuleBuffer(freesd_irx, size_freesd_irx, 0, nullptr, nullptr);
    if (0 > ret) {
        printf("Failed to load freesd: %d\n", ret);
    }
    ret = SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, nullptr, nullptr);
    if (0 > ret) {
        printf("Failed to load audsrv: %d\n", ret);
    }
#endif

    fprintf(stderr, "Waiting for controller...\n");
    int padState;
    do {
        padState = padGetState(0, 0);
    } while (PAD_STATE_STABLE != padState && PAD_STATE_FINDCTP1 != padState);
    fprintf(stderr, "Controller initialized\n");

    fprintf(stderr, "Parsing DATA.WIN...\n");
    DataWin* dataWin = DataWin_parse(
        dataWinPath,
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
            .parseTxtr = false,
            .parseAudo = false,
            .skipLoadingPreciseMasksForNonPreciseSprites = true,
        }
    );
    free(dataWinPath);

    {
        void* heapTop = sbrk(0);
        int32_t usedBytes = (int32_t) (uintptr_t) heapTop;
        int32_t freeBytes = MAX_MEMORY_BYTES - usedBytes;
        printf("Memory after data.win parsing: used=%d bytes (%.1f KB), total=%d bytes (%.1f KB), free=%d bytes (%.1f KB)\n", usedBytes, usedBytes / 1024.0f, MAX_MEMORY_BYTES, MAX_MEMORY_BYTES / 1024.0f, freeBytes, freeBytes / 1024.0f);
    }

    fprintf(stderr, "Creating renderer...\n");
    Renderer* renderer = GsRenderer_create(gsGlobal);

    fprintf(stderr, "Loading CONFIG.JSN...\n");
    char* configJsonPath = PS2Utils_createDevicePath("CONFIG.JSN");
    FILE* configFile = fopen(configJsonPath, "rb");
    JsonValue* configRoot = nullptr;

    if (configFile != nullptr) {
        fseek(configFile, 0, SEEK_END);
        long configSize = ftell(configFile);
        fseek(configFile, 0, SEEK_SET);

        char* configJsonText = safeMalloc((size_t) configSize + 1);
        size_t configBytesRead = fread(configJsonText, 1, (size_t) configSize, configFile);
        configJsonText[configBytesRead] = '\0';
        fclose(configFile);

        configRoot = JsonReader_parse(configJsonText);
        free(configJsonText);
    }
    free(configJsonPath);

    if (configRoot == nullptr) {
        fprintf(stderr, "CONFIG.JSN invalid or not found!\n");
        return 1;
    }

    FileSystem* fileSystem = Ps2FileSystem_create(configRoot, dataWin->gen8.displayName);
    if (fileSystem == nullptr) {
        fprintf(stderr, "CONFIG.JSN is missing the fileSystem configuration!\n");
        return 1;
    }

    VMContext* vm = VM_create(dataWin);
    Runner* runner = Runner_create(dataWin, vm, fileSystem);

    // Parse disabledObjects from CONFIG.JSN
    JsonValue* disabledObjectsArr = JsonReader_getObject(configRoot, "disabledObjects");
    if (disabledObjectsArr != nullptr && JsonReader_isArray(disabledObjectsArr)) {
        sh_new_strdup(runner->disabledObjects);
        int disabledCount = JsonReader_arrayLength(disabledObjectsArr);
        repeat(disabledCount, i) {
            JsonValue* elem = JsonReader_getArrayElement(disabledObjectsArr, i);
            if (elem != nullptr && JsonReader_isString(elem)) {
                const char* objName = JsonReader_getString(elem);
                shput(runner->disabledObjects, objName, 1);
                printf("Disabled object: %s\n", objName);
            }
        }
    }

    // Parse controllerMappings from CONFIG.JSN
    JsonValue* controllerMappingsObj = JsonReader_getObject(configRoot, "controllerMappings");
    if (controllerMappingsObj != nullptr && JsonReader_isObject(controllerMappingsObj)) {
        padMappingCount = JsonReader_objectLength(controllerMappingsObj);
        padMappings = safeMalloc(sizeof(PadMapping) * padMappingCount);
        repeat(padMappingCount, i) {
            const char* padButtonStr = JsonReader_getObjectKey(controllerMappingsObj, i);
            JsonValue* gmlKeyVal = JsonReader_getObjectValue(controllerMappingsObj, i);
            padMappings[i].padButton = (uint16_t) atoi(padButtonStr);
            padMappings[i].gmlKey = (int32_t) JsonReader_getInt(gmlKeyVal);
            printf("CONFIG.JSN: controllerMapping pad=%d -> gmlKey=%d\n", padMappings[i].padButton, padMappings[i].gmlKey);
        }
    }

    {
        void* heapTop = sbrk(0);
        int32_t usedBytes = (int32_t) (uintptr_t) heapTop;
        int32_t freeBytes = MAX_MEMORY_BYTES - usedBytes;
        printf("Memory after VM and runner creation: used=%d bytes (%.1f KB), total=%d bytes (%.1f KB), free=%d bytes (%.1f KB)\n", usedBytes, usedBytes / 1024.0f, MAX_MEMORY_BYTES, MAX_MEMORY_BYTES / 1024.0f, freeBytes, freeBytes / 1024.0f);
    }

    runner->renderer = renderer;

    DataWin_printDebugSummary(dataWin, 0);

    // ===[ Initialize Audio System ]===
#ifndef DISABLE_AUDIO
    fprintf(stderr, "Initializing audio...\n");
    Ps2AudioSystem* ps2Audio = Ps2AudioSystem_create();
    AudioSystem* audioSystem = (AudioSystem*) ps2Audio;
    audioSystem->vtable->init(audioSystem, dataWin, fileSystem);
    runner->audioSystem = audioSystem;
#else
    runner->audioSystem = (AudioSystem*) NoopAudioSystem_create();
#endif

    fprintf(stderr, "Initializing renderer...\n");
    renderer->vtable->init(renderer, dataWin);

    fprintf(stderr, "Initializing first room...\n");
    Runner_initFirstRoom(runner);

    fprintf(stderr, "Reticulating splines...\n");
    Gen8* gen8 = &dataWin->gen8;
    int32_t gameW = (int32_t) gen8->defaultWindowWidth;
    int32_t gameH = (int32_t) gen8->defaultWindowHeight;

    // ===[ Initialize Timer ]===
    InitTimer(kBUSCLK);
    StartTimerSystemTime();

    // ===[ Main Loop ]===
    bool debugOverlayEnabled = JsonReader_getBool(JsonReader_getObject(configRoot, "debugOverlayEnabled"));
    while (!runner->shouldExit) {
        u64 frameStartTime = GetTimerSystemTime();
        // ===[ Poll Controller (always poll every vsync) ]===
        // NOTE: We do NOT call RunnerKeyboard_beginFrame here! Pressed/released edges accumulate across vsyncs so that quick taps on non-game-frame
        // vsyncs are not lost
        //
        // beginFrame is called after the game consumes input.

        struct padButtonStatus padStatus;
        unsigned char padResult = padRead(0, 0, &padStatus);
        uint16_t buttons = 0xFFFF; // all released by default
        if (padResult != 0) {
            buttons = padStatus.btns;

            repeat(padMappingCount, i) {
                uint16_t mask = padMappings[i].padButton;
                int32_t gmlKey = padMappings[i].gmlKey;

                // PS2 buttons are active-low: 0 = pressed, 1 = released
                bool wasPressed = (prevButtons & mask) == 0;
                bool isPressed = (buttons & mask) == 0;

                if (isPressed && !wasPressed) {
                    RunnerKeyboard_onKeyDown(runner->keyboard, gmlKey);
                } else if (!isPressed && wasPressed) {
                    RunnerKeyboard_onKeyUp(runner->keyboard, gmlKey);
                }
            }

            prevButtons = buttons;
        }

        // R2 removes speed cap (ignore waiting for vsync)
        bool speedCapRemoved = (padResult != 0) && ((buttons & PAD_R2) == 0);

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
            forEachIndexed(Room, room, i, dw->room.rooms, dw->room.count) {
                if (strcmp(room->name, "room_asrielappears") == 0) {
                    runner->pendingRoom = i;
                    runner->audioSystem->vtable->stopAll(runner->audioSystem);
                    break;
                }
            }
        }

        if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F12)) {
            debugOverlayEnabled = !debugOverlayEnabled;
        }

        // Reset global interact state because I HATE when I get stuck while moving through rooms
        if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F10)) {
            int32_t interactVarId = shget(runner->vmContext->globalVarNameMap, "interact");

            runner->vmContext->globalVars[interactVarId] = RValue_makeInt32(0);
            printf("Changed global.interact [%d] value!\n", interactVarId);
        }

        // ===[ Game Logic ]===
        uint32_t roomSpeed = runner->currentRoom->speed;

        Runner_step(runner);

        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));

        renderer->vtable->beginFrame(renderer, gameW, gameH, 640, 448);

        // Clear with room background color
        if (runner->drawBackgroundColor) {
            uint8_t bgR = BGR_R(runner->backgroundColor);
            uint8_t bgG = BGR_G(runner->backgroundColor);
            uint8_t bgB = BGR_B(runner->backgroundColor);
            u64 bgColor = GS_SETREG_RGBAQ(bgR, bgG, bgB, 0x80, 0x00);
            gsKit_prim_sprite(gsGlobal, 0, 0, 640, 448, 0, bgColor);
        }

        // Render views
        Room* activeRoom = runner->currentRoom;
        bool anyViewRendered = false;

        bool viewsEnabled = (activeRoom->flags & 1) != 0;

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

                runner->viewCurrent = (int32_t) vi;
                renderer->vtable->beginView(renderer, viewX, viewY, viewW, viewH, portX, portY, portW, portH, viewAngle);

                Runner_draw(runner);

                renderer->vtable->endView(renderer);
                anyViewRendered = true;
            }
        }

        if (!anyViewRendered) {
            // No views enabled: render with default full-screen view
            runner->viewCurrent = 0;
            renderer->vtable->beginView(renderer, 0, 0, gameW, gameH, 0, 0, gameW, gameH, 0.0f);
            Runner_draw(runner);
            renderer->vtable->endView(renderer);
        }

        runner->viewCurrent = 0;

        renderer->vtable->endFrame(renderer);

        // Clear pressed/released edges after both Step and Draw have consumed input
        // This MUST be after Runner_draw because games CAN handle input in Draw events (e.g. Undertale's naming screen)
        RunnerKeyboard_beginFrame(runner->keyboard);

        // Update audio system (gain fading, stream to audsrv)
        float dt = 1.0f / (float) roomSpeed;
        if (0.0f > dt) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;
        runner->audioSystem->vtable->update(runner->audioSystem, dt);

        u64 runnerEndTime = GetTimerSystemTime();
        u64 duration = runnerEndTime - frameStartTime;
        float tickTime = (float) duration / (float) (kBUSCLK / 1000);

        // ===[ Debug Overlay ]===
        if (debugOverlayEnabled) {
            u64 debugColor = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00);
            // sbrk(0) returns the actual heap frontier; true free = top of RAM - sbrk frontier
            void* heapTop = sbrk(0);
            int32_t freeBytes = MAX_MEMORY_BYTES - (int32_t) (uintptr_t) heapTop;

            char debugText[256];
            uint32_t vramFreeBytes = GS_VRAM_SIZE - gsGlobal->CurrentPointer;

            // Count atlases loaded in VRAM and EE RAM cache
            GsRenderer* gsRenderer = (GsRenderer*) renderer;
            uint32_t vramAtlasCount = 0;
            uint32_t eeramAtlasCount = 0;
            repeat(gsRenderer->atlasCount, ai) {
                if (gsRenderer->atlasToChunk[ai] >= 0) vramAtlasCount++;
                if (gsRenderer->eeCacheEntries[ai].atlasId >= 0) eeramAtlasCount++;
            }

            snprintf(debugText, sizeof(debugText), "Tick: %.2fms\nFree: %d bytes\nVRAM Free: %lu bytes\nRoom Speed: %u%s\nAtlas: (%u, %u, %u)", tickTime, freeBytes, (unsigned long) vramFreeBytes, roomSpeed, speedCapRemoved ? " [UNCAPPED]" : "", vramAtlasCount, eeramAtlasCount, gsRenderer->atlasCount);
            renderer->drawColor = 0xFFFFFF;
            renderer->drawAlpha = 1.0f;
            renderer->drawFont = 1;
            renderer->drawHalign = 0;
            renderer->drawValign = 0;

            renderer->vtable->beginView(renderer, 0, 0, gameW, gameH, 0, 0, gameW, gameH, 0.0f);
            renderer->vtable->drawText(renderer, debugText, 20, 20, 0.6f, 0.6f, 0);
            renderer->vtable->endView(renderer);
        }

        // Execute draw queue and flip buffers
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);

        // Busy-wait until enough time has elapsed for this frame if needed
        if (!speedCapRemoved && roomSpeed > 0) {
            u64 targetTicks = kBUSCLK / roomSpeed;
            while (targetTicks > GetTimerSystemTime() - frameStartTime) {
                // spin spin spin!
            }
        }
    }

    runner->audioSystem->vtable->destroy(runner->audioSystem);
    runner->audioSystem = nullptr;
    renderer->vtable->destroy(renderer);
    gsKit_deinit_global(gsGlobal);
    gsGlobal = nullptr;
    DataWin_free(dataWin);

    return 0;
}
