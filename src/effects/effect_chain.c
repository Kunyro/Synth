#include "synth/effect_chain.h"

void synth_effect_chain_init(synth_effect_chain *chain, float sample_rate)
{
    synth_distortion_init(&chain->distortion);
    synth_delay_init(&chain->delay, sample_rate);
}

synth_stereo_sample synth_effect_chain_process(
    synth_effect_chain *chain,
    synth_stereo_sample input)
{
    synth_stereo_sample sample = input;

    sample = synth_distortion_process(&chain->distortion, sample);
    sample = synth_delay_process(&chain->delay, sample);
    return sample;
}
