#ifndef SYNTH_FLANGER_H
#define SYNTH_FLANGER_H

#include <stddef.h>

#include "synth/audio_types.h"

#define SYNTH_FLANGER_MIN_SAMPLE_RATE 1.0f
#define SYNTH_FLANGER_MIN_RATE_HZ 0.02f
#define SYNTH_FLANGER_MAX_RATE_HZ 16.0f
#define SYNTH_FLANGER_DEFAULT_RATE_HZ 0.50f
#define SYNTH_FLANGER_MIN_MANUAL_SECONDS 0.0002f
#define SYNTH_FLANGER_MAX_MANUAL_SECONDS 0.008f
#define SYNTH_FLANGER_DEFAULT_MANUAL_SECONDS 0.0005f
#define SYNTH_FLANGER_MAX_DELAY_SECONDS 0.012f
#define SYNTH_FLANGER_DEFAULT_INTENSITY 0.65f
#define SYNTH_FLANGER_DEFAULT_DEPTH SYNTH_FLANGER_DEFAULT_INTENSITY
#define SYNTH_FLANGER_MAX_FEEDBACK 0.95f
#define SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK 0.10f
#define SYNTH_FLANGER_DEFAULT_FEEDBACK 0.7853f

// effective controls, separate from persistent dsp history
typedef struct synth_flanger_params {
    float rate_hz;
    float intensity;
    float depth;
    float feedback;
    float mix;
    float manual_delay_seconds;
} synth_flanger_params;

typedef struct synth_flanger_delay_line {
    float *left;
    float *right;
    size_t write_index;
    size_t capacity_frames;
} synth_flanger_delay_line;

typedef struct synth_flanger {
    float sample_rate;
    float rate_hz;
    float intensity;
    float depth;
    float feedback;
    float mix;
    float manual_delay_seconds;
    float phase;
    synth_flanger_delay_line delay;
} synth_flanger;

void synth_flanger_init(synth_flanger *flanger, float sample_rate);
void synth_flanger_uninit(synth_flanger *flanger);
void synth_flanger_set_sample_rate(synth_flanger *flanger, float sample_rate);
void synth_flanger_set_rate(synth_flanger *flanger, float hz);
void synth_flanger_set_intensity(synth_flanger *flanger, float intensity);
void synth_flanger_set_depth(synth_flanger *flanger, float depth);
void synth_flanger_set_feedback(synth_flanger *flanger, float feedback);
void synth_flanger_set_mix(synth_flanger *flanger, float mix);
void synth_flanger_set_manual(synth_flanger *flanger, float seconds);
float synth_flanger_get_rate(const synth_flanger *flanger);
float synth_flanger_get_intensity(const synth_flanger *flanger);
float synth_flanger_get_depth(const synth_flanger *flanger);
float synth_flanger_get_feedback(const synth_flanger *flanger);
float synth_flanger_get_mix(const synth_flanger *flanger);
float synth_flanger_get_manual(const synth_flanger *flanger);
synth_stereo_sample synth_flanger_process(
    synth_flanger *flanger,
    synth_stereo_sample input);

// returns a copy of stored controls; processing overrides never change those bases
synth_flanger_params synth_flanger_get_params(const synth_flanger *effect);
// effective controls must be finite and within the module bounds
// advances dsp history without storing controls or calling parameter setters
synth_stereo_sample synth_flanger_process_with_params(
    synth_flanger *effect,
    synth_stereo_sample input,
    const synth_flanger_params *params);

// resolves intensity before direct depth/feedback modulation and final clamping
// flanger intensity is a mix of depth and feedback
void synth_flanger_resolve_intensity(synth_flanger_params *params, float intensity);

#endif
