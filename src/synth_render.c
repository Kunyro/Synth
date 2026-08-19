#include "synth/synth.h"

#include <math.h>

#include "internal/synth_internal.h"

// combines a route amount with the global lfo depth.
static float lfo_route_depth(const synth *s, float amount)
{
    return s->lfo_depth * amount;
}

// turns a bipolar lfo into tremolo that moves from the base gain downwards.
static float modulated_gain(float base_gain, float lfo_value, float depth)
{
    const float unipolar_lfo = (lfo_value + 1.0f) * 0.5f;

    return base_gain * (1.0f - (unipolar_lfo * depth));
}

// packages base settings and the current lfo value for voice rendering.
static synth_voice_mix synth_voice_mix_from_state(const synth *s, float lfo_value)
{
    synth_voice_mix mix;
    const float first_gain_depth =
        lfo_route_depth(s, s->lfo_first_oscillator_gain_amount);
    const float second_gain_depth =
        lfo_route_depth(s, s->lfo_second_oscillator_gain_amount);
    const float first_morph_depth =
        lfo_route_depth(s, s->lfo_first_oscillator_morph_amount);
    const float second_morph_depth =
        lfo_route_depth(s, s->lfo_second_oscillator_morph_amount);

    mix.first_oscillator_gain =
        modulated_gain(s->first_oscillator_gain, lfo_value, first_gain_depth);
    mix.second_oscillator_gain =
        modulated_gain(s->second_oscillator_gain, lfo_value, second_gain_depth);
    mix.stereo_spread = s->stereo_spread;
    mix.first_oscillator_morph_offset = lfo_value * first_morph_depth * 0.5f;
    mix.second_oscillator_morph_offset = lfo_value * second_morph_depth * 0.5f;
    return mix;
}

// moves the base filter cutoff exponentially so both directions cover octaves.
static float modulated_filter_cutoff(const synth *s, float lfo_value)
{
    const float depth_octaves =
        lfo_route_depth(s, s->lfo_filter_amount) * SYNTH_LFO_FILTER_MAX_OCTAVES;

    return s->filter.cutoff_hz * powf(2.0f, lfo_value * depth_octaves);
}

static synth_stereo_sample apply_master_gain(synth_stereo_sample sample, float master_gain)
{
    const float output_gain = master_gain * SYNTH_MASTER_GAIN_FULL_SCALE;

    sample.left *= output_gain;
    sample.right *= output_gain;
    return sample;
}

// renders one mixed stereo sample through independent channel filter state.
static synth_stereo_sample synth_render_stereo_sample(synth *s)
{
    const float lfo_value = synth_lfo_advance(&s->lfo, s->sample_rate);
    const synth_voice_mix mix = synth_voice_mix_from_state(s, lfo_value);
    const float filter_cutoff = modulated_filter_cutoff(s, lfo_value);
    synth_stereo_sample sample = {0.0f, 0.0f};

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        const synth_stereo_sample voice_sample =
            synth_voice_render_stereo_mix(&s->voices[i], s->sample_rate, mix);

        sample.left += voice_sample.left;
        sample.right += voice_sample.right;
    }

    sample.left = synth_filter_process_with_cutoff(
        &s->filter,
        sample.left,
        filter_cutoff);
    sample.right = synth_filter_process_with_cutoff(
        &s->right_filter,
        sample.right,
        filter_cutoff);
    sample = synth_effect_chain_process(&s->effects, sample);
    return apply_master_gain(sample, s->master_gain);
}

void synth_render_stereo(synth *s, synth_audio_buffer *output)
{
    for (size_t frame = 0; frame < output->frame_count; ++frame) {
        const synth_stereo_sample sample = synth_render_stereo_sample(s);

        output->left[frame] = sample.left;
        output->right[frame] = sample.right;
    }
}

void synth_render_mono(synth *s, float *output, size_t frame_count)
{
    for (size_t frame = 0; frame < frame_count; ++frame) {
        const synth_stereo_sample sample = synth_render_stereo_sample(s);

        output[frame] = (sample.left + sample.right) * 0.5f;
    }
}
