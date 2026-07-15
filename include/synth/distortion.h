#ifndef SYNTH_DISTORTION_H
#define SYNTH_DISTORTION_H

#include "synth/audio_types.h"

#define SYNTH_DISTORTION_MIN_DRIVE 1.0f
#define SYNTH_DISTORTION_MAX_DRIVE 32.0f

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

#endif
