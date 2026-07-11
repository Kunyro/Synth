#ifndef SYNTH_VOICE_H
#define SYNTH_VOICE_H

#include "synth/audio_types.h"
#include "synth/envelope.h"
#include "synth/oscillator.h"

// per-voice oscillator mix controls.
typedef struct synth_voice_mix {
    float first_oscillator_gain;
    float second_oscillator_gain;
    float stereo_spread;
    float first_oscillator_morph_offset;
    float second_oscillator_morph_offset;
} synth_voice_mix;

// one playable synth voice with its oscillators and envelope.
typedef struct synth_voice {
    int active;
    int midi_note;
    float base_frequency;
    float velocity;
    synth_oscillator oscillator;
    synth_oscillator second_oscillator;
    synth_envelope envelope;
} synth_voice;

// sets up a quiet voice with the given envelope shape.
void synth_voice_init(synth_voice *voice, synth_adsr adsr);
// starts a voice on a note, pitch, velocity, waveform, and envelope.
void synth_voice_note_on(
    synth_voice *voice,
    int midi_note,
    float frequency,
    float velocity,
    synth_waveform waveform,
    synth_adsr adsr);
// retunes both oscillators in the voice.
void synth_voice_set_frequencies(synth_voice *voice, float primary_frequency, float second_frequency);
// changes the primary oscillator shape.
void synth_voice_set_waveform(synth_voice *voice, synth_waveform waveform);
// changes the primary oscillator morph.
void synth_voice_set_oscillator_morph(synth_voice *voice, float morph);
// changes the second oscillator morph.
void synth_voice_set_second_oscillator_morph(synth_voice *voice, float morph);
// releases a voice so it can fade out.
void synth_voice_note_off(synth_voice *voice);
// renders one sample from the voice.
float synth_voice_render(synth_voice *voice, float sample_rate);
// renders one sample from the voice with oscillator mix controls.
float synth_voice_render_mix(synth_voice *voice, float sample_rate, synth_voice_mix mix);
// renders one stereo sample with opposed oscillator panning.
synth_stereo_sample synth_voice_render_stereo_mix(
    synth_voice *voice,
    float sample_rate,
    synth_voice_mix mix);

#endif
