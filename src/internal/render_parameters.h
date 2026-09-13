#ifndef SYNTH_RENDER_PARAMETERS_H
#define SYNTH_RENDER_PARAMETERS_H

#include "synth/synth.h"

// a frame of effective controls no oscillator, filter, or delay history lives here
typedef struct synth_render_parameters {
    float master_gain;
    float oscillator_morph;
    float first_oscillator_gain;
    float second_oscillator_gain;
    float second_oscillator_morph;
    int second_oscillator_octave;
    int second_oscillator_pitch;
    float second_oscillator_fine_tune;
    float stereo_spread;
    synth_filter_params filter;
    synth_effect_chain_params effects;
} synth_render_parameters;

// fills a fresh frame from stored settings and one shared lfo sample
void synth_resolve_render_parameters(const synth *s, float lfo_value,
                                     synth_render_parameters *params);
// captures a new voice's envelope at the current phase without moving the source
synth_adsr synth_capture_modulated_adsr(const synth *s);

#endif
