#ifndef SYNTH_CHORUS_H
#define SYNTH_CHORUS_H

#include <stddef.h>

#include "synth/audio_types.h"

#define SYNTH_CHORUS_MIN_SAMPLE_RATE 1.0f
#define SYNTH_CHORUS_MIN_RATE_HZ 0.05f
#define SYNTH_CHORUS_MAX_RATE_HZ 8.0f
#define SYNTH_CHORUS_DEFAULT_RATE_HZ 0.80f
#define SYNTH_CHORUS_MIN_DELAY_SECONDS 0.006f
#define SYNTH_CHORUS_MAX_DELAY_SECONDS 0.030f
#define SYNTH_CHORUS_DEFAULT_DELAY_SECONDS 0.018f
#define SYNTH_CHORUS_MAX_MODULATION_SECONDS 0.012f
#define SYNTH_CHORUS_DEFAULT_DEPTH 0.70f
#define SYNTH_CHORUS_DEFAULT_WIDTH 1.0f
#define SYNTH_CHORUS_MAX_FEEDBACK 0.35f
#define SYNTH_CHORUS_VOICE_COUNT 3

// effective controls, separate from persistent dsp history
typedef struct synth_chorus_params {
    float rate_hz;
    float depth;
    float mix;
    float width;
    float delay_seconds;
    float feedback;
} synth_chorus_params;

typedef struct synth_chorus_delay_line {
    float *left;
    float *right;
    size_t write_index;
    size_t capacity_frames;
} synth_chorus_delay_line;

typedef struct synth_chorus {
    float sample_rate;
    float rate_hz;
    float depth;
    float mix;
    float width;
    float delay_seconds;
    float feedback;
    float phases[SYNTH_CHORUS_VOICE_COUNT];
    synth_chorus_delay_line delay;
} synth_chorus;

void synth_chorus_init(synth_chorus *chorus, float sample_rate);
void synth_chorus_uninit(synth_chorus *chorus);
void synth_chorus_set_sample_rate(synth_chorus *chorus, float sample_rate);
void synth_chorus_set_rate(synth_chorus *chorus, float hz);
void synth_chorus_set_depth(synth_chorus *chorus, float depth);
void synth_chorus_set_mix(synth_chorus *chorus, float mix);
void synth_chorus_set_width(synth_chorus *chorus, float width);
void synth_chorus_set_delay(synth_chorus *chorus, float seconds);
void synth_chorus_set_feedback(synth_chorus *chorus, float feedback);
float synth_chorus_get_rate(const synth_chorus *chorus);
float synth_chorus_get_depth(const synth_chorus *chorus);
float synth_chorus_get_mix(const synth_chorus *chorus);
float synth_chorus_get_width(const synth_chorus *chorus);
float synth_chorus_get_delay(const synth_chorus *chorus);
float synth_chorus_get_feedback(const synth_chorus *chorus);
synth_stereo_sample synth_chorus_process(
    synth_chorus *chorus,
    synth_stereo_sample input);

// returns a copy of stored controls; processing overrides never change those bases
synth_chorus_params synth_chorus_get_params(const synth_chorus *effect);
// effective controls must be finite and within the module bounds
// advances dsp history without storing controls or calling parameter setters
synth_stereo_sample synth_chorus_process_with_params(
    synth_chorus *effect,
    synth_stereo_sample input,
    const synth_chorus_params *params);

#endif
