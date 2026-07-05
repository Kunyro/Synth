#ifndef SYNTH_H
#define SYNTH_H

#include <stddef.h>

#include "synth/audio_types.h"
#include "synth/filter.h"
#include "synth/midi_types.h"
#include "synth/synth_config.h"
#include "synth/voice.h"

// the full synth state with voices, envelope, filter, and global settings.
typedef struct synth {
    float sample_rate;
    float master_gain;
    synth_waveform waveform;
    float oscillator_morph;
    synth_adsr envelope;
    synth_filter filter;
    synth_voice voices[SYNTH_MAX_VOICES];
} synth;

// sets up the synth with defaults.
void synth_init(synth *s, float sample_rate);
// starts a note by frequency instead of midi note.
void synth_note_on_frequency(synth *s, float frequency, float velocity);
// starts a midi note.
void synth_note_on(synth *s, int midi_note, float velocity);
// releases a midi note if it is playing.
void synth_note_off(synth *s, int midi_note);
// releases every active voice.
void synth_all_notes_off(synth *s);
// changes the main output level.
void synth_set_master_gain(synth *s, float gain);
// changes the envelope shape for new and active voices.
void synth_set_adsr(synth *s, synth_adsr envelope);
// returns the current sanitized envelope shape.
synth_adsr synth_get_adsr(const synth *s);
// changes the default waveform and current voice waveforms.
void synth_set_waveform(synth *s, synth_waveform waveform);
// changes the default oscillator morph and current voice morphs.
void synth_set_oscillator_morph(synth *s, float morph);
// changes the synth filter cutoff in hz.
void synth_set_filter_cutoff(synth *s, float cutoff_hz);
// changes how many poles the synth filter uses.
void synth_set_filter_poles(synth *s, int pole_count);
// renders stereo frames into an audio buffer.
void synth_render_stereo(synth *s, synth_audio_buffer *output);
// renders mono frames into a sample array.
void synth_render_mono(synth *s, float *output, size_t frame_count);

#endif
