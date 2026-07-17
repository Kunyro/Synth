#ifndef SYNTH_DELAY_H
#define SYNTH_DELAY_H

#include <stddef.h>

#include "synth/audio_types.h"

#define SYNTH_DELAY_MAX_FRAMES 96000
#define SYNTH_DELAY_MIN_TIME_SECONDS 0.001f
#define SYNTH_DELAY_MAX_TIME_SECONDS 2.0f
#define SYNTH_DELAY_DEFAULT_TIME_SECONDS 0.25f
#define SYNTH_DELAY_MAX_FEEDBACK 0.95f

typedef struct synth_delay {
    float left[SYNTH_DELAY_MAX_FRAMES];
    float right[SYNTH_DELAY_MAX_FRAMES];
    size_t write_index;
    size_t delay_frames;
    float sample_rate;
    float time_seconds;
    float feedback;
    float mix;
} synth_delay;

void synth_delay_init(synth_delay *delay, float sample_rate);
void synth_delay_set_sample_rate(synth_delay *delay, float sample_rate);
void synth_delay_set_time(synth_delay *delay, float seconds);
void synth_delay_set_feedback(synth_delay *delay, float feedback);
void synth_delay_set_mix(synth_delay *delay, float mix);
float synth_delay_get_time(const synth_delay *delay);
float synth_delay_get_feedback(const synth_delay *delay);
float synth_delay_get_mix(const synth_delay *delay);
synth_stereo_sample synth_delay_process(
    synth_delay *delay,
    synth_stereo_sample input);

#endif
