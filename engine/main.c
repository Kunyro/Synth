#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 48000
#define CHANNEL_COUNT 2

typedef struct oscillator {
    float frequency;
    float amplitude;
    double phase;
} oscillator;

static void audio_callback(ma_device *device, void *output, const void *input, ma_uint32 frame_count)
{
    oscillator *osc = (oscillator *)device->pUserData;
    float *out = (float *)output;
    const double tau = 6.28318530717958647692;
    const double phase_step = tau * (double)osc->frequency / (double)device->sampleRate;

    (void)input;

    for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
        const float sample = (float)sin(osc->phase) * osc->amplitude;

        for (ma_uint32 channel = 0; channel < device->playback.channels; ++channel) {
            out[frame * device->playback.channels + channel] = sample;
        }

        osc->phase += phase_step;
        if (osc->phase >= tau) {
            osc->phase -= tau;
        }
    }
}

int main(int argc, char **argv)
{
    oscillator osc = {
        .frequency = 220.0f,
        .amplitude = 0.20f,
        .phase = 0.0,
    };

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    ma_device device;
    ma_result result;
    double seconds = 0.0;

    if (argc > 1) {
        osc.frequency = (float)strtod(argv[1], NULL);
    }

    if (argc > 2) {
        seconds = strtod(argv[2], NULL);
    }

    config.playback.format = ma_format_f32;
    config.playback.channels = CHANNEL_COUNT;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = audio_callback;
    config.pUserData = &osc;

    result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio device: %s\n", ma_result_description(result));
        return 1;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to start audio device: %s\n", ma_result_description(result));
        ma_device_uninit(&device);
        return 1;
    }

    printf("Playing %.2f Hz sine wave. ", osc.frequency);

    if (seconds > 0.0) {
        printf("Stopping after %.2f seconds.\n", seconds);
        ma_sleep((ma_uint32)(seconds * 1000.0));
    } else {
        printf("Press Enter to stop.\n");
        (void)getchar();
    }

    ma_device_uninit(&device);
    return 0;
}
