#include "synth/synth.h"

#include <math.h>
#include <string.h>

#include "internal/synth_internal.h"

// converts a semitone offset into a frequency multiplier.
static float pitch_bend_ratio(float semitones)
{
    return powf(2.0f, semitones / 12.0f);
}

// applies the current synth bend to a base frequency.
static float bend_frequency(const synth *s, float base_frequency)
{
    return base_frequency * pitch_bend_ratio(s->pitch_bend_semitones);
}

// totals the second oscillator pitch offset in semitones.
static float second_oscillator_semitones(const synth *s)
{
    return (float)(s->second_oscillator_octave * 12) +
           (float)s->second_oscillator_pitch_semitones +
           (s->second_oscillator_fine_tune_cents / 100.0f);
}

// applies the current second oscillator tuning to a primary frequency.
static float second_oscillator_frequency(const synth *s, float primary_frequency)
{
    return primary_frequency * pitch_bend_ratio(second_oscillator_semitones(s));
}

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

static int voice_is_releasable_for_note(const synth_voice *voice, int midi_note)
{
    return voice->active &&
        voice->midi_note == midi_note &&
        voice->envelope.stage != SYNTH_ENV_RELEASE;
}

// finds the held voice that should respond to a midi note-off.
static synth_voice *find_releasable_voice_for_note(synth *s, int midi_note)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice_is_releasable_for_note(voice, midi_note)) {
            return voice;
        }
    }

    return 0;
}

// finds a free voice or steals the quietest one.
static synth_voice *find_available_voice(synth *s)
{
    synth_voice *quietest = &s->voices[0];

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (!voice->active) {
            return voice;
        }

        if (voice->envelope.level < quietest->envelope.level) {
            quietest = voice;
        }
    }

    return quietest;
}

// retunes one voice without changing its original note or phase.
static void retune_voice(synth *s, synth_voice *voice)
{
    const float primary_frequency = bend_frequency(s, voice->base_frequency);
    const float secondary_frequency = second_oscillator_frequency(s, primary_frequency);

    synth_voice_set_frequencies(voice, primary_frequency, secondary_frequency);
}

// retunes active voices without changing their original note or phase.
static void retune_active_voices(synth *s)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice->active) {
            retune_voice(s, voice);
        }
    }
}

// starts a voice and reapplies the synth's current tuning and morph settings.
static void start_voice(
    synth *s,
    synth_voice *voice,
    int midi_note,
    float base_frequency,
    float velocity)
{
    synth_voice_note_on(
        voice,
        midi_note,
        base_frequency,
        velocity,
        s->waveform,
        s->envelope);
    retune_voice(s, voice);
    synth_voice_set_oscillator_morph(voice, s->oscillator_morph);
    synth_voice_set_second_oscillator_morph(voice, s->second_oscillator_morph);
}

// sets up the synth with defaults.
void synth_init(synth *s, float sample_rate)
{
    const synth_adsr default_envelope = {0.01f, 0.08f, 0.75f, 0.16f};

    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate;
    s->master_gain = SYNTH_DEFAULT_MASTER_GAIN;
    s->pitch_bend = 0.0f;
    s->pitch_bend_semitones = 0.0f;
    s->waveform = SYNTH_WAVEFORM_SINE;
    s->oscillator_morph = synth_waveform_to_morph(s->waveform);
    s->first_oscillator_gain = 1.0f;
    s->second_oscillator_gain = 0.0f;
    s->stereo_spread = 0.0f;
    s->second_oscillator_morph = synth_waveform_to_morph(SYNTH_WAVEFORM_SQUARE);
    s->second_oscillator_octave = 0;
    s->second_oscillator_pitch_semitones = 0;
    s->second_oscillator_fine_tune_cents = 0.0f;
    synth_lfo_init(&s->lfo, SYNTH_DEFAULT_LFO_RATE_HZ);
    s->lfo_depth = 0.0f;
    s->lfo_first_oscillator_morph_amount = 0.0f;
    s->lfo_second_oscillator_morph_amount = 0.0f;
    s->lfo_first_oscillator_gain_amount = 0.0f;
    s->lfo_second_oscillator_gain_amount = 0.0f;
    s->lfo_filter_amount = 0.0f;
    s->envelope = synth_sanitize_adsr(default_envelope);
    synth_filter_init(&s->filter, sample_rate, sample_rate * 0.5f);
    synth_filter_init(&s->right_filter, sample_rate, sample_rate * 0.5f);
    synth_effect_chain_init(&s->effects, sample_rate);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_init(&s->voices[i], s->envelope);
    }
}

void synth_note_on(synth *s, int midi_note, float velocity)
{
    synth_voice *voice = find_available_voice(s);
    const float base_frequency = synth_midi_note_to_frequency(midi_note);

    start_voice(s, voice, midi_note, base_frequency, velocity);
}

void synth_note_on_frequency(synth *s, float frequency, float velocity)
{
    synth_voice *voice = find_available_voice(s);

    start_voice(s, voice, -1, frequency, velocity);
}

void synth_note_off(synth *s, int midi_note)
{
    synth_voice *voice = find_releasable_voice_for_note(s, midi_note);

    if (voice != 0) {
        synth_voice_note_off(voice);
    }
}

void synth_all_notes_off(synth *s)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (s->voices[i].active) {
            synth_voice_note_off(&s->voices[i]);
        }
    }
}

void synth_set_pitch_bend(synth *s, float pitch_bend)
{
    s->pitch_bend = synth_clampf(pitch_bend, -1.0f, 1.0f);
    s->pitch_bend_semitones = s->pitch_bend * SYNTH_PITCH_BEND_MAX_SEMITONES;
    retune_active_voices(s);
}

void synth_set_master_gain(synth *s, float gain)
{
    s->master_gain = synth_clampf(gain, 0.0f, 1.0f);
}

void synth_set_adsr(synth *s, synth_adsr envelope)
{
    s->envelope = synth_sanitize_adsr(envelope);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        s->voices[i].envelope.adsr = s->envelope;
    }
}

synth_adsr synth_get_adsr(const synth *s)
{
    return s->envelope;
}

void synth_set_waveform(synth *s, synth_waveform waveform)
{
    s->waveform = waveform;
    s->oscillator_morph = synth_waveform_to_morph(waveform);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_waveform(&s->voices[i], waveform);
    }
}

void synth_set_oscillator_morph(synth *s, float morph)
{
    s->oscillator_morph = synth_clampf(morph, 0.0f, 1.0f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_oscillator_morph(&s->voices[i], s->oscillator_morph);
    }
}

void synth_set_second_oscillator_morph(synth *s, float morph)
{
    s->second_oscillator_morph = synth_clampf(morph, 0.0f, 1.0f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_second_oscillator_morph(&s->voices[i], s->second_oscillator_morph);
    }
}

void synth_set_first_oscillator_gain(synth *s, float gain)
{
    s->first_oscillator_gain = synth_clampf(gain, 0.0f, 1.0f);
}

void synth_set_second_oscillator_gain(synth *s, float gain)
{
    s->second_oscillator_gain = synth_clampf(gain, 0.0f, 1.0f);
}

void synth_set_stereo_spread(synth *s, float spread)
{
    s->stereo_spread = synth_clampf(spread, 0.0f, 1.0f);
}

void synth_set_second_oscillator_octave(synth *s, int octave)
{
    s->second_oscillator_octave = synth_clampi(octave, -1, 1);
    retune_active_voices(s);
}

void synth_set_second_oscillator_pitch(synth *s, int semitones)
{
    s->second_oscillator_pitch_semitones = synth_clampi(semitones, -6, 6);
    retune_active_voices(s);
}

void synth_set_second_oscillator_fine_tune(synth *s, float cents)
{
    s->second_oscillator_fine_tune_cents = synth_clampf(cents, -50.0f, 50.0f);
    retune_active_voices(s);
}

void synth_set_filter_cutoff(synth *s, float cutoff_hz)
{
    synth_filter_set_cutoff(&s->filter, cutoff_hz);
    synth_filter_set_cutoff(&s->right_filter, cutoff_hz);
}

void synth_set_filter_poles(synth *s, int pole_count)
{
    synth_filter_set_poles(&s->filter, pole_count);
    synth_filter_set_poles(&s->right_filter, pole_count);
}

void synth_set_lfo_rate(synth *s, float frequency_hz)
{
    synth_lfo_set_frequency(&s->lfo, frequency_hz);
}

void synth_set_lfo_shape_morph(synth *s, float morph)
{
    synth_lfo_set_morph(&s->lfo, morph);
}

void synth_set_lfo_depth(synth *s, float depth)
{
    s->lfo_depth = synth_clampf(depth, 0.0f, 1.0f);
}

void synth_set_lfo_first_oscillator_morph_amount(synth *s, float amount)
{
    s->lfo_first_oscillator_morph_amount = synth_clampf(amount, 0.0f, 1.0f);
}

void synth_set_lfo_second_oscillator_morph_amount(synth *s, float amount)
{
    s->lfo_second_oscillator_morph_amount = synth_clampf(amount, 0.0f, 1.0f);
}

void synth_set_lfo_first_oscillator_gain_amount(synth *s, float amount)
{
    s->lfo_first_oscillator_gain_amount = synth_clampf(amount, 0.0f, 1.0f);
}

void synth_set_lfo_second_oscillator_gain_amount(synth *s, float amount)
{
    s->lfo_second_oscillator_gain_amount = synth_clampf(amount, 0.0f, 1.0f);
}

void synth_set_lfo_filter_amount(synth *s, float amount)
{
    s->lfo_filter_amount = synth_clampf(amount, 0.0f, 1.0f);
}

void synth_set_saturation_drive(synth *s, float drive)
{
    synth_saturation_set_drive(&s->effects.saturation, drive);
}

void synth_set_saturation_mix(synth *s, float mix)
{
    synth_saturation_set_mix(&s->effects.saturation, mix);
}

void synth_set_distortion_drive(synth *s, float drive)
{
    synth_distortion_set_drive(&s->effects.distortion, drive);
}

void synth_set_distortion_mix(synth *s, float mix)
{
    synth_distortion_set_mix(&s->effects.distortion, mix);
}

void synth_set_bitcrusher_sample_rate(synth *s, float sample_rate)
{
    synth_bitcrusher_set_sample_rate(&s->effects.bitcrusher, sample_rate);
}

void synth_set_bitcrusher_bits(synth *s, int bits)
{
    synth_bitcrusher_set_bits(&s->effects.bitcrusher, bits);
}

void synth_set_bitcrusher_mix(synth *s, float mix)
{
    synth_bitcrusher_set_mix(&s->effects.bitcrusher, mix);
}

void synth_set_delay_time(synth *s, float seconds)
{
    synth_delay_set_time(&s->effects.delay, seconds);
}

void synth_set_delay_feedback(synth *s, float feedback)
{
    synth_delay_set_feedback(&s->effects.delay, feedback);
}

void synth_set_delay_mix(synth *s, float mix)
{
    synth_delay_set_mix(&s->effects.delay, mix);
}

float synth_get_master_gain(const synth *s)
{
    return s->master_gain;
}

float synth_get_oscillator_morph(const synth *s)
{
    return s->oscillator_morph;
}

float synth_get_first_oscillator_gain(const synth *s)
{
    return s->first_oscillator_gain;
}

float synth_get_second_oscillator_gain(const synth *s)
{
    return s->second_oscillator_gain;
}

float synth_get_stereo_spread(const synth *s)
{
    return s->stereo_spread;
}

float synth_get_second_oscillator_morph(const synth *s)
{
    return s->second_oscillator_morph;
}

int synth_get_second_oscillator_octave(const synth *s)
{
    return s->second_oscillator_octave;
}

int synth_get_second_oscillator_pitch(const synth *s)
{
    return s->second_oscillator_pitch_semitones;
}

float synth_get_second_oscillator_fine_tune(const synth *s)
{
    return s->second_oscillator_fine_tune_cents;
}

float synth_get_filter_cutoff(const synth *s)
{
    return s->filter.cutoff_hz;
}

int synth_get_filter_poles(const synth *s)
{
    return s->filter.pole_count;
}

float synth_get_lfo_rate(const synth *s)
{
    return s->lfo.frequency_hz;
}

float synth_get_lfo_shape_morph(const synth *s)
{
    return s->lfo.morph;
}

float synth_get_lfo_depth(const synth *s)
{
    return s->lfo_depth;
}

float synth_get_lfo_first_oscillator_morph_amount(const synth *s)
{
    return s->lfo_first_oscillator_morph_amount;
}

float synth_get_lfo_second_oscillator_morph_amount(const synth *s)
{
    return s->lfo_second_oscillator_morph_amount;
}

float synth_get_lfo_first_oscillator_gain_amount(const synth *s)
{
    return s->lfo_first_oscillator_gain_amount;
}

float synth_get_lfo_second_oscillator_gain_amount(const synth *s)
{
    return s->lfo_second_oscillator_gain_amount;
}

float synth_get_lfo_filter_amount(const synth *s)
{
    return s->lfo_filter_amount;
}

float synth_get_saturation_drive(const synth *s)
{
    return synth_saturation_get_drive(&s->effects.saturation);
}

float synth_get_saturation_mix(const synth *s)
{
    return synth_saturation_get_mix(&s->effects.saturation);
}

float synth_get_distortion_drive(const synth *s)
{
    return synth_distortion_get_drive(&s->effects.distortion);
}

float synth_get_distortion_mix(const synth *s)
{
    return synth_distortion_get_mix(&s->effects.distortion);
}

float synth_get_bitcrusher_sample_rate(const synth *s)
{
    return synth_bitcrusher_get_sample_rate(&s->effects.bitcrusher);
}

int synth_get_bitcrusher_bits(const synth *s)
{
    return synth_bitcrusher_get_bits(&s->effects.bitcrusher);
}

float synth_get_bitcrusher_mix(const synth *s)
{
    return synth_bitcrusher_get_mix(&s->effects.bitcrusher);
}

float synth_get_delay_time(const synth *s)
{
    return synth_delay_get_time(&s->effects.delay);
}

float synth_get_delay_feedback(const synth *s)
{
    return synth_delay_get_feedback(&s->effects.delay);
}

float synth_get_delay_mix(const synth *s)
{
    return synth_delay_get_mix(&s->effects.delay);
}

static synth_stereo_sample apply_master_gain(synth_stereo_sample sample, float master_gain)
{
    sample.left *= master_gain;
    sample.right *= master_gain;
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
