#ifndef SYNTH_SATURATION_H
#define SYNTH_SATURATION_H

#include "synth/audio_types.h"

#define SYNTH_SATURATION_MIN_DRIVE 0.0f
#define SYNTH_SATURATION_MAX_DRIVE 24.0f
#define SYNTH_SATURATION_DEFAULT_DRIVE SYNTH_SATURATION_MIN_DRIVE
#define SYNTH_SATURATION_MIN_SAMPLE_RATE 1.0f
#define SYNTH_SATURATION_DC_BLOCK_CUTOFF_HZ 20.0f

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

#endif
