#ifndef SYNTH_H
#define SYNTH_H

#include <stddef.h>

#include "synth/audio_types.h"
#include "synth/filter.h"
#include "synth/lfo.h"
#include "synth/midi_types.h"
#include "synth/synth_config.h"
#include "synth/voice.h"

// the full synth state with voices, envelope, filter, and global settings.
typedef struct synth {
    float sample_rate;
    float master_gain;
    float pitch_bend;
    float pitch_bend_semitones;
    synth_waveform waveform;
    float oscillator_morph;
    float first_oscillator_gain;
    float second_oscillator_gain;
    float stereo_spread;
    float second_oscillator_morph;
    int second_oscillator_octave;
    int second_oscillator_pitch_semitones;
    float second_oscillator_fine_tune_cents;
    synth_lfo lfo;
    float lfo_depth;
    float lfo_first_oscillator_morph_amount;
    float lfo_second_oscillator_morph_amount;
    float lfo_first_oscillator_gain_amount;
    float lfo_second_oscillator_gain_amount;
    float lfo_filter_amount;
    synth_adsr envelope;
    synth_filter filter;
    synth_filter right_filter;
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
// changes pitch bend from full down (-1) through center (0) to full up (1).
void synth_set_pitch_bend(synth *s, float pitch_bend);
// changes the main output level.
void synth_set_master_gain(synth *s, float gain);
// changes the envelope shape for new and active voices.
void synth_set_adsr(synth *s, synth_adsr envelope);
// returns the current sanitized envelope shape.
synth_adsr synth_get_adsr(const synth *s);
// changes the default primary oscillator waveform and active primary oscillators.
void synth_set_waveform(synth *s, synth_waveform waveform);
// changes the default primary oscillator morph and active primary oscillators.
void synth_set_oscillator_morph(synth *s, float morph);
// changes the primary oscillator output level.
void synth_set_first_oscillator_gain(synth *s, float gain);
// changes the second oscillator output level.
void synth_set_second_oscillator_gain(synth *s, float gain);
// changes opposed oscillator panning from centered to fully spread.
void synth_set_stereo_spread(synth *s, float spread);
// changes the second oscillator morph and active second oscillators.
void synth_set_second_oscillator_morph(synth *s, float morph);
// changes the second oscillator octave offset.
void synth_set_second_oscillator_octave(synth *s, int octave);
// changes the second oscillator semitone offset.
void synth_set_second_oscillator_pitch(synth *s, int semitones);
// changes the second oscillator fine tune in cents.
void synth_set_second_oscillator_fine_tune(synth *s, float cents);
// changes the synth filter cutoff in hz.
void synth_set_filter_cutoff(synth *s, float cutoff_hz);
// changes how many poles the synth filter uses.
void synth_set_filter_poles(synth *s, int pole_count);
// changes the global lfo rate in cycles per second.
void synth_set_lfo_rate(synth *s, float frequency_hz);
// changes the global lfo shape from sine through saw to square.
void synth_set_lfo_shape_morph(synth *s, float morph);
// changes the master multiplier applied to every lfo route.
void synth_set_lfo_depth(synth *s, float depth);
// changes how strongly the lfo moves the primary oscillator morph.
void synth_set_lfo_first_oscillator_morph_amount(synth *s, float amount);
// changes how strongly the lfo moves the second oscillator morph.
void synth_set_lfo_second_oscillator_morph_amount(synth *s, float amount);
// changes how strongly the lfo modulates the primary oscillator level.
void synth_set_lfo_first_oscillator_gain_amount(synth *s, float amount);
// changes how strongly the lfo modulates the second oscillator level.
void synth_set_lfo_second_oscillator_gain_amount(synth *s, float amount);
// changes how strongly the lfo moves the shared filter cutoff.
void synth_set_lfo_filter_amount(synth *s, float amount);

// reads current synth values without exposing where they are stored.
float synth_get_master_gain(const synth *s);
float synth_get_oscillator_morph(const synth *s);
float synth_get_first_oscillator_gain(const synth *s);
float synth_get_second_oscillator_gain(const synth *s);
float synth_get_stereo_spread(const synth *s);
float synth_get_second_oscillator_morph(const synth *s);
int synth_get_second_oscillator_octave(const synth *s);
int synth_get_second_oscillator_pitch(const synth *s);
float synth_get_second_oscillator_fine_tune(const synth *s);
float synth_get_filter_cutoff(const synth *s);
int synth_get_filter_poles(const synth *s);
float synth_get_lfo_rate(const synth *s);
float synth_get_lfo_shape_morph(const synth *s);
float synth_get_lfo_depth(const synth *s);
float synth_get_lfo_first_oscillator_morph_amount(const synth *s);
float synth_get_lfo_second_oscillator_morph_amount(const synth *s);
float synth_get_lfo_first_oscillator_gain_amount(const synth *s);
float synth_get_lfo_second_oscillator_gain_amount(const synth *s);
float synth_get_lfo_filter_amount(const synth *s);

// renders stereo frames into an audio buffer.
void synth_render_stereo(synth *s, synth_audio_buffer *output);
// renders mono frames into a sample array.
void synth_render_mono(synth *s, float *output, size_t frame_count);

#endif
