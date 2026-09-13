#ifndef SYNTH_EFFECT_CHAIN_H
#define SYNTH_EFFECT_CHAIN_H

#include "synth/audio_types.h"
#include "synth/bitcrusher.h"
#include "synth/chorus.h"
#include "synth/compressor.h"
#include "synth/delay.h"
#include "synth/distortion.h"
#include "synth/eq.h"
#include "synth/flanger.h"
#include "synth/plate_reverb.h"
#include "synth/ring_mod.h"
#include "synth/saturation.h"

// groups each effect's temporary controls so the chain passes only the relevant
// subset to each module audio buffers and processing history live in the chain below
typedef struct synth_effect_chain_params {
    synth_saturation_params saturation;
    synth_distortion_params distortion;
    synth_bitcrusher_params bitcrusher;
    synth_flanger_params flanger;
    synth_ring_mod_params ring_mod;
    synth_chorus_params chorus;
    synth_eq_params eq;
    synth_delay_params delay;
    synth_plate_reverb_params plate_reverb;
    synth_compressor_params compressor;
} synth_effect_chain_params;

typedef struct synth_effect_chain {
    synth_saturation saturation;
    synth_distortion distortion;
    synth_bitcrusher bitcrusher;
    synth_flanger flanger;
    synth_ring_mod ring_mod;
    synth_chorus chorus;
    synth_eq eq;
    synth_delay delay;
    synth_plate_reverb plate_reverb;
    synth_compressor compressor;
} synth_effect_chain;

void synth_effect_chain_init(synth_effect_chain *chain, float sample_rate);
void synth_effect_chain_uninit(synth_effect_chain *chain);
synth_stereo_sample synth_effect_chain_process(
    synth_effect_chain *chain,
    synth_stereo_sample input);

// copies all stored effect controls for callers that do not need modulation overrides
synth_effect_chain_params synth_effect_chain_get_params(const synth_effect_chain *chain);
// processes one stereo frame in the normal effect order with supplied controls
synth_stereo_sample synth_effect_chain_process_with_params(
    synth_effect_chain *chain, synth_stereo_sample input,
    const synth_effect_chain_params *params);

#endif
