#include "synth/synth.h"

#include <math.h>
#include <string.h>

#include "internal/synth_internal.h"
#include "internal/render_parameters.h"

// converts a semitone offset into a frequency multiplier
static float pitch_bend_ratio(float semitones)
{
    return powf(2.0f, semitones / 12.0f);
}

// applies the current synth bend to a base frequency
static float bend_frequency(const synth *s, float base_frequency)
{
    return base_frequency * pitch_bend_ratio(s->pitch_bend_semitones);
}

// totals the second oscillator pitch offset in semitones
static float second_oscillator_semitones(const synth *s)
{
    return (float)(s->second_oscillator_octave * 12) +
           (float)s->second_oscillator_pitch_semitones +
           (s->second_oscillator_fine_tune_cents / 100.0f);
}

// applies the current second oscillator tuning to a primary frequency
static float second_oscillator_frequency(const synth *s, float primary_frequency)
{
    return primary_frequency * pitch_bend_ratio(second_oscillator_semitones(s));
}

// matches a held voice for this note; a voice already fading out is not released again
static int voice_is_releasable_for_note(const synth_voice *voice, int note_number)
{
    return voice->active &&
        voice->note_number == note_number &&
        voice->envelope.stage != SYNTH_ENV_RELEASE;
}

// finds the held voice that should respond to a numbered note-off
static synth_voice *find_releasable_voice_for_note(synth *s, int note_number)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice_is_releasable_for_note(voice, note_number)) {
            return voice;
        }
    }

    return 0;
}

// finds a free voice or steals the quietest one
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

// retunes one voice without changing its original note or phase
static void retune_voice(synth *s, synth_voice *voice)
{
    const float primary_frequency = bend_frequency(s, voice->base_frequency);
    const float secondary_frequency = second_oscillator_frequency(s, primary_frequency);

    synth_voice_set_frequencies(voice, primary_frequency, secondary_frequency);
}

// retunes active voices without changing their original note or phase
static void retune_active_voices(synth *s)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice->active) {
            retune_voice(s, voice);
        }
    }
}

// starts a voice and reapplies the synth's current tuning and morph settings
static void start_voice(
    synth *s,
    synth_voice *voice,
    int note_number,
    float base_frequency,
    float velocity)
{
    synth_voice_note_on(
        voice,
        note_number,
        base_frequency,
        velocity,
        s->waveform,
        synth_capture_modulated_adsr(s));
    retune_voice(s, voice);
    synth_voice_set_oscillator_morph(voice, s->oscillator_morph);
    synth_voice_set_second_oscillator_morph(voice, s->second_oscillator_morph);
}

// sets up the synth with defaults
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
    s->envelope = synth_sanitize_adsr(default_envelope);
    synth_filter_init(&s->filter, sample_rate, sample_rate * 0.5f);
    synth_filter_init(&s->right_filter, sample_rate, sample_rate * 0.5f);
    synth_effect_chain_init(&s->effects, sample_rate);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_init(&s->voices[i], s->envelope);
    }
}

void synth_uninit(synth *s)
{
    if (s == 0) {
        return;
    }

    synth_effect_chain_uninit(&s->effects);
}

// converts a musical note number to pitch and starts the next available voice
void synth_note_on(synth *s, int note_number, float velocity)
{
    synth_voice *voice = find_available_voice(s);
    const float base_frequency = synth_note_to_frequency(note_number);

    start_voice(s, voice, note_number, base_frequency, velocity);
}

void synth_note_on_frequency(synth *s, float frequency, float velocity)
{
    synth_voice *voice = find_available_voice(s);

    start_voice(s, voice, -1, frequency, velocity);
}

// releases one held instance of a note, allowing repeated note-ons to be released separately
void synth_note_off(synth *s, int note_number)
{
    synth_voice *voice = find_releasable_voice_for_note(s, note_number);

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

void synth_set_flanger_rate(synth *s, float hz)
{
    synth_flanger_set_rate(&s->effects.flanger, hz);
}

void synth_set_flanger_intensity(synth *s, float intensity)
{
    synth_flanger_set_intensity(&s->effects.flanger, intensity);
}

void synth_set_flanger_depth(synth *s, float depth)
{
    synth_flanger_set_depth(&s->effects.flanger, depth);
}

void synth_set_flanger_feedback(synth *s, float feedback)
{
    synth_flanger_set_feedback(&s->effects.flanger, feedback);
}

void synth_set_flanger_mix(synth *s, float mix)
{
    synth_flanger_set_mix(&s->effects.flanger, mix);
}

void synth_set_flanger_manual(synth *s, float seconds)
{
    synth_flanger_set_manual(&s->effects.flanger, seconds);
}

void synth_set_ring_mod_frequency(synth *s, float hz)
{
    synth_ring_mod_set_frequency(&s->effects.ring_mod, hz);
}

void synth_set_ring_mod_rectify(synth *s, float rectify)
{
    synth_ring_mod_set_rectify(&s->effects.ring_mod, rectify);
}

void synth_set_ring_mod_mix(synth *s, float mix)
{
    synth_ring_mod_set_mix(&s->effects.ring_mod, mix);
}

void synth_set_chorus_rate(synth *s, float hz)
{
    synth_chorus_set_rate(&s->effects.chorus, hz);
}

void synth_set_chorus_depth(synth *s, float depth)
{
    synth_chorus_set_depth(&s->effects.chorus, depth);
}

void synth_set_chorus_mix(synth *s, float mix)
{
    synth_chorus_set_mix(&s->effects.chorus, mix);
}

void synth_set_chorus_width(synth *s, float width)
{
    synth_chorus_set_width(&s->effects.chorus, width);
}

void synth_set_chorus_delay(synth *s, float seconds)
{
    synth_chorus_set_delay(&s->effects.chorus, seconds);
}

void synth_set_chorus_feedback(synth *s, float feedback)
{
    synth_chorus_set_feedback(&s->effects.chorus, feedback);
}

void synth_set_eq_low(synth *s, float gain_db)
{
    synth_eq_set_low(&s->effects.eq, gain_db);
}

void synth_set_eq_mid(synth *s, float gain_db)
{
    synth_eq_set_mid(&s->effects.eq, gain_db);
}

void synth_set_eq_high(synth *s, float gain_db)
{
    synth_eq_set_high(&s->effects.eq, gain_db);
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

void synth_set_plate_reverb_decay(synth *s, float seconds)
{
    synth_plate_reverb_set_decay(&s->effects.plate_reverb, seconds);
}

void synth_set_plate_reverb_damping(synth *s, float damping)
{
    synth_plate_reverb_set_damping(&s->effects.plate_reverb, damping);
}

void synth_set_plate_reverb_mix(synth *s, float mix)
{
    synth_plate_reverb_set_mix(&s->effects.plate_reverb, mix);
}

void synth_set_plate_reverb_predelay(synth *s, float seconds)
{
    synth_plate_reverb_set_predelay(&s->effects.plate_reverb, seconds);
}

void synth_set_compressor_threshold(synth *s, float threshold_db)
{
    synth_compressor_set_threshold(&s->effects.compressor, threshold_db);
}

void synth_set_compressor_ratio(synth *s, float ratio)
{
    synth_compressor_set_ratio(&s->effects.compressor, ratio);
}

void synth_set_compressor_makeup_gain(synth *s, float makeup_gain_db)
{
    synth_compressor_set_makeup_gain(&s->effects.compressor, makeup_gain_db);
}

void synth_set_compressor_attack_seconds(synth *s, float seconds)
{
    synth_compressor_set_attack(&s->effects.compressor, seconds);
}

void synth_set_compressor_release_seconds(synth *s, float seconds)
{
    synth_compressor_set_release(&s->effects.compressor, seconds);
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

float synth_get_flanger_rate(const synth *s)
{
    return synth_flanger_get_rate(&s->effects.flanger);
}

float synth_get_flanger_intensity(const synth *s)
{
    return synth_flanger_get_intensity(&s->effects.flanger);
}

float synth_get_flanger_depth(const synth *s)
{
    return synth_flanger_get_depth(&s->effects.flanger);
}

float synth_get_flanger_feedback(const synth *s)
{
    return synth_flanger_get_feedback(&s->effects.flanger);
}

float synth_get_flanger_mix(const synth *s)
{
    return synth_flanger_get_mix(&s->effects.flanger);
}

float synth_get_flanger_manual(const synth *s)
{
    return synth_flanger_get_manual(&s->effects.flanger);
}

float synth_get_ring_mod_frequency(const synth *s)
{
    return synth_ring_mod_get_frequency(&s->effects.ring_mod);
}

float synth_get_ring_mod_rectify(const synth *s)
{
    return synth_ring_mod_get_rectify(&s->effects.ring_mod);
}

float synth_get_ring_mod_mix(const synth *s)
{
    return synth_ring_mod_get_mix(&s->effects.ring_mod);
}

float synth_get_chorus_rate(const synth *s)
{
    return synth_chorus_get_rate(&s->effects.chorus);
}

float synth_get_chorus_depth(const synth *s)
{
    return synth_chorus_get_depth(&s->effects.chorus);
}

float synth_get_chorus_mix(const synth *s)
{
    return synth_chorus_get_mix(&s->effects.chorus);
}

float synth_get_chorus_width(const synth *s)
{
    return synth_chorus_get_width(&s->effects.chorus);
}

float synth_get_chorus_delay(const synth *s)
{
    return synth_chorus_get_delay(&s->effects.chorus);
}

float synth_get_chorus_feedback(const synth *s)
{
    return synth_chorus_get_feedback(&s->effects.chorus);
}

float synth_get_eq_low(const synth *s)
{
    return synth_eq_get_low(&s->effects.eq);
}

float synth_get_eq_mid(const synth *s)
{
    return synth_eq_get_mid(&s->effects.eq);
}

float synth_get_eq_high(const synth *s)
{
    return synth_eq_get_high(&s->effects.eq);
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

float synth_get_plate_reverb_decay(const synth *s)
{
    return synth_plate_reverb_get_decay(&s->effects.plate_reverb);
}

float synth_get_plate_reverb_damping(const synth *s)
{
    return synth_plate_reverb_get_damping(&s->effects.plate_reverb);
}

float synth_get_plate_reverb_mix(const synth *s)
{
    return synth_plate_reverb_get_mix(&s->effects.plate_reverb);
}

float synth_get_plate_reverb_predelay(const synth *s)
{
    return synth_plate_reverb_get_predelay(&s->effects.plate_reverb);
}

float synth_get_compressor_threshold(const synth *s)
{
    return synth_compressor_get_threshold(&s->effects.compressor);
}

float synth_get_compressor_ratio(const synth *s)
{
    return synth_compressor_get_ratio(&s->effects.compressor);
}

float synth_get_compressor_makeup_gain(const synth *s)
{
    return synth_compressor_get_makeup_gain(&s->effects.compressor);
}

float synth_get_compressor_attack_seconds(const synth *s)
{
    return synth_compressor_get_attack(&s->effects.compressor);
}

float synth_get_compressor_release_seconds(const synth *s)
{
    return synth_compressor_get_release(&s->effects.compressor);
}
