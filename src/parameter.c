#include "synth/parameter.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include "internal/render_parameters.h"
#include "internal/synth_internal.h"

// reads the stored attack time for the generic float-valued parameter api
static float get_attack(const synth *s)
{
    return synth_get_adsr(s).attack_seconds;
}

// reads the stored decay time from the base envelope
static float get_decay(const synth *s)
{
    return synth_get_adsr(s).decay_seconds;
}

// reads the stored sustain level from the base envelope
static float get_sustain(const synth *s)
{
    return synth_get_adsr(s).sustain_level;
}

// reads the stored release time from the base envelope
static float get_release(const synth *s)
{
    return synth_get_adsr(s).release_seconds;
}

// base adsr edits preserve the existing whole-envelope setter behavior
static void set_attack(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.attack_seconds = value;
    synth_set_adsr(s, adsr);
}

// changes base decay through the whole-envelope setter, keeping manual voice updates intact
static void set_decay(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.decay_seconds = value;
    synth_set_adsr(s, adsr);
}

// changes base sustain through the whole-envelope setter, keeping manual voice updates intact
static void set_sustain(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.sustain_level = value;
    synth_set_adsr(s, adsr);
}

// changes base release through the whole-envelope setter, keeping manual voice updates intact
static void set_release(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.release_seconds = value;
    synth_set_adsr(s, adsr);
}

// exposes the stored integer octave setting as a float for the common catalog api
static float get_second_oscillator_octave(const synth *s)
{
    return (float)synth_get_second_oscillator_octave(s);
}

// exposes the stored integer semitone setting as a float for the common catalog api
static float get_second_oscillator_pitch(const synth *s)
{
    return (float)synth_get_second_oscillator_pitch(s);
}

// exposes the stored integer pole count as a float for the common catalog api
static float get_filter_poles(const synth *s)
{
    return (float)synth_get_filter_poles(s);
}

// rounds a catalog value to a whole pole count before calling the filter setter
static void set_filter_poles(synth *s, float value)
{
    synth_set_filter_poles(s, (int)roundf(value));
}

// rounds a catalog value to a whole octave, rather than introducing pitch glide
static void set_second_oscillator_octave(synth *s, float value)
{
    synth_set_second_oscillator_octave(s, (int)roundf(value));
}

// rounds a catalog value to whole semitones, with half steps rounded away from zero
static void set_second_oscillator_pitch(synth *s, float value)
{
    synth_set_second_oscillator_pitch(s, (int)roundf(value));
}

// exposes the stored integer bit depth as a float for the common catalog api
static float get_bitcrusher_bits(const synth *s)
{
    return (float)synth_get_bitcrusher_bits(s);
}

// rounds a catalog value to whole bits before using the manual bit-depth setter
static void set_bitcrusher_bits(synth *s, float value)
{
    synth_set_bitcrusher_bits(s, (int)roundf(value));
}

typedef struct parameter_entry {
    synth_parameter_info info;
    // these functions access the owner's existing setting; the table stores no values
    float (*get)(const synth *s);
    void (*set)(synth *s, float value);
    // byte position of this control in the temporary render frame, found by offsetof()
    size_t render_offset;
} parameter_entry;

// marks controls that are not evaluated every frame (adsr and the global lfo)
// sizeof the frame is just beyond its last byte, so it cannot be a real field offset
#define NO_RENDER_VALUE sizeof(synth_render_parameters)

// each row holds {id, name, unit, type, domain, minimum, maximum, span, eligible},
// followed by base accessors and the render destination a span is the maximum
// change on either side of the base at full route amount and global depth
// linear spans are generally half the useful control range: mix 0..1 gives 0.5,
// poles 1..8 gives 3.5, and delay 0.001..2 seconds gives 0.9995 seconds
// log spans are fixed octave excursions (for example, 5 means a factor of 32)
// these are sound-design choices, independent of any controller's knob range
// the largest finite float preserves uncapped base ranges; sample-rate limits are applied below
static const parameter_entry parameters[SYNTH_PARAM_COUNT] = {
    [SYNTH_PARAM_ATTACK] = {
        {SYNTH_PARAM_ATTACK, "attack", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, FLT_MAX, 1.0f, 1},
        get_attack, set_attack, NO_RENDER_VALUE
    },
    [SYNTH_PARAM_DECAY] = {
        {SYNTH_PARAM_DECAY, "decay", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, FLT_MAX, 1.0f, 1},
        get_decay, set_decay, NO_RENDER_VALUE
    },
    [SYNTH_PARAM_SUSTAIN] = {
        {SYNTH_PARAM_SUSTAIN, "sustain", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        get_sustain, set_sustain, NO_RENDER_VALUE
    },
    [SYNTH_PARAM_RELEASE] = {
        {SYNTH_PARAM_RELEASE, "release", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, FLT_MAX, 1.5f, 1},
        get_release, set_release, NO_RENDER_VALUE
    },
    [SYNTH_PARAM_MASTER_GAIN] = {
        {SYNTH_PARAM_MASTER_GAIN, "master_gain", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_master_gain, synth_set_master_gain, offsetof(synth_render_parameters, master_gain)
    },
    [SYNTH_PARAM_FILTER_CUTOFF] = {
        {SYNTH_PARAM_FILTER_CUTOFF, "filter_cutoff", "Hz", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LOG2, 10.0f, FLT_MAX, 5.0f, 1},
        synth_get_filter_cutoff, synth_set_filter_cutoff, offsetof(synth_render_parameters, filter.cutoff_hz)
    },
    [SYNTH_PARAM_FILTER_POLES] = {
        {SYNTH_PARAM_FILTER_POLES, "filter_poles", "poles", SYNTH_PARAMETER_INTEGER,
         SYNTH_DOMAIN_LINEAR, 1.0f, 8.0f, 3.5f, 1},
        get_filter_poles, set_filter_poles, offsetof(synth_render_parameters, filter.pole_count)
    },
    [SYNTH_PARAM_OSCILLATOR_MORPH] = {
        {SYNTH_PARAM_OSCILLATOR_MORPH, "oscillator_morph", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_oscillator_morph, synth_set_oscillator_morph, offsetof(synth_render_parameters, oscillator_morph)
    },
    [SYNTH_PARAM_FIRST_OSCILLATOR_GAIN] = {
        {SYNTH_PARAM_FIRST_OSCILLATOR_GAIN, "first_oscillator_gain", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_first_oscillator_gain, synth_set_first_oscillator_gain, offsetof(synth_render_parameters, first_oscillator_gain)
    },
    [SYNTH_PARAM_SECOND_OSCILLATOR_GAIN] = {
        {SYNTH_PARAM_SECOND_OSCILLATOR_GAIN, "second_oscillator_gain", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_second_oscillator_gain, synth_set_second_oscillator_gain, offsetof(synth_render_parameters, second_oscillator_gain)
    },
    [SYNTH_PARAM_SECOND_OSCILLATOR_MORPH] = {
        {SYNTH_PARAM_SECOND_OSCILLATOR_MORPH, "second_oscillator_morph", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_second_oscillator_morph, synth_set_second_oscillator_morph, offsetof(synth_render_parameters, second_oscillator_morph)
    },
    [SYNTH_PARAM_SECOND_OSCILLATOR_OCTAVE] = {
        {SYNTH_PARAM_SECOND_OSCILLATOR_OCTAVE, "second_oscillator_octave", "octaves", SYNTH_PARAMETER_INTEGER,
         SYNTH_DOMAIN_LINEAR, -1.0f, 1.0f, 1.0f, 1},
        get_second_oscillator_octave, set_second_oscillator_octave, offsetof(synth_render_parameters, second_oscillator_octave)
    },
    [SYNTH_PARAM_SECOND_OSCILLATOR_PITCH] = {
        {SYNTH_PARAM_SECOND_OSCILLATOR_PITCH, "second_oscillator_pitch", "semitones", SYNTH_PARAMETER_INTEGER,
         SYNTH_DOMAIN_LINEAR, -6.0f, 6.0f, 6.0f, 1},
        get_second_oscillator_pitch, set_second_oscillator_pitch, offsetof(synth_render_parameters, second_oscillator_pitch)
    },
    [SYNTH_PARAM_SECOND_OSCILLATOR_FINE_TUNE] = {
        {SYNTH_PARAM_SECOND_OSCILLATOR_FINE_TUNE, "second_oscillator_fine_tune", "cents", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, -50.0f, 50.0f, 50.0f, 1},
        synth_get_second_oscillator_fine_tune, synth_set_second_oscillator_fine_tune, offsetof(synth_render_parameters, second_oscillator_fine_tune)
    },
    [SYNTH_PARAM_STEREO_SPREAD] = {
        {SYNTH_PARAM_STEREO_SPREAD, "stereo_spread", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_stereo_spread, synth_set_stereo_spread, offsetof(synth_render_parameters, stereo_spread)
    },
    [SYNTH_PARAM_LFO_RATE] = {
        {SYNTH_PARAM_LFO_RATE, "lfo_rate", "Hz", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, FLT_MAX, 0.0f, 0},
        synth_get_lfo_rate, synth_set_lfo_rate, NO_RENDER_VALUE
    },
    [SYNTH_PARAM_LFO_SHAPE_MORPH] = {
        {SYNTH_PARAM_LFO_SHAPE_MORPH, "lfo_shape_morph", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.0f, 0},
        synth_get_lfo_shape_morph, synth_set_lfo_shape_morph, NO_RENDER_VALUE
    },
    [SYNTH_PARAM_LFO_DEPTH] = {
        {SYNTH_PARAM_LFO_DEPTH, "lfo_depth", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.0f, 0},
        synth_get_lfo_depth, synth_set_lfo_depth, NO_RENDER_VALUE
    },
    [SYNTH_PARAM_SATURATION_DRIVE] = {
        {SYNTH_PARAM_SATURATION_DRIVE, "saturation_drive", "dimensionless", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_SATURATION_MIN_DRIVE, SYNTH_SATURATION_MAX_DRIVE, 12.0f, 1},
        synth_get_saturation_drive, synth_set_saturation_drive, offsetof(synth_render_parameters, effects.saturation.drive)
    },
    [SYNTH_PARAM_SATURATION_MIX] = {
        {SYNTH_PARAM_SATURATION_MIX, "saturation_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_saturation_mix, synth_set_saturation_mix, offsetof(synth_render_parameters, effects.saturation.mix)
    },
    [SYNTH_PARAM_DISTORTION_DRIVE] = {
        {SYNTH_PARAM_DISTORTION_DRIVE, "distortion_drive", "dimensionless", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_DISTORTION_MIN_DRIVE, SYNTH_DISTORTION_MAX_DRIVE, 16.0f, 1},
        synth_get_distortion_drive, synth_set_distortion_drive, offsetof(synth_render_parameters, effects.distortion.drive)
    },
    [SYNTH_PARAM_DISTORTION_MIX] = {
        {SYNTH_PARAM_DISTORTION_MIX, "distortion_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_distortion_mix, synth_set_distortion_mix, offsetof(synth_render_parameters, effects.distortion.mix)
    },
    [SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE] = {
        {SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE, "bitcrusher_sample_rate", "Hz", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LOG2, SYNTH_BITCRUSHER_MIN_SAMPLE_RATE, FLT_MAX, 4.5f, 1},
        synth_get_bitcrusher_sample_rate, synth_set_bitcrusher_sample_rate, offsetof(synth_render_parameters, effects.bitcrusher.sample_rate)
    },
    [SYNTH_PARAM_BITCRUSHER_BITS] = {
        {SYNTH_PARAM_BITCRUSHER_BITS, "bitcrusher_bits", "bits", SYNTH_PARAMETER_INTEGER,
         SYNTH_DOMAIN_LINEAR, 1.0f, 16.0f, 7.5f, 1},
        get_bitcrusher_bits, set_bitcrusher_bits, offsetof(synth_render_parameters, effects.bitcrusher.bits)
    },
    [SYNTH_PARAM_BITCRUSHER_MIX] = {
        {SYNTH_PARAM_BITCRUSHER_MIX, "bitcrusher_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_bitcrusher_mix, synth_set_bitcrusher_mix, offsetof(synth_render_parameters, effects.bitcrusher.mix)
    },
    [SYNTH_PARAM_FLANGER_RATE] = {
        {SYNTH_PARAM_FLANGER_RATE, "flanger_rate", "Hz", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LOG2, SYNTH_FLANGER_MIN_RATE_HZ, SYNTH_FLANGER_MAX_RATE_HZ, 4.8f, 1},
        synth_get_flanger_rate, synth_set_flanger_rate, offsetof(synth_render_parameters, effects.flanger.rate_hz)
    },
    [SYNTH_PARAM_FLANGER_INTENSITY] = {
        {SYNTH_PARAM_FLANGER_INTENSITY, "flanger_intensity", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_flanger_intensity, synth_set_flanger_intensity, offsetof(synth_render_parameters, effects.flanger.intensity)
    },
    [SYNTH_PARAM_FLANGER_DEPTH] = {
        {SYNTH_PARAM_FLANGER_DEPTH, "flanger_depth", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_flanger_depth, synth_set_flanger_depth, offsetof(synth_render_parameters, effects.flanger.depth)
    },
    [SYNTH_PARAM_FLANGER_FEEDBACK] = {
        {SYNTH_PARAM_FLANGER_FEEDBACK, "flanger_feedback", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, -SYNTH_FLANGER_MAX_FEEDBACK, SYNTH_FLANGER_MAX_FEEDBACK, 0.95f, 1},
        synth_get_flanger_feedback, synth_set_flanger_feedback, offsetof(synth_render_parameters, effects.flanger.feedback)
    },
    [SYNTH_PARAM_FLANGER_MIX] = {
        {SYNTH_PARAM_FLANGER_MIX, "flanger_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_flanger_mix, synth_set_flanger_mix, offsetof(synth_render_parameters, effects.flanger.mix)
    },
    [SYNTH_PARAM_FLANGER_MANUAL] = {
        {SYNTH_PARAM_FLANGER_MANUAL, "flanger_manual", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_FLANGER_MIN_MANUAL_SECONDS, SYNTH_FLANGER_MAX_MANUAL_SECONDS, 0.0039f, 1},
        synth_get_flanger_manual, synth_set_flanger_manual, offsetof(synth_render_parameters, effects.flanger.manual_delay_seconds)
    },
    [SYNTH_PARAM_RING_MOD_FREQUENCY] = {
        {SYNTH_PARAM_RING_MOD_FREQUENCY, "ring_mod_frequency", "Hz", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LOG2, SYNTH_RING_MOD_MIN_FREQUENCY_HZ, SYNTH_RING_MOD_MAX_FREQUENCY_HZ, 4.5f, 1},
        synth_get_ring_mod_frequency, synth_set_ring_mod_frequency, offsetof(synth_render_parameters, effects.ring_mod.frequency_hz)
    },
    [SYNTH_PARAM_RING_MOD_RECTIFY] = {
        {SYNTH_PARAM_RING_MOD_RECTIFY, "ring_mod_rectify", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_RING_MOD_MIN_RECTIFY, SYNTH_RING_MOD_MAX_RECTIFY, 1.0f, 1},
        synth_get_ring_mod_rectify, synth_set_ring_mod_rectify, offsetof(synth_render_parameters, effects.ring_mod.rectify)
    },
    [SYNTH_PARAM_RING_MOD_MIX] = {
        {SYNTH_PARAM_RING_MOD_MIX, "ring_mod_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_ring_mod_mix, synth_set_ring_mod_mix, offsetof(synth_render_parameters, effects.ring_mod.mix)
    },
    [SYNTH_PARAM_CHORUS_RATE] = {
        {SYNTH_PARAM_CHORUS_RATE, "chorus_rate", "Hz", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LOG2, SYNTH_CHORUS_MIN_RATE_HZ, SYNTH_CHORUS_MAX_RATE_HZ, 3.7f, 1},
        synth_get_chorus_rate, synth_set_chorus_rate, offsetof(synth_render_parameters, effects.chorus.rate_hz)
    },
    [SYNTH_PARAM_CHORUS_DEPTH] = {
        {SYNTH_PARAM_CHORUS_DEPTH, "chorus_depth", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_chorus_depth, synth_set_chorus_depth, offsetof(synth_render_parameters, effects.chorus.depth)
    },
    [SYNTH_PARAM_CHORUS_MIX] = {
        {SYNTH_PARAM_CHORUS_MIX, "chorus_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_chorus_mix, synth_set_chorus_mix, offsetof(synth_render_parameters, effects.chorus.mix)
    },
    [SYNTH_PARAM_CHORUS_WIDTH] = {
        {SYNTH_PARAM_CHORUS_WIDTH, "chorus_width", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_chorus_width, synth_set_chorus_width, offsetof(synth_render_parameters, effects.chorus.width)
    },
    [SYNTH_PARAM_CHORUS_DELAY] = {
        {SYNTH_PARAM_CHORUS_DELAY, "chorus_delay", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_CHORUS_MIN_DELAY_SECONDS, SYNTH_CHORUS_MAX_DELAY_SECONDS, 0.012f, 1},
        synth_get_chorus_delay, synth_set_chorus_delay, offsetof(synth_render_parameters, effects.chorus.delay_seconds)
    },
    [SYNTH_PARAM_CHORUS_FEEDBACK] = {
        {SYNTH_PARAM_CHORUS_FEEDBACK, "chorus_feedback", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, -SYNTH_CHORUS_MAX_FEEDBACK, SYNTH_CHORUS_MAX_FEEDBACK, 0.35f, 1},
        synth_get_chorus_feedback, synth_set_chorus_feedback, offsetof(synth_render_parameters, effects.chorus.feedback)
    },
    [SYNTH_PARAM_EQ_LOW] = {
        {SYNTH_PARAM_EQ_LOW, "eq_low", "dB", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_EQ_MIN_GAIN_DB, SYNTH_EQ_MAX_GAIN_DB, 12.0f, 1},
        synth_get_eq_low, synth_set_eq_low, offsetof(synth_render_parameters, effects.eq.low_gain_db)
    },
    [SYNTH_PARAM_EQ_MID] = {
        {SYNTH_PARAM_EQ_MID, "eq_mid", "dB", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_EQ_MIN_GAIN_DB, SYNTH_EQ_MAX_GAIN_DB, 12.0f, 1},
        synth_get_eq_mid, synth_set_eq_mid, offsetof(synth_render_parameters, effects.eq.mid_gain_db)
    },
    [SYNTH_PARAM_EQ_HIGH] = {
        {SYNTH_PARAM_EQ_HIGH, "eq_high", "dB", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_EQ_MIN_GAIN_DB, SYNTH_EQ_MAX_GAIN_DB, 12.0f, 1},
        synth_get_eq_high, synth_set_eq_high, offsetof(synth_render_parameters, effects.eq.high_gain_db)
    },
    [SYNTH_PARAM_DELAY_TIME] = {
        {SYNTH_PARAM_DELAY_TIME, "delay_time", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.001f, 2.0f, 0.9995f, 1},
        synth_get_delay_time, synth_set_delay_time, offsetof(synth_render_parameters, effects.delay.time_seconds)
    },
    [SYNTH_PARAM_DELAY_FEEDBACK] = {
        {SYNTH_PARAM_DELAY_FEEDBACK, "delay_feedback", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 0.95f, 0.475f, 1},
        synth_get_delay_feedback, synth_set_delay_feedback, offsetof(synth_render_parameters, effects.delay.feedback)
    },
    [SYNTH_PARAM_DELAY_MIX] = {
        {SYNTH_PARAM_DELAY_MIX, "delay_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_delay_mix, synth_set_delay_mix, offsetof(synth_render_parameters, effects.delay.mix)
    },
    [SYNTH_PARAM_PLATE_REVERB_DECAY] = {
        {SYNTH_PARAM_PLATE_REVERB_DECAY, "plate_reverb_decay", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS, SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS, 4.95f, 1},
        synth_get_plate_reverb_decay, synth_set_plate_reverb_decay, offsetof(synth_render_parameters, effects.plate_reverb.decay_seconds)
    },
    [SYNTH_PARAM_PLATE_REVERB_DAMPING] = {
        {SYNTH_PARAM_PLATE_REVERB_DAMPING, "plate_reverb_damping", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_plate_reverb_damping, synth_set_plate_reverb_damping, offsetof(synth_render_parameters, effects.plate_reverb.damping)
    },
    [SYNTH_PARAM_PLATE_REVERB_MIX] = {
        {SYNTH_PARAM_PLATE_REVERB_MIX, "plate_reverb_mix", "normalized", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, 1.0f, 0.5f, 1},
        synth_get_plate_reverb_mix, synth_set_plate_reverb_mix, offsetof(synth_render_parameters, effects.plate_reverb.mix)
    },
    [SYNTH_PARAM_PLATE_REVERB_PREDELAY] = {
        {SYNTH_PARAM_PLATE_REVERB_PREDELAY, "plate_reverb_predelay", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, 0.0f, SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS, 0.1f, 1},
        synth_get_plate_reverb_predelay, synth_set_plate_reverb_predelay, offsetof(synth_render_parameters, effects.plate_reverb.predelay_seconds)
    },
    [SYNTH_PARAM_COMPRESSOR_THRESHOLD] = {
        {SYNTH_PARAM_COMPRESSOR_THRESHOLD, "compressor_threshold", "dB", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_COMPRESSOR_MIN_THRESHOLD_DB, SYNTH_COMPRESSOR_MAX_THRESHOLD_DB, 30.0f, 1},
        synth_get_compressor_threshold, synth_set_compressor_threshold, offsetof(synth_render_parameters, effects.compressor.threshold_db)
    },
    [SYNTH_PARAM_COMPRESSOR_RATIO] = {
        {SYNTH_PARAM_COMPRESSOR_RATIO, "compressor_ratio", "ratio", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_COMPRESSOR_MIN_RATIO, SYNTH_COMPRESSOR_MAX_RATIO, 9.5f, 1},
        synth_get_compressor_ratio, synth_set_compressor_ratio, offsetof(synth_render_parameters, effects.compressor.ratio)
    },
    [SYNTH_PARAM_COMPRESSOR_MAKEUP_GAIN] = {
        {SYNTH_PARAM_COMPRESSOR_MAKEUP_GAIN, "compressor_makeup_gain", "dB", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LINEAR, SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB, SYNTH_COMPRESSOR_MAX_MAKEUP_GAIN_DB, 12.0f, 1},
        synth_get_compressor_makeup_gain, synth_set_compressor_makeup_gain, offsetof(synth_render_parameters, effects.compressor.makeup_gain_db)
    },
    [SYNTH_PARAM_COMPRESSOR_ATTACK_SECONDS] = {
        {SYNTH_PARAM_COMPRESSOR_ATTACK_SECONDS, "compressor_attack_seconds", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LOG2, SYNTH_COMPRESSOR_MIN_ATTACK_SECONDS, SYNTH_COMPRESSOR_MAX_ATTACK_SECONDS, 3.9f, 1},
        synth_get_compressor_attack_seconds, synth_set_compressor_attack_seconds, offsetof(synth_render_parameters, effects.compressor.attack_seconds)
    },
    [SYNTH_PARAM_COMPRESSOR_RELEASE_SECONDS] = {
        {SYNTH_PARAM_COMPRESSOR_RELEASE_SECONDS, "compressor_release_seconds", "seconds", SYNTH_PARAMETER_CONTINUOUS,
         SYNTH_DOMAIN_LOG2, SYNTH_COMPRESSOR_MIN_RELEASE_SECONDS, SYNTH_COMPRESSOR_MAX_RELEASE_SECONDS, 3.9f, 1},
        synth_get_compressor_release_seconds, synth_set_compressor_release_seconds, offsetof(synth_render_parameters, effects.compressor.release_seconds)
    },
};

// looks up immutable metadata by id; checks bounds before indexing the table
const synth_parameter_info *synth_parameter_info_at(synth_parameter_id id)
{
    return id >= 0 && id < SYNTH_PARAM_COUNT ? &parameters[id].info : NULL;
}

// finds the exact canonical name; callers can resolve names once before rendering
const synth_parameter_info *synth_parameter_info_by_name(const char *name)
{
    if (name != NULL) {
        for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) {
            if (strcmp(name, parameters[i].info.name) == 0) {
                return &parameters[i].info;
            }
        }
    }
    return NULL;
}

// limits a value to what its dsp module supports, then rounds stepped controls
float synth_parameter_clamp(synth_parameter_id id, float value, float sample_rate)
{
    const synth_parameter_info *info = synth_parameter_info_at(id);
    float low, high;
    if (info == NULL || !isfinite(value)) {
        return 0.0f;
    }
    low = info->min_value;
    high = info->max_value;
    if (id == SYNTH_PARAM_FILTER_CUTOFF) {
        // nyquist is half the sample rate also lower the minimum if an unusually
        // low sample rate would otherwise leave us with an inverted range
        high = fmaxf(0.0f, sample_rate * 0.5f);
        low = fminf(low, high);
    } else if (id == SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE) {
        // the reduced clock cannot sample faster than the host, or below its 1 hz floor
        high = fmaxf(low, sample_rate);
    }
    value = synth_clampf(value, low, high);
    // roundf keeps negative tuning symmetric: +1.5 becomes +2 and -1.5 becomes -2
    return info->type == SYNTH_PARAMETER_INTEGER ? roundf(value) : value;
}

// reads the owning module's base setting without keeping a second parameter store
float synth_get_parameter(const synth *s, synth_parameter_id id)
{
    return s != NULL && synth_parameter_info_at(id) != NULL ? parameters[id].get(s) : 0.0f;
}

// checks a generic base edit, then delegates to the owning setter and its normal side effects
int synth_set_parameter(synth *s, synth_parameter_id id, float value)
{
    if (s == NULL || synth_parameter_info_at(id) == NULL || !isfinite(value)) {
        return 0;
    }
    parameters[id].set(s, synth_parameter_clamp(id, value, s->sample_rate));
    return 1;
}

// the table addresses only typed control values in a private frame, never dsp state
static void write_render_value(synth_render_parameters *frame,
                               const parameter_entry *entry, float value)
{
    // work in byte offsets, then copy the correct field type memcpy avoids casting
    // the destination to an incompatible pointer type; integer fields stay integers
    unsigned char *destination = (unsigned char *)frame + entry->render_offset;
    if (entry->info.type == SYNTH_PARAMETER_INTEGER) {
        const int integer = (int)value;
        memcpy(destination, &integer, sizeof(integer));
    } else {
        memcpy(destination, &value, sizeof(value));
    }
}

// builds all continuously evaluated controls for one audio frame from the current bases
void synth_resolve_render_parameters(const synth *s, float lfo_value,
                                     synth_render_parameters *frame)
{
    // resolve the compound control first; component routes use its resulting base
    synth_flanger_params flanger = synth_flanger_get_params(&s->effects.flanger);
    const float intensity = synth_modulate_value(s, SYNTH_PARAM_FLANGER_INTENSITY,
                                                 flanger.intensity, lfo_value);
    synth_flanger_resolve_intensity(&flanger, intensity);

    for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) {
        const parameter_entry *entry = &parameters[i];
        float base, value;
        if (entry->render_offset == NO_RENDER_VALUE) {
            // adsr is captured at note-on; lfo controls drive the source itself
            continue;
        }
        // always start from the stored setting, never last frame's modulated value
        base = entry->get(s);
        if (i == SYNTH_PARAM_FLANGER_DEPTH) base = flanger.depth;
        if (i == SYNTH_PARAM_FLANGER_FEEDBACK) base = flanger.feedback;
        value = synth_modulate_value(s, (synth_parameter_id)i, base, lfo_value);
        if (i == SYNTH_PARAM_FLANGER_DEPTH || i == SYNTH_PARAM_FLANGER_FEEDBACK) {
            // intensity may have moved these beyond their limits even when their
            // own amount is zero clamp after both contributions have been added
            value = synth_parameter_clamp((synth_parameter_id)i, value, s->sample_rate);
        }
        write_render_value(frame, entry, value);
    }
}

// samples all four envelope controls at note-on, including the later release setting
synth_adsr synth_capture_modulated_adsr(const synth *s)
{
    // peek at the source: several notes started together must see the same phase
    const float value = synth_lfo_value(&s->lfo, s->sample_rate);
    synth_adsr adsr;
    // the voice stores this whole envelope release is captured now, not at note-off;
    // later manual adsr edits still replace it through the existing setter behavior
    adsr.attack_seconds = synth_modulate_value(s, SYNTH_PARAM_ATTACK, s->envelope.attack_seconds, value);
    adsr.decay_seconds = synth_modulate_value(s, SYNTH_PARAM_DECAY, s->envelope.decay_seconds, value);
    adsr.sustain_level = synth_modulate_value(s, SYNTH_PARAM_SUSTAIN, s->envelope.sustain_level, value);
    adsr.release_seconds = synth_modulate_value(s, SYNTH_PARAM_RELEASE, s->envelope.release_seconds, value);
    return adsr;
}
