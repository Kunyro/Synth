#ifndef SYNTH_SATURATION_H
#define SYNTH_SATURATION_H

#include "synth/audio_types.h"

#define SYNTH_SATURATION_MIN_DRIVE 0.0f
#define SYNTH_SATURATION_MAX_DRIVE 24.0f
#define SYNTH_SATURATION_DEFAULT_DRIVE SYNTH_SATURATION_MIN_DRIVE
#define SYNTH_SATURATION_MIN_SAMPLE_RATE 1.0f
#define SYNTH_SATURATION_DC_BLOCK_CUTOFF_HZ 20.0f

// effective controls, separate from persistent dsp history
typedef struct synth_saturation_params {
    float drive;
    float mix;
} synth_saturation_params;

typedef struct synth_saturation_channel {
    float previous_input;
    float previous_output;
} synth_saturation_channel;

typedef struct synth_saturation {
    float sample_rate;
    float dc_block_coefficient;
    float drive;
    float mix;
    synth_saturation_channel left;
    synth_saturation_channel right;
} synth_saturation;

void synth_saturation_init(synth_saturation *saturation, float sample_rate);
void synth_saturation_set_sample_rate(synth_saturation *saturation, float sample_rate);
void synth_saturation_set_drive(synth_saturation *saturation, float drive);
void synth_saturation_set_mix(synth_saturation *saturation, float mix);
float synth_saturation_get_drive(const synth_saturation *saturation);
float synth_saturation_get_mix(const synth_saturation *saturation);
synth_stereo_sample synth_saturation_process(
    synth_saturation *saturation,
    synth_stereo_sample input);

// returns a copy of stored controls; processing overrides never change those bases
synth_saturation_params synth_saturation_get_params(const synth_saturation *effect);
// effective controls must be finite and within the module bounds
// advances dsp history without storing controls or calling parameter setters
synth_stereo_sample synth_saturation_process_with_params(
    synth_saturation *effect,
    synth_stereo_sample input,
    const synth_saturation_params *params);

#endif
