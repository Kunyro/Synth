#include "synth/voice.h"

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

// sets up a quiet voice with the given envelope shape.
void synth_voice_init(synth_voice *voice, synth_adsr adsr)
{
    voice->active = 0;
    voice->midi_note = -1;
    voice->velocity = 0.0f;
    synth_oscillator_init(&voice->oscillator, SYNTH_WAVEFORM_SINE, 0.0f);
    synth_envelope_init(&voice->envelope, adsr);
}

// starts a voice on a note, pitch, velocity, waveform, and envelope.
void synth_voice_note_on(
    synth_voice *voice,
    int midi_note,
    float frequency,
    float velocity,
    synth_waveform waveform,
    synth_adsr adsr)
{
    voice->active = 1;
    voice->midi_note = midi_note;
    voice->velocity = clampf(velocity, 0.0f, 1.0f);
    synth_oscillator_init(&voice->oscillator, waveform, frequency);
    synth_envelope_init(&voice->envelope, adsr);
    synth_envelope_note_on(&voice->envelope);
}

// releases a voice so it can fade out.
void synth_voice_note_off(synth_voice *voice)
{
    synth_envelope_note_off(&voice->envelope);
}

// renders one sample from the voice.
float synth_voice_render(synth_voice *voice, float sample_rate)
{
    float envelope_level;
    float oscillator_sample;

    if (!voice->active) {
        return 0.0f;
    }

    envelope_level = synth_envelope_advance(&voice->envelope, sample_rate);
    if (!synth_envelope_is_active(&voice->envelope)) {
        voice->active = 0;
        return 0.0f;
    }

    oscillator_sample = synth_oscillator_render(&voice->oscillator, sample_rate);
    return oscillator_sample * envelope_level * voice->velocity;
}
