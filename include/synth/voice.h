#ifndef SYNTH_VOICE_H
#define SYNTH_VOICE_H

#include "synth/audio_types.h"
#include "synth/envelope.h"
#include "synth/filter.h"
#include "synth/effect_chain.h"
#include "synth/oscillator.h"

// per-voice oscillator mix controls
typedef struct synth_voice_mix {
    float first_oscillator_gain;
    float second_oscillator_gain;
    float stereo_spread;
    float first_oscillator_morph_offset;
    float second_oscillator_morph_offset;
} synth_voice_mix;

// a bounded replacement request, captured at the note event rather than after the fade
typedef struct synth_pending_note {
    int active;
    int note_number;
    float frequency;
    float velocity;
    synth_adsr adsr;
} synth_pending_note;

// one note's sources and dsp history; prepared voices own noncopyable resources
typedef struct synth_voice {
    int active;
    int note_number;
    float base_frequency;
    float velocity;
    synth_oscillator oscillator;
    synth_oscillator second_oscillator;
    synth_envelope envelope;
    synth_envelope mod_envelope;
    int gate;
    int prepared;
    int tail_active;
    float output_level;
    size_t tail_check_frames;
    size_t steal_frames;
    size_t steal_remaining;
    synth_pending_note pending;
    synth_filter filter;
    synth_filter right_filter;
    synth_effect_chain effects;
} synth_voice;

// sets up a quiet voice with the given envelope shape
void synth_voice_init(synth_voice *voice, synth_adsr adsr);
// optional full processing resources; initialize outside the audio callback
// prepared voices own buffers and must not be copied or reinitialized before uninit
int synth_voice_prepare(synth_voice *voice, float sample_rate);
void synth_voice_uninit(synth_voice *voice);
// clears processing history without allocation or changing base controls
void synth_voice_reset_processing(synth_voice *voice);
// starts a voice on a note, pitch, velocity, waveform, and envelope
void synth_voice_note_on(
    synth_voice *voice,
    int note_number,
    float frequency,
    float velocity,
    synth_waveform waveform,
    synth_adsr adsr);
// retunes both oscillators in the voice
void synth_voice_set_frequencies(synth_voice *voice, float primary_frequency, float second_frequency);
// changes the primary oscillator shape
void synth_voice_set_waveform(synth_voice *voice, synth_waveform waveform);
// changes the primary oscillator morph
void synth_voice_set_oscillator_morph(synth_voice *voice, float morph);
// changes the second oscillator morph
void synth_voice_set_second_oscillator_morph(synth_voice *voice, float morph);
// releases a voice so it can fade out
void synth_voice_note_off(synth_voice *voice);
// renders one sample from the voice
float synth_voice_render(synth_voice *voice, float sample_rate);
// renders one sample from the voice with oscillator mix controls
float synth_voice_render_mix(synth_voice *voice, float sample_rate, synth_voice_mix mix);
// renders one stereo sample with opposed oscillator panning
synth_stereo_sample synth_voice_render_stereo_mix(
    synth_voice *voice,
    float sample_rate,
    synth_voice_mix mix);

// applies temporary secondary tuning without changing note identity or base frequencies
synth_stereo_sample synth_voice_render_with_params(synth_voice *voice,
    float sample_rate, synth_voice_mix mix, float secondary_ratio);

// split clock and oscillator steps for hosts resolving controls between them
void synth_voice_advance_envelopes(synth_voice *voice, float sample_rate);
synth_stereo_sample synth_voice_render_current(synth_voice *voice, float sample_rate,
                                              synth_voice_mix mix, float secondary_ratio);

#endif
