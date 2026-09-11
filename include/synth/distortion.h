#ifndef SYNTH_DISTORTION_H
#define SYNTH_DISTORTION_H

#include "synth/audio_types.h"

#define SYNTH_DISTORTION_MIN_DRIVE 0.0f
#define SYNTH_DISTORTION_MAX_DRIVE 32.0f
#define SYNTH_DISTORTION_DEFAULT_DRIVE SYNTH_DISTORTION_MIN_DRIVE

// effective controls, separate from persistent dsp history
typedef struct synth_distortion_params {
    float drive;
    float mix;
} synth_distortion_params;

typedef struct synth_distortion {
    float drive;
    float mix;
} synth_distortion;

void synth_distortion_init(synth_distortion *distortion);
void synth_distortion_set_drive(synth_distortion *distortion, float drive);
void synth_distortion_set_mix(synth_distortion *distortion, float mix);
float synth_distortion_get_drive(const synth_distortion *distortion);
float synth_distortion_get_mix(const synth_distortion *distortion);
synth_stereo_sample synth_distortion_process(
    const synth_distortion *distortion,
    synth_stereo_sample input);

// returns a copy of stored controls; processing overrides never change those bases
synth_distortion_params synth_distortion_get_params(const synth_distortion *effect);
// effective controls must be finite and within the module bounds
// advances dsp history without storing controls or calling parameter setters
synth_stereo_sample synth_distortion_process_with_params(
    const synth_distortion *effect,
    synth_stereo_sample input,
    const synth_distortion_params *params);

#endif
