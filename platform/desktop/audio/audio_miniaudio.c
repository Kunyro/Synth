#define MINIAUDIO_IMPLEMENTATION
#include "audio/audio_miniaudio.h"

#include <stdio.h>
#include <string.h>

static void audio_callback(ma_device *device, void *output, const void *input, ma_uint32 frame_count)
{
    audio_miniaudio_device *audio = (audio_miniaudio_device *)device->pUserData;

    (void)input;
    audio->render(audio->user_data, (float *)output, frame_count, device->playback.channels);
}

int audio_miniaudio_init(
    audio_miniaudio_device *device,
    unsigned int sample_rate,
    unsigned int channel_count,
    audio_device_render_fn render,
    void *user_data)
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    ma_result result;

    memset(device, 0, sizeof(*device));
    device->render = render;
    device->user_data = user_data;

    if (ma_mutex_init(&device->lock) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio lock.\n");
        return 0;
    }

    config.playback.format = ma_format_f32;
    config.playback.channels = channel_count;
    config.sampleRate = sample_rate;
    config.dataCallback = audio_callback;
    config.pUserData = device;

    result = ma_device_init(NULL, &config, &device->device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio device: %s\n", ma_result_description(result));
        ma_mutex_uninit(&device->lock);
        return 0;
    }

    return 1;
}

int audio_miniaudio_start(audio_miniaudio_device *device)
{
    ma_result result = ma_device_start(&device->device);

    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to start audio device: %s\n", ma_result_description(result));
        return 0;
    }

    return 1;
}

void audio_miniaudio_uninit(audio_miniaudio_device *device)
{
    ma_device_uninit(&device->device);
    ma_mutex_uninit(&device->lock);
}

void audio_miniaudio_lock(audio_miniaudio_device *device)
{
    ma_mutex_lock(&device->lock);
}

void audio_miniaudio_unlock(audio_miniaudio_device *device)
{
    ma_mutex_unlock(&device->lock);
}

void audio_miniaudio_sleep(unsigned int milliseconds)
{
    ma_sleep(milliseconds);
}
