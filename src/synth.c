#include "synth/synth.h"

#include <string.h>

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

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

void synth_init(synth *s, float sample_rate)
{
    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate;
    s->master_gain = SYNTH_DEFAULT_MASTER_GAIN;
    s->waveform = SYNTH_WAVEFORM_SINE;
    s->envelope.attack_seconds = 0.01f;
    s->envelope.decay_seconds = 0.08f;
    s->envelope.sustain_level = 0.75f;
    s->envelope.release_seconds = 0.16f;
    synth_filter_init(&s->filter, sample_rate * 0.5f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_init(&s->voices[i], s->envelope);
    }
}

void synth_note_on(synth *s, int midi_note, float velocity)
{
    synth_voice *voice = find_voice_for_note(s, midi_note);

    if (voice == 0) {
        voice = find_available_voice(s);
    }

    synth_voice_note_on(
        voice,
        midi_note,
        synth_midi_note_to_frequency(midi_note),
        velocity,
        s->waveform,
        s->envelope);
}

void synth_note_on_frequency(synth *s, float frequency, float velocity)
{
    synth_voice_note_on(
        find_available_voice(s),
        -1,
        frequency,
        velocity,
        s->waveform,
        s->envelope);
}

void synth_note_off(synth *s, int midi_note)
{
    synth_voice *voice = find_voice_for_note(s, midi_note);

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

void synth_set_master_gain(synth *s, float gain)
{
    s->master_gain = clampf(gain, 0.0f, 1.0f);
}

void synth_set_adsr(synth *s, synth_adsr envelope)
{
    s->envelope.attack_seconds = envelope.attack_seconds;
    s->envelope.decay_seconds = envelope.decay_seconds;
    s->envelope.sustain_level = clampf(envelope.sustain_level, 0.0f, 1.0f);
    s->envelope.release_seconds = envelope.release_seconds;
}

void synth_set_waveform(synth *s, synth_waveform waveform)
{
    s->waveform = waveform;

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_oscillator_set_waveform(&s->voices[i].oscillator, waveform);
    }
}

void synth_set_filter_cutoff(synth *s, float cutoff_hz)
{
    synth_filter_set_cutoff(&s->filter, cutoff_hz);
}

void synth_render_mono(synth *s, float *output, size_t frame_count)
{
    for (size_t frame = 0; frame < frame_count; ++frame) {
        float sample = 0.0f;

        for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
            sample += synth_voice_render(&s->voices[i], s->sample_rate);
        }

        output[frame] = synth_filter_process(&s->filter, sample * s->master_gain, s->sample_rate);
    }
}
