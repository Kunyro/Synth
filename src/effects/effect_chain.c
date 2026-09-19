#include "synth/effect_chain.h"

void synth_effect_chain_init(synth_effect_chain *chain, float sample_rate)
{
    synth_saturation_init(&chain->saturation, sample_rate);
    synth_distortion_init(&chain->distortion);
    synth_bitcrusher_init(&chain->bitcrusher, sample_rate);
    synth_flanger_init(&chain->flanger, sample_rate);
    synth_ring_mod_init(&chain->ring_mod, sample_rate);
    synth_chorus_init(&chain->chorus, sample_rate);
    synth_eq_init(&chain->eq, sample_rate);
    synth_delay_init(&chain->delay, sample_rate);
    synth_plate_reverb_init(&chain->plate_reverb, sample_rate);
    synth_compressor_init(&chain->compressor, sample_rate);
}

void synth_effect_chain_uninit(synth_effect_chain *chain)
{
    synth_plate_reverb_uninit(&chain->plate_reverb);
    synth_delay_uninit(&chain->delay);
    synth_chorus_uninit(&chain->chorus);
    synth_flanger_uninit(&chain->flanger);
}

// warms, clips, degrades, modulates, thickens, shapes, repeats, places, then controls level
synth_stereo_sample synth_effect_chain_process_with_params(
    synth_effect_chain *chain,
    synth_stereo_sample input,
    const synth_effect_chain_params *params)
{
    synth_stereo_sample sample = input;

    sample = synth_saturation_process_with_params(&chain->saturation, sample, &params->saturation);
    sample = synth_distortion_process_with_params(&chain->distortion, sample, &params->distortion);
    sample = synth_bitcrusher_process_with_params(&chain->bitcrusher, sample, &params->bitcrusher);
    sample = synth_flanger_process_with_params(&chain->flanger, sample, &params->flanger);
    sample = synth_ring_mod_process_with_params(&chain->ring_mod, sample, &params->ring_mod);
    sample = synth_chorus_process_with_params(&chain->chorus, sample, &params->chorus);
    sample = synth_eq_process_with_params(&chain->eq, sample, &params->eq);
    sample = synth_delay_process_with_params(&chain->delay, sample, &params->delay);
    sample = synth_plate_reverb_process_with_params(&chain->plate_reverb, sample, &params->plate_reverb);
    sample = synth_compressor_process_with_params(&chain->compressor, sample, &params->compressor);
    return sample;
}

// collects value copies of each module's stored controls, without copying dsp history
synth_effect_chain_params synth_effect_chain_get_params(const synth_effect_chain *chain)
{
    synth_effect_chain_params params;
    params.saturation = synth_saturation_get_params(&chain->saturation);
    params.distortion = synth_distortion_get_params(&chain->distortion);
    params.bitcrusher = synth_bitcrusher_get_params(&chain->bitcrusher);
    params.flanger = synth_flanger_get_params(&chain->flanger);
    params.ring_mod = synth_ring_mod_get_params(&chain->ring_mod);
    params.chorus = synth_chorus_get_params(&chain->chorus);
    params.eq = synth_eq_get_params(&chain->eq);
    params.delay = synth_delay_get_params(&chain->delay);
    params.plate_reverb = synth_plate_reverb_get_params(&chain->plate_reverb);
    params.compressor = synth_compressor_get_params(&chain->compressor);
    return params;
}

// runs the normal effect order using stored controls through the shared processing path
synth_stereo_sample synth_effect_chain_process(synth_effect_chain *chain, synth_stereo_sample input)
{
    const synth_effect_chain_params params = synth_effect_chain_get_params(chain);
    return synth_effect_chain_process_with_params(chain, input, &params);
}

int synth_effect_chain_is_ready(const synth_effect_chain *chain)
{
    return synth_delay_is_ready(&chain->delay) && synth_chorus_is_ready(&chain->chorus) &&
        synth_flanger_is_ready(&chain->flanger) && synth_plate_reverb_is_ready(&chain->plate_reverb);
}

void synth_effect_chain_reset(synth_effect_chain *chain)
{
    synth_saturation_reset(&chain->saturation);
    synth_bitcrusher_reset(&chain->bitcrusher);
    synth_flanger_reset(&chain->flanger);
    synth_chorus_reset(&chain->chorus);
    synth_eq_reset(&chain->eq);
    synth_delay_reset(&chain->delay);
    synth_plate_reverb_reset(&chain->plate_reverb);
    synth_ring_mod_reset(&chain->ring_mod);
    synth_compressor_reset(&chain->compressor);
}

int synth_effect_chain_has_tail(const synth_effect_chain *chain)
{
    return synth_saturation_has_tail(&chain->saturation) ||
        synth_bitcrusher_has_tail(&chain->bitcrusher) ||
        synth_flanger_has_tail(&chain->flanger) ||
        synth_chorus_has_tail(&chain->chorus) ||
        synth_eq_has_tail(&chain->eq) ||
        synth_delay_has_tail(&chain->delay) ||
        synth_plate_reverb_has_tail(&chain->plate_reverb);
}
