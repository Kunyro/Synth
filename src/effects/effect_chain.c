#include "synth/effect_chain.h"

void synth_effect_chain_init(synth_effect_chain *chain, float sample_rate)
{
    synth_saturation_init(&chain->saturation, sample_rate);
    synth_distortion_init(&chain->distortion);
    synth_bitcrusher_init(&chain->bitcrusher, sample_rate);
    synth_flanger_init(&chain->flanger, sample_rate);
    synth_ring_mod_init(&chain->ring_mod, sample_rate);
    synth_eq_init(&chain->eq, sample_rate);
    synth_delay_init(&chain->delay, sample_rate);
    synth_plate_reverb_init(&chain->plate_reverb, sample_rate);
}

void synth_effect_chain_uninit(synth_effect_chain *chain)
{
    synth_plate_reverb_uninit(&chain->plate_reverb);
    synth_delay_uninit(&chain->delay);
    synth_flanger_uninit(&chain->flanger);
}

// warms, clips, degrades, modulates, shapes, repeats, then places the tone.
synth_stereo_sample synth_effect_chain_process(
    synth_effect_chain *chain,
    synth_stereo_sample input)
{
    synth_stereo_sample sample = input;

    sample = synth_saturation_process(&chain->saturation, sample);
    sample = synth_distortion_process(&chain->distortion, sample);
    sample = synth_bitcrusher_process(&chain->bitcrusher, sample);
    sample = synth_flanger_process(&chain->flanger, sample);
    sample = synth_ring_mod_process(&chain->ring_mod, sample);
    sample = synth_eq_process(&chain->eq, sample);
    sample = synth_delay_process(&chain->delay, sample);
    sample = synth_plate_reverb_process(&chain->plate_reverb, sample);
    return sample;
}
