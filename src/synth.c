#include "synth/synth.h"

#include <string.h>

// keeps a float inside a min and max range.
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

// sets up the synth with defaults.
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

// starts a midi note.
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

// starts a note by frequency instead of midi note.
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

// changes the main output level.
void synth_set_master_gain(synth *s, float gain)
{
    s->master_gain = clampf(gain, 0.0f, 1.0f);
}

// changes the envelope shape for new and active voices.
void synth_set_adsr(synth *s, synth_adsr envelope)
{
    s->envelope.attack_seconds = envelope.attack_seconds;
    s->envelope.decay_seconds = envelope.decay_seconds;
    s->envelope.sustain_level = clampf(envelope.sustain_level, 0.0f, 1.0f);
    s->envelope.release_seconds = envelope.release_seconds;

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        s->voices[i].envelope.adsr = s->envelope;
    }
}

// changes the default waveform and current voice waveforms.
void synth_set_waveform(synth *s, synth_waveform waveform)
{
    s->waveform = waveform;

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_oscillator_set_waveform(&s->voices[i].oscillator, waveform);
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

    return synth_filter_process(&s->filter, sample * s->master_gain, s->sample_rate);
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
