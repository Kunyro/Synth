#ifndef SYNTH_EFFECT_CHAIN_H
#define SYNTH_EFFECT_CHAIN_H

#include "synth/audio_types.h"
#include "synth/delay.h"
#include "synth/distortion.h"

typedef struct synth_effect_chain {
    synth_distortion distortion;
    synth_delay delay;
} synth_effect_chain;

void synth_effect_chain_init(synth_effect_chain *chain, float sample_rate);
synth_stereo_sample synth_effect_chain_process(
    synth_effect_chain *chain,
    synth_stereo_sample input);

#endif
