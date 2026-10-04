// звуковой микшер чисто чтоб играло
#include "drivers/system/sound_manager.h"
#include "drivers/system/ac97.h"
#include "drivers/system/hda.h"

static audio_dev_t active_dev = AUDIO_DEV_NONE;

int sound_init(void) {
    if (ac97_init()) {
        active_dev = AUDIO_DEV_AC97;
        return 0;
    }

    if (hda_init()) {
        active_dev = AUDIO_DEV_HDA;
        return -2;
    }

    active_dev = AUDIO_DEV_NONE;
    return -1;
}

int sound_set_volume(uint8_t vol) {
    if (vol > 100) return -1;
    if (active_dev == AUDIO_DEV_AC97) { ac97_set_volume(vol); return 0; }
    return -1;
}

int sound_play_pcm(const void *data, uint32_t bytes) {
    if (active_dev == AUDIO_DEV_AC97) return ac97_play_pcm((const uint8_t *)data, bytes);
    return -1;
}

audio_dev_t sound_device(void) { return active_dev; }
int sound_is_ready(void) { return active_dev == AUDIO_DEV_AC97; }
