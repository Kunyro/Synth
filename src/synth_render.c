#include "synth/synth.h"

#include <math.h>

#include "internal/synth_internal.h"

#include "internal/render_parameters.h"

// packs the effective levels/spread and morph changes needed by every voice
static synth_voice_mix voice_mix(const synth *s, const synth_render_parameters *params)
{
    // voices already store their base morph, so pass only the lfo's change to it
    const synth_voice_mix mix = {
        params->first_oscillator_gain,
        params->second_oscillator_gain,
        params->stereo_spread,
        params->oscillator_morph - s->oscillator_morph,
        params->second_oscillator_morph - s->second_oscillator_morph
    };
    return mix;
}

// converts the lfo's secondary tuning changes into one frequency multiplier
static float secondary_tuning_ratio(const synth *s, const synth_render_parameters *params)
{
    // there are 12 semitones per octave and 100 cents per semitone subtract the
    // bases because the voice frequency already includes manual tuning and bend
    const float semitones =
        12.0f * (float)(params->second_oscillator_octave - s->second_oscillator_octave) +
        (float)(params->second_oscillator_pitch - s->second_oscillator_pitch_semitones) +
        (params->second_oscillator_fine_tune - s->second_oscillator_fine_tune_cents) / 100.0f;
    // twelve extra semitones double frequency zero must leave it exactly unchanged
    return semitones == 0.0f ? 1.0f : exp2f(semitones / 12.0f);
}

// applies the output level after all effects, using the engine's headroom scale
static synth_stereo_sample apply_master_gain(synth_stereo_sample sample, float master_gain)
{
    const float output_gain = master_gain * SYNTH_MASTER_GAIN_FULL_SCALE;

    sample.left *= output_gain;
    sample.right *= output_gain;
    return sample;
}

// renders one mixed stereo sample through independent channel filter state
static synth_stereo_sample synth_render_stereo_sample(synth *s)
{
    // one source sample is shared by all voices, destinations, and both channels
    // advance even during silence so new notes do not restart the modulation
    const float lfo_value = synth_lfo_advance(&s->lfo, s->sample_rate);
    synth_render_parameters params;
    synth_voice_mix mix;
    float tuning_ratio;
    synth_stereo_sample sample = {0.0f, 0.0f};

    // rebuild from current bases each frame so offsets never accumulate or drift
    synth_resolve_render_parameters(s, lfo_value, &params);
    mix = voice_mix(s, &params);
    tuning_ratio = secondary_tuning_ratio(s, &params);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        const synth_stereo_sample voice_sample =
            synth_voice_render_with_params(&s->voices[i], s->sample_rate, mix, tuning_ratio);

        sample.left += voice_sample.left;
        sample.right += voice_sample.right;
    }

    sample.left = synth_filter_process_with_params(
        &s->filter,
        sample.left,
        &params.filter);
    sample.right = synth_filter_process_with_params(
        &s->right_filter,
        sample.right,
        &params.filter);
    sample = synth_effect_chain_process_with_params(&s->effects, sample, &params.effects);
    return apply_master_gain(sample, params.master_gain);
}

// fills separate left/right buffers using the same per-frame processing path
void synth_render_stereo(synth *s, synth_audio_buffer *output)
{
    for (size_t frame = 0; frame < output->frame_count; ++frame) {
        const synth_stereo_sample sample = synth_render_stereo_sample(s);

        output->left[frame] = sample.left;
        output->right[frame] = sample.right;
    }
}

// averages the stereo path, keeping effect behavior and lfo timing identical in mono
void synth_render_mono(synth *s, float *output, size_t frame_count)
{
    for (size_t frame = 0; frame < frame_count; ++frame) {
        const synth_stereo_sample sample = synth_render_stereo_sample(s);

        output[frame] = (sample.left + sample.right) * 0.5f;
    }
}
