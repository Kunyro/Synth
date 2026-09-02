#ifndef SYNTH_H
#define SYNTH_H

#include <stddef.h>

#include "synth/audio_types.h"
#include "synth/effect_chain.h"
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
    synth_effect_chain effects;
    synth_voice voices[SYNTH_MAX_VOICES];
} synth;

// sets up the synth with defaults.
void synth_init(synth *s, float sample_rate);
// releases resources owned by the synth.
void synth_uninit(synth *s);
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
// changes the input gain feeding the warm post-filter saturation.
void synth_set_saturation_drive(synth *s, float drive);
// changes the wet/dry mix for warm post-filter saturation.
void synth_set_saturation_mix(synth *s, float mix);
// changes the input gain feeding the post-filter distortion.
void synth_set_distortion_drive(synth *s, float drive);
// changes the wet/dry mix for post-filter distortion.
void synth_set_distortion_mix(synth *s, float mix);
// changes the reduced sample rate for the bitcrusher.
void synth_set_bitcrusher_sample_rate(synth *s, float sample_rate);
// changes the bit depth for the bitcrusher.
void synth_set_bitcrusher_bits(synth *s, int bits);
// changes the wet/dry mix for the bitcrusher.
void synth_set_bitcrusher_mix(synth *s, float mix);
// changes the flanger sweep rate in cycles per second.
void synth_set_flanger_rate(synth *s, float hz);
// changes flanger depth and feedback together for stronger performance sweeps.
void synth_set_flanger_intensity(synth *s, float intensity);
// changes how widely the flanger delay time sweeps.
void synth_set_flanger_depth(synth *s, float depth);
// changes how much delayed signal feeds back into the flanger.
void synth_set_flanger_feedback(synth *s, float feedback);
// changes the wet/dry mix for flanger.
void synth_set_flanger_mix(synth *s, float mix);
// changes the center delay time for the flanger sweep.
void synth_set_flanger_manual(synth *s, float seconds);
// changes the sine modulator frequency for ring modulation.
void synth_set_ring_mod_frequency(synth *s, float hz);
// changes full-wave rectification applied to the sine modulator.
void synth_set_ring_mod_rectify(synth *s, float rectify);
// changes the wet/dry mix for ring modulation.
void synth_set_ring_mod_mix(synth *s, float mix);
// changes the Juno-style chorus sweep rate in cycles per second.
void synth_set_chorus_rate(synth *s, float hz);
// changes how widely the chorus delay voices modulate.
void synth_set_chorus_depth(synth *s, float depth);
// changes the wet/dry mix for chorus.
void synth_set_chorus_mix(synth *s, float mix);
// changes how far apart the chorus stereo modulation voices are.
void synth_set_chorus_width(synth *s, float width);
// changes the center delay time for the chorus voices.
void synth_set_chorus_delay(synth *s, float seconds);
// changes how much delayed signal feeds back into the chorus.
void synth_set_chorus_feedback(synth *s, float feedback);
// changes the low shelf gain for the post-modulation EQ in decibels.
void synth_set_eq_low(synth *s, float gain_db);
// changes the mid bell gain for the post-modulation EQ in decibels.
void synth_set_eq_mid(synth *s, float gain_db);
// changes the high shelf gain for the post-modulation EQ in decibels.
void synth_set_eq_high(synth *s, float gain_db);
// changes the delay time in seconds.
void synth_set_delay_time(synth *s, float seconds);
// changes how much delayed signal feeds back into the delay line.
void synth_set_delay_feedback(synth *s, float feedback);
// changes the wet/dry mix for delay.
void synth_set_delay_mix(synth *s, float mix);
// changes the plate reverb decay time in seconds.
void synth_set_plate_reverb_decay(synth *s, float seconds);
// changes how quickly high frequencies fade inside the plate tank.
void synth_set_plate_reverb_damping(synth *s, float damping);
// changes the wet/dry mix for plate reverb.
void synth_set_plate_reverb_mix(synth *s, float mix);
// changes the delay before sound enters the plate reverb.
void synth_set_plate_reverb_predelay(synth *s, float seconds);
// changes the compressor threshold in decibels.
void synth_set_compressor_threshold(synth *s, float threshold_db);
// changes the compressor ratio from no compression upward.
void synth_set_compressor_ratio(synth *s, float ratio);
// changes the gain added after compression in decibels.
void synth_set_compressor_makeup_gain(synth *s, float makeup_gain_db);
// changes how quickly the compressor reacts to rising level.
void synth_set_compressor_attack_seconds(synth *s, float seconds);
// changes how quickly the compressor lets go after the level falls.
void synth_set_compressor_release_seconds(synth *s, float seconds);

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
float synth_get_saturation_drive(const synth *s);
float synth_get_saturation_mix(const synth *s);
float synth_get_distortion_drive(const synth *s);
float synth_get_distortion_mix(const synth *s);
float synth_get_bitcrusher_sample_rate(const synth *s);
int synth_get_bitcrusher_bits(const synth *s);
float synth_get_bitcrusher_mix(const synth *s);
float synth_get_flanger_rate(const synth *s);
float synth_get_flanger_intensity(const synth *s);
float synth_get_flanger_depth(const synth *s);
float synth_get_flanger_feedback(const synth *s);
float synth_get_flanger_mix(const synth *s);
float synth_get_flanger_manual(const synth *s);
float synth_get_ring_mod_frequency(const synth *s);
float synth_get_ring_mod_rectify(const synth *s);
float synth_get_ring_mod_mix(const synth *s);
float synth_get_chorus_rate(const synth *s);
float synth_get_chorus_depth(const synth *s);
float synth_get_chorus_mix(const synth *s);
float synth_get_chorus_width(const synth *s);
float synth_get_chorus_delay(const synth *s);
float synth_get_chorus_feedback(const synth *s);
float synth_get_eq_low(const synth *s);
float synth_get_eq_mid(const synth *s);
float synth_get_eq_high(const synth *s);
float synth_get_delay_time(const synth *s);
float synth_get_delay_feedback(const synth *s);
float synth_get_delay_mix(const synth *s);
float synth_get_plate_reverb_decay(const synth *s);
float synth_get_plate_reverb_damping(const synth *s);
float synth_get_plate_reverb_mix(const synth *s);
float synth_get_plate_reverb_predelay(const synth *s);
float synth_get_compressor_threshold(const synth *s);
float synth_get_compressor_ratio(const synth *s);
float synth_get_compressor_makeup_gain(const synth *s);
float synth_get_compressor_attack_seconds(const synth *s);
float synth_get_compressor_release_seconds(const synth *s);

// renders stereo frames into an audio buffer.
void synth_render_stereo(synth *s, synth_audio_buffer *output);
// renders mono frames into a sample array.
void synth_render_mono(synth *s, float *output, size_t frame_count);

#endif
