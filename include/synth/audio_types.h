#ifndef SYNTH_AUDIO_TYPES_H
#define SYNTH_AUDIO_TYPES_H

#include <stddef.h>

typedef float synth_sample;

#define SYNTH_STEREO_CHANNEL_COUNT 2

typedef struct synth_audio_buffer {
    synth_sample *left;
    synth_sample *right;
    size_t frame_count;
} synth_audio_buffer;

#endif
