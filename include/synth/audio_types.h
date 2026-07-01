#ifndef SYNTH_AUDIO_TYPES_H
#define SYNTH_AUDIO_TYPES_H

#include <stddef.h>

typedef float synth_sample;

typedef struct synth_audio_buffer {
    synth_sample *samples;
    size_t frame_count;
    unsigned int channel_count;
} synth_audio_buffer;

#endif
