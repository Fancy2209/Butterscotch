#include "noop_audio_system.h"

#include <stdlib.h>

static void noopInit(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) DataWin* dataWin, __attribute__((unused)) FileSystem* fileSystem) {}

static void noopDestroy(AudioSystem* audio) {
    free(audio);
}

static void noopUpdate(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) float deltaTime) {}

static int32_t noopPlaySound(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundIndex, __attribute__((unused)) int32_t priority, __attribute__((unused)) bool loop) {
    return -1;
}

static void noopStopSound(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance) {}

static void noopStopAll(__attribute__((unused)) AudioSystem* audio) {}

static bool noopIsPlaying(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance) {
    return false;
}

static void noopPauseSound(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance) {}

static void noopResumeSound(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance) {}

static void noopPauseAll(__attribute__((unused)) AudioSystem* audio) {}

static void noopResumeAll(__attribute__((unused)) AudioSystem* audio) {}

static void noopSetSoundGain(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance, __attribute__((unused)) float gain, __attribute__((unused)) uint32_t timeMs) {}

static float noopGetSoundGain(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance) {
    return 1.0f;
}

static void noopSetSoundPitch(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance, __attribute__((unused)) float pitch) {}

static float noopGetSoundPitch(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance) {
    return 1.0f;
}

static float noopGetTrackPosition(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance) {
    return 0.0f;
}

static void noopSetTrackPosition(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t soundOrInstance, __attribute__((unused)) float positionSeconds) {}

static void noopSetMasterGain(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) float gain) {}

static void noopSetChannelCount(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t count) {}

static void noopGroupLoad(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t groupIndex) {}

static bool noopGroupIsLoaded(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t groupIndex) {
    return true;
}

static int32_t noopCreateStream(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) const char* filename) {
    return -1;
}

static bool noopDestroyStream(__attribute__((unused)) AudioSystem* audio, __attribute__((unused)) int32_t streamIndex) {
    return false;
}

static AudioSystemVtable noopVtable = {
    .init = noopInit,
    .destroy = noopDestroy,
    .update = noopUpdate,
    .playSound = noopPlaySound,
    .stopSound = noopStopSound,
    .stopAll = noopStopAll,
    .isPlaying = noopIsPlaying,
    .pauseSound = noopPauseSound,
    .resumeSound = noopResumeSound,
    .pauseAll = noopPauseAll,
    .resumeAll = noopResumeAll,
    .setSoundGain = noopSetSoundGain,
    .getSoundGain = noopGetSoundGain,
    .setSoundPitch = noopSetSoundPitch,
    .getSoundPitch = noopGetSoundPitch,
    .getTrackPosition = noopGetTrackPosition,
    .setTrackPosition = noopSetTrackPosition,
    .setMasterGain = noopSetMasterGain,
    .setChannelCount = noopSetChannelCount,
    .groupLoad = noopGroupLoad,
    .groupIsLoaded = noopGroupIsLoaded,
    .createStream = noopCreateStream,
    .destroyStream = noopDestroyStream,
};

NoopAudioSystem* NoopAudioSystem_create(void) {
    NoopAudioSystem* audio = calloc(1, sizeof(NoopAudioSystem));
    audio->base.vtable = &noopVtable;
    return audio;
}
