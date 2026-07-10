#include "synth/voice.h"

#include "internal/synth_internal.h"

static const float voice_oscillator_mix_gain = 0.5f;

// sets up both oscillators at the same pitch.
static void init_oscillators(synth_voice *voice, synth_waveform waveform, float frequency)
{
    synth_oscillator_init(&voice->oscillator, waveform, frequency);
    synth_oscillator_init(&voice->second_oscillator, SYNTH_WAVEFORM_SQUARE, frequency);
}

// renders the voice oscillator pair before envelope and voice gain.
static float render_oscillators(synth_voice *voice, float sample_rate)
{
    const float primary = synth_oscillator_render(&voice->oscillator, sample_rate);
    const float secondary = synth_oscillator_render(&voice->second_oscillator, sample_rate);

    return (primary + secondary) * voice_oscillator_mix_gain;
}

// sets up a quiet voice with the given envelope shape.
void synth_voice_init(synth_voice *voice, synth_adsr adsr)
{
    voice->active = 0;
    voice->midi_note = -1;
    voice->base_frequency = 0.0f;
    voice->velocity = 0.0f;
    init_oscillators(voice, SYNTH_WAVEFORM_SINE, 0.0f);
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
    voice->base_frequency = frequency;
    voice->velocity = synth_clampf(velocity, 0.0f, 1.0f);
    init_oscillators(voice, waveform, frequency);
    synth_envelope_init(&voice->envelope, adsr);
    synth_envelope_note_on(&voice->envelope);
}

// retunes every oscillator in the voice together.
void synth_voice_set_frequency(synth_voice *voice, float frequency)
{
    synth_oscillator_set_frequency(&voice->oscillator, frequency);
    synth_oscillator_set_frequency(&voice->second_oscillator, frequency);
}

// changes the primary oscillator shape.
void synth_voice_set_waveform(synth_voice *voice, synth_waveform waveform)
{
    synth_oscillator_set_waveform(&voice->oscillator, waveform);
}

// changes the primary oscillator morph.
void synth_voice_set_oscillator_morph(synth_voice *voice, float morph)
{
    synth_oscillator_set_morph(&voice->oscillator, morph);
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

    oscillator_sample = render_oscillators(voice, sample_rate);
    return oscillator_sample * envelope_level * voice->velocity;
}
