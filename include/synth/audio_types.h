#ifndef SYNTH_AUDIO_TYPES_H
#define SYNTH_AUDIO_TYPES_H

#include <stddef.h>

// a single audio sample value.
typedef float synth_sample;

// the number of channels in a stereo buffer.
#define SYNTH_STEREO_CHANNEL_COUNT 2

// a stereo buffer that points at left and right sample arrays.
typedef struct synth_audio_buffer {
    synth_sample *left;
    synth_sample *right;
    size_t frame_count;
} synth_audio_buffer;

#endif
