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

// packages the current oscillator mix settings for voice rendering.
static synth_voice_mix synth_voice_mix_from_state(const synth *s)
{
    synth_voice_mix mix;

    mix.first_oscillator_gain = s->first_oscillator_gain;
    mix.second_oscillator_gain = s->second_oscillator_gain;
    return mix;
}

// finds the active voice that is playing a midi note.
static synth_voice *find_voice_for_note(synth *s, int midi_note)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice->active && voice->midi_note == midi_note) {
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
    s->second_oscillator_gain = 1.0f;
    s->second_oscillator_morph = synth_waveform_to_morph(SYNTH_WAVEFORM_SQUARE);
    s->second_oscillator_octave = 0;
    s->second_oscillator_pitch_semitones = 0;
    s->second_oscillator_fine_tune_cents = 0.0f;
    s->envelope = synth_sanitize_adsr(default_envelope);
    synth_filter_init(&s->filter, sample_rate, sample_rate * 0.5f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_init(&s->voices[i], s->envelope);
    }
}

// starts a midi note.
void synth_note_on(synth *s, int midi_note, float velocity)
{
    synth_voice *voice = find_voice_for_note(s, midi_note);
    const float base_frequency = synth_midi_note_to_frequency(midi_note);

    if (voice == 0) {
        voice = find_available_voice(s);
    }

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

// starts a note by frequency instead of midi note.
void synth_note_on_frequency(synth *s, float frequency, float velocity)
{
    synth_voice *voice = find_available_voice(s);

    synth_voice_note_on(
        voice,
        -1,
        frequency,
        velocity,
        s->waveform,
        s->envelope);
    retune_voice(s, voice);
    synth_voice_set_oscillator_morph(voice, s->oscillator_morph);
    synth_voice_set_second_oscillator_morph(voice, s->second_oscillator_morph);
}

// releases a midi note if it is playing.
void synth_note_off(synth *s, int midi_note)
{
    synth_voice *voice = find_voice_for_note(s, midi_note);

    if (voice != 0) {
        synth_voice_note_off(voice);
    }
}

// releases every active voice.
void synth_all_notes_off(synth *s)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (s->voices[i].active) {
            synth_voice_note_off(&s->voices[i]);
        }
    }
}

// changes pitch bend from full down (-1) through center (0) to full up (1).
void synth_set_pitch_bend(synth *s, float pitch_bend)
{
    s->pitch_bend = synth_clampf(pitch_bend, -1.0f, 1.0f);
    s->pitch_bend_semitones = s->pitch_bend * SYNTH_PITCH_BEND_MAX_SEMITONES;
    retune_active_voices(s);
}

// changes the main output level.
void synth_set_master_gain(synth *s, float gain)
{
    s->master_gain = synth_clampf(gain, 0.0f, 1.0f);
}

// changes the envelope shape for new and active voices.
void synth_set_adsr(synth *s, synth_adsr envelope)
{
    s->envelope = synth_sanitize_adsr(envelope);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        s->voices[i].envelope.adsr = s->envelope;
    }
}

// returns the current sanitized envelope shape.
synth_adsr synth_get_adsr(const synth *s)
{
    return s->envelope;
}

// changes the default primary oscillator waveform and active primary oscillators.
void synth_set_waveform(synth *s, synth_waveform waveform)
{
    s->waveform = waveform;
    s->oscillator_morph = synth_waveform_to_morph(waveform);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_waveform(&s->voices[i], waveform);
    }
}

// changes the default primary oscillator morph and active primary oscillators.
void synth_set_oscillator_morph(synth *s, float morph)
{
    s->oscillator_morph = synth_clampf(morph, 0.0f, 1.0f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_oscillator_morph(&s->voices[i], s->oscillator_morph);
    }
}

// changes the second oscillator morph and active second oscillators.
void synth_set_second_oscillator_morph(synth *s, float morph)
{
    s->second_oscillator_morph = synth_clampf(morph, 0.0f, 1.0f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_second_oscillator_morph(&s->voices[i], s->second_oscillator_morph);
    }
}

// changes the primary oscillator output level.
void synth_set_first_oscillator_gain(synth *s, float gain)
{
    s->first_oscillator_gain = synth_clampf(gain, 0.0f, 1.0f);
}

// changes the second oscillator output level.
void synth_set_second_oscillator_gain(synth *s, float gain)
{
    s->second_oscillator_gain = synth_clampf(gain, 0.0f, 1.0f);
}

// changes the second oscillator octave offset.
void synth_set_second_oscillator_octave(synth *s, int octave)
{
    s->second_oscillator_octave = synth_clampi(octave, -1, 1);
    retune_active_voices(s);
}

// changes the second oscillator semitone offset.
void synth_set_second_oscillator_pitch(synth *s, int semitones)
{
    s->second_oscillator_pitch_semitones = synth_clampi(semitones, -6, 6);
    retune_active_voices(s);
}

// changes the second oscillator fine tune in cents.
void synth_set_second_oscillator_fine_tune(synth *s, float cents)
{
    s->second_oscillator_fine_tune_cents = synth_clampf(cents, -50.0f, 50.0f);
    retune_active_voices(s);
}

// changes the synth filter cutoff in hz.
void synth_set_filter_cutoff(synth *s, float cutoff_hz)
{
    synth_filter_set_cutoff(&s->filter, cutoff_hz);
}

// changes how many poles the synth filter uses.
void synth_set_filter_poles(synth *s, int pole_count)
{
    synth_filter_set_poles(&s->filter, pole_count);
}

// renders one mixed and filtered synth sample.
static float synth_render_sample(synth *s)
{
    const synth_voice_mix mix = synth_voice_mix_from_state(s);
    float sample = 0.0f;

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        sample += synth_voice_render_mix(&s->voices[i], s->sample_rate, mix);
    }

    return synth_filter_process(&s->filter, sample * s->master_gain);
}

// renders stereo frames into an audio buffer.
void synth_render_stereo(synth *s, synth_audio_buffer *output)
{
    for (size_t frame = 0; frame < output->frame_count; ++frame) {
        const float sample = synth_render_sample(s);

        output->left[frame] = sample;
        output->right[frame] = sample;
    }
}

// renders mono frames into a sample array.
void synth_render_mono(synth *s, float *output, size_t frame_count)
{
    for (size_t frame = 0; frame < frame_count; ++frame) {
        output[frame] = synth_render_sample(s);
    }
}
