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

// retunes active voices without changing their original note or phase.
static void retune_active_voices(synth *s)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice->active) {
            synth_oscillator_set_frequency(&voice->oscillator, bend_frequency(s, voice->base_frequency));
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
    synth_oscillator_set_frequency(&voice->oscillator, bend_frequency(s, voice->base_frequency));
    synth_oscillator_set_morph(&voice->oscillator, s->oscillator_morph);
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
    synth_oscillator_set_frequency(&voice->oscillator, bend_frequency(s, voice->base_frequency));
    synth_oscillator_set_morph(&voice->oscillator, s->oscillator_morph);
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

// changes the default waveform and current voice waveforms.
void synth_set_waveform(synth *s, synth_waveform waveform)
{
    s->waveform = waveform;
    s->oscillator_morph = synth_waveform_to_morph(waveform);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_oscillator_set_waveform(&s->voices[i].oscillator, waveform);
    }
}

// changes the default oscillator morph and current voice morphs.
void synth_set_oscillator_morph(synth *s, float morph)
{
    s->oscillator_morph = synth_clampf(morph, 0.0f, 1.0f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_oscillator_set_morph(&s->voices[i].oscillator, s->oscillator_morph);
    }
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
    float sample = 0.0f;

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        sample += synth_voice_render(&s->voices[i], s->sample_rate);
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
