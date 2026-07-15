#include "synth/effect_chain.h"

void synth_effect_chain_init(synth_effect_chain *chain)
{
    synth_distortion_init(&chain->distortion);
}

synth_stereo_sample synth_effect_chain_process(
    synth_effect_chain *chain,
    synth_stereo_sample input)
{
    synth_stereo_sample sample = input;

    sample = synth_distortion_process(&chain->distortion, sample);
    return sample;
}
