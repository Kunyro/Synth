#ifndef SYNTH_H
#define SYNTH_H

#include <stddef.h>

#include "synth/audio_types.h"
#include "synth/filter.h"
#include "synth/midi_types.h"
#include "synth/synth_config.h"
#include "synth/voice.h"

typedef struct synth {
    float sample_rate;
    float master_gain;
    synth_waveform waveform;
    synth_adsr envelope;
    synth_filter filter;
    synth_voice voices[SYNTH_MAX_VOICES];
} synth;

void synth_init(synth *s, float sample_rate);
void synth_note_on_frequency(synth *s, float frequency, float velocity);
void synth_note_on(synth *s, int midi_note, float velocity);
void synth_note_off(synth *s, int midi_note);
void synth_all_notes_off(synth *s);
void synth_set_master_gain(synth *s, float gain);
void synth_set_adsr(synth *s, synth_adsr envelope);
void synth_set_waveform(synth *s, synth_waveform waveform);
void synth_set_filter_cutoff(synth *s, float cutoff_hz);
void synth_set_filter_poles(synth *s, int pole_count);
void synth_render_stereo(synth *s, synth_audio_buffer *output);
void synth_render_mono(synth *s, float *output, size_t frame_count);

#endif
