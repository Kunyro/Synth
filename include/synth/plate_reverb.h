#ifndef SYNTH_PLATE_REVERB_H
#define SYNTH_PLATE_REVERB_H

#include <stddef.h>

#include "synth/audio_types.h"

#define SYNTH_PLATE_REVERB_MIN_SAMPLE_RATE 1.0f
#define SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS 0.10f
#define SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS 10.0f
#define SYNTH_PLATE_REVERB_DEFAULT_DECAY_SECONDS 1.80f
#define SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS 0.20f
#define SYNTH_PLATE_REVERB_DEFAULT_PREDELAY_SECONDS 0.0f
#define SYNTH_PLATE_REVERB_MAX_FEEDBACK 0.97f
#define SYNTH_PLATE_REVERB_INPUT_ALLPASS_COUNT 4

// effective controls, separate from persistent dsp history
typedef struct synth_plate_reverb_params {
    float decay_seconds;
    float damping;
    float mix;
    float predelay_seconds;
} synth_plate_reverb_params;

typedef struct synth_plate_reverb_delay_line {
    float *samples;
    size_t write_index;
    size_t capacity_frames;
} synth_plate_reverb_delay_line;

typedef struct synth_plate_reverb_allpass {
    synth_plate_reverb_delay_line delay;
    float feedback;
} synth_plate_reverb_allpass;

typedef struct synth_plate_reverb_one_pole {
    float state;
} synth_plate_reverb_one_pole;

typedef struct synth_plate_reverb_tank {
    synth_plate_reverb_allpass left_diffuser_1;
    synth_plate_reverb_allpass left_diffuser_2;
    synth_plate_reverb_allpass right_diffuser_1;
    synth_plate_reverb_allpass right_diffuser_2;
    synth_plate_reverb_delay_line left_delay_1;
    synth_plate_reverb_delay_line left_delay_2;
    synth_plate_reverb_delay_line right_delay_1;
    synth_plate_reverb_delay_line right_delay_2;
    synth_plate_reverb_one_pole left_damping_filter;
    synth_plate_reverb_one_pole right_damping_filter;
    float left_feedback;
    float right_feedback;
} synth_plate_reverb_tank;

typedef struct synth_plate_reverb {
    float sample_rate;
    float decay_seconds;
    float damping;
    float mix;
    float predelay_seconds;
    float feedback;
    synth_plate_reverb_delay_line predelay;
    synth_plate_reverb_one_pole bandwidth_filter;
    synth_plate_reverb_allpass input_diffusers[SYNTH_PLATE_REVERB_INPUT_ALLPASS_COUNT];
    synth_plate_reverb_tank tank;
} synth_plate_reverb;

void synth_plate_reverb_init(synth_plate_reverb *reverb, float sample_rate);
void synth_plate_reverb_uninit(synth_plate_reverb *reverb);
void synth_plate_reverb_set_sample_rate(synth_plate_reverb *reverb, float sample_rate);
void synth_plate_reverb_set_decay(synth_plate_reverb *reverb, float seconds);
void synth_plate_reverb_set_damping(synth_plate_reverb *reverb, float damping);
void synth_plate_reverb_set_mix(synth_plate_reverb *reverb, float mix);
void synth_plate_reverb_set_predelay(synth_plate_reverb *reverb, float seconds);
float synth_plate_reverb_get_decay(const synth_plate_reverb *reverb);
float synth_plate_reverb_get_damping(const synth_plate_reverb *reverb);
float synth_plate_reverb_get_mix(const synth_plate_reverb *reverb);
float synth_plate_reverb_get_predelay(const synth_plate_reverb *reverb);
synth_stereo_sample synth_plate_reverb_process(
    synth_plate_reverb *reverb,
    synth_stereo_sample input);

// returns a copy of stored controls; processing overrides never change those bases
synth_plate_reverb_params synth_plate_reverb_get_params(const synth_plate_reverb *effect);
// effective controls must be finite and within the module bounds
// advances dsp history without storing controls or calling parameter setters
synth_stereo_sample synth_plate_reverb_process_with_params(
    synth_plate_reverb *effect,
    synth_stereo_sample input,
    const synth_plate_reverb_params *params);

#endif
