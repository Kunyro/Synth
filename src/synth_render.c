#include "synth/synth.h"

#include <math.h>

#include "internal/synth_internal.h"

#include "internal/render_parameters.h"

// packs the effective levels/spread and morph changes needed by every voice
static synth_voice_mix voice_mix(const synth *s, const synth_render_parameters *params)
{
    // voices already store their base morph, so pass only the modulation offset
    const synth_voice_mix mix = {
        params->first_oscillator_gain,
        params->second_oscillator_gain,
        params->stereo_spread,
        params->oscillator_morph - s->oscillator_morph,
        params->second_oscillator_morph - s->second_oscillator_morph
    };
    return mix;
}

// converts secondary tuning modulation into one frequency multiplier
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

// retained filter or effect memory keeps a tail alive even between delayed echoes
static int voice_has_tail(const synth_voice *voice)
{
    return synth_filter_has_tail(&voice->filter) ||
        synth_filter_has_tail(&voice->right_filter) ||
        synth_effect_chain_has_tail(&voice->effects);
}

static synth_stereo_sample render_voice(synth *s, synth_voice *voice, float lfo_value)
{
    synth_render_parameters params;
    synth_stereo_sample sample = {0, 0};
    if (!voice->active && !voice->tail_active && voice->steal_remaining == 0) return sample;
    synth_voice_advance_envelopes(voice, s->sample_rate);
    synth_resolve_render_parameters(s, lfo_value, voice->mod_envelope.level, &params);
    sample = synth_voice_render_current(voice, s->sample_rate, voice_mix(s, &params),
                                        secondary_tuning_ratio(s, &params));
    sample.left = synth_filter_process_with_params(&voice->filter, sample.left, &params.filter);
    sample.right = synth_filter_process_with_params(&voice->right_filter, sample.right, &params.filter);
    sample = synth_effect_chain_process_with_params(&voice->effects, sample, &params.effects);
    sample = apply_master_gain(sample, params.master_gain);
    voice->output_level = fmaxf(fmaxf(fabsf(sample.left), fabsf(sample.right)), voice->output_level * 0.99f);
    if (voice->steal_remaining > 0) {
        const float gain = (float)(voice->steal_remaining - 1) / (float)voice->steal_frames;
        sample.left *= gain;
        sample.right *= gain;
        if (--voice->steal_remaining == 0) synth_start_pending_voice(s, voice);
    } else if (!voice->active && ++voice->tail_check_frames >= SYNTH_TAIL_CHECK_FRAMES) {
        voice->tail_check_frames = 0;
        voice->tail_active = voice_has_tail(voice);
    }
    return sample;
}

// one global source sample, followed by complete independent voice processing
static synth_stereo_sample synth_render_stereo_sample(synth *s)
{
    synth_stereo_sample sample = {0, 0};
    if (!s->ready) return sample;
    const float lfo_value = synth_lfo_advance(&s->lfo, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        const synth_stereo_sample v = render_voice(s, &s->voices[i], lfo_value);
        sample.left += v.left;
        sample.right += v.right;
    }
    return sample;
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
