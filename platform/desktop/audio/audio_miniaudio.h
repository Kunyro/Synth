#ifndef DESKTOP_AUDIO_MINIAUDIO_H
#define DESKTOP_AUDIO_MINIAUDIO_H

#include "audio_device.h"
#include "miniaudio/miniaudio.h"

typedef struct audio_miniaudio_device {
    ma_device device;
    ma_mutex lock;
    audio_device_render_fn render;
    void *user_data;
} audio_miniaudio_device;

int audio_miniaudio_init(
    audio_miniaudio_device *device,
    unsigned int sample_rate,
    unsigned int channel_count,
    audio_device_render_fn render,
    void *user_data);
int audio_miniaudio_start(audio_miniaudio_device *device);
void audio_miniaudio_uninit(audio_miniaudio_device *device);
void audio_miniaudio_lock(audio_miniaudio_device *device);
void audio_miniaudio_unlock(audio_miniaudio_device *device);
void audio_miniaudio_sleep(unsigned int milliseconds);

#endif
