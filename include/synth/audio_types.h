#ifndef SYNTH_AUDIO_TYPES_H
#define SYNTH_AUDIO_TYPES_H

#include <stddef.h>

// a single audio sample value.
typedef float synth_sample;

// the number of channels in a stereo buffer.
#define SYNTH_STEREO_CHANNEL_COUNT 2

// one stereo frame used while samples move through the synth.
typedef struct synth_stereo_sample {
    synth_sample left;
    synth_sample right;
} synth_stereo_sample;

// a stereo buffer that points at left and right sample arrays.
typedef struct synth_audio_buffer {
    synth_sample *left;
    synth_sample *right;
    size_t frame_count;
} synth_audio_buffer;

#endif
