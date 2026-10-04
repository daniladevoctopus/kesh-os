// апишка звука
#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <stdint.h>

typedef enum {
    AUDIO_DEV_NONE = 0,
    AUDIO_DEV_AC97,
    AUDIO_DEV_HDA
} audio_dev_t;

int sound_init(void);
int sound_set_volume(uint8_t vol);
int sound_play_pcm(const void *data, uint32_t bytes);
audio_dev_t sound_device(void);
int sound_is_ready(void);

#endif
