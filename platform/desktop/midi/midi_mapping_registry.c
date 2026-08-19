#include "midi/midi_mapping_internal.h"

#include <string.h>

static const midi_mapping_chord_entry chord_entries[] = {
    {MIDI_CHORD_MODE_PAD_DIMINISHED, "chord_diminished"},
    {MIDI_CHORD_MODE_PAD_MINOR, "chord_minor"},
    {MIDI_CHORD_MODE_PAD_MAJOR, "chord_major"},
    {MIDI_CHORD_MODE_PAD_SUSPENDED, "chord_suspended"},
    {MIDI_CHORD_MODE_PAD_SIXTH, "chord_6"},
    {MIDI_CHORD_MODE_PAD_MINOR_SEVENTH, "chord_minor_7"},
    {MIDI_CHORD_MODE_PAD_MAJOR_SEVENTH, "chord_major_7"},
    {MIDI_CHORD_MODE_PAD_NINTH, "chord_9"}
};

static float get_attack(const synth *s)
{
    return synth_get_adsr(s).attack_seconds;
}

static float get_decay(const synth *s)
{
    return synth_get_adsr(s).decay_seconds;
}

static float get_sustain(const synth *s)
{
    return synth_get_adsr(s).sustain_level;
}

static float get_release(const synth *s)
{
    return synth_get_adsr(s).release_seconds;
}

// adsr routes update one field, then hand the whole envelope back to the synth.
static void set_attack(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.attack_seconds = value;
    synth_set_adsr(s, adsr);
}

static void set_decay(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.decay_seconds = value;
    synth_set_adsr(s, adsr);
}

static void set_sustain(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.sustain_level = value;
    synth_set_adsr(s, adsr);
}

static void set_release(synth *s, float value)
{
    synth_adsr adsr = synth_get_adsr(s);

    adsr.release_seconds = value;
    synth_set_adsr(s, adsr);
}

static float get_second_oscillator_octave(const synth *s)
{
    return (float)synth_get_second_oscillator_octave(s);
}

static float get_second_oscillator_pitch(const synth *s)
{
    return (float)synth_get_second_oscillator_pitch(s);
}

static float get_filter_poles(const synth *s)
{
    return (float)synth_get_filter_poles(s);
}

static void set_filter_poles(synth *s, float value)
{
    synth_set_filter_poles(s, (int)value);
}

static void set_second_oscillator_octave(synth *s, float value)
{
    synth_set_second_oscillator_octave(s, (int)value);
}

static void set_second_oscillator_pitch(synth *s, float value)
{
    synth_set_second_oscillator_pitch(s, (int)value);
}

static float get_bitcrusher_bits(const synth *s)
{
    return (float)synth_get_bitcrusher_bits(s);
}

static void set_bitcrusher_bits(synth *s, float value)
{
    synth_set_bitcrusher_bits(s, (int)value);
}

static const midi_mapping_parameter_entry parameter_entries[] = {
    {MIDI_MAPPING_PARAM_ATTACK, "attack", get_attack, set_attack, MIDI_MAPPING_SCALE_LINEAR, 0.001f, 2.0f},
    {MIDI_MAPPING_PARAM_DECAY, "decay", get_decay, set_decay, MIDI_MAPPING_SCALE_LINEAR, 0.001f, 2.0f},
    {MIDI_MAPPING_PARAM_SUSTAIN, "sustain", get_sustain, set_sustain, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_RELEASE, "release", get_release, set_release, MIDI_MAPPING_SCALE_LINEAR, 0.001f, 3.0f},
    {MIDI_MAPPING_PARAM_MASTER_GAIN, "master_gain", synth_get_master_gain, synth_set_master_gain, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_FILTER_CUTOFF, "filter_cutoff", synth_get_filter_cutoff, synth_set_filter_cutoff, MIDI_MAPPING_SCALE_LOG, 20.0f, 20000.0f},
    {MIDI_MAPPING_PARAM_FILTER_POLES, "filter_poles", get_filter_poles, set_filter_poles, MIDI_MAPPING_SCALE_STEP, 1.0f, 8.0f},
    {MIDI_MAPPING_PARAM_OSCILLATOR_MORPH, "oscillator_morph", synth_get_oscillator_morph, synth_set_oscillator_morph, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN, "first_oscillator_gain", synth_get_first_oscillator_gain, synth_set_first_oscillator_gain, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN, "second_oscillator_gain", synth_get_second_oscillator_gain, synth_set_second_oscillator_gain, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH, "second_oscillator_morph", synth_get_second_oscillator_morph, synth_set_second_oscillator_morph, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE, "second_oscillator_octave", get_second_oscillator_octave, set_second_oscillator_octave, MIDI_MAPPING_SCALE_STEP, -1.0f, 1.0f},
    {MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH, "second_oscillator_pitch", get_second_oscillator_pitch, set_second_oscillator_pitch, MIDI_MAPPING_SCALE_STEP, -6.0f, 6.0f},
    {MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE, "second_oscillator_fine_tune", synth_get_second_oscillator_fine_tune, synth_set_second_oscillator_fine_tune, MIDI_MAPPING_SCALE_LINEAR, -50.0f, 50.0f},
    {MIDI_MAPPING_PARAM_STEREO_SPREAD, "stereo_spread", synth_get_stereo_spread, synth_set_stereo_spread, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_LFO_RATE, "lfo_rate", synth_get_lfo_rate, synth_set_lfo_rate, MIDI_MAPPING_SCALE_LOG, 0.05f, 20.0f},
    {MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH, "lfo_shape_morph", synth_get_lfo_shape_morph, synth_set_lfo_shape_morph, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_LFO_DEPTH, "lfo_depth", synth_get_lfo_depth, synth_set_lfo_depth, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_MORPH_AMOUNT, "lfo_first_oscillator_morph_amount", synth_get_lfo_first_oscillator_morph_amount, synth_set_lfo_first_oscillator_morph_amount, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_MORPH_AMOUNT, "lfo_second_oscillator_morph_amount", synth_get_lfo_second_oscillator_morph_amount, synth_set_lfo_second_oscillator_morph_amount, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT, "lfo_first_oscillator_gain_amount", synth_get_lfo_first_oscillator_gain_amount, synth_set_lfo_first_oscillator_gain_amount, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT, "lfo_second_oscillator_gain_amount", synth_get_lfo_second_oscillator_gain_amount, synth_set_lfo_second_oscillator_gain_amount, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT, "lfo_filter_amount", synth_get_lfo_filter_amount, synth_set_lfo_filter_amount, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_SATURATION_DRIVE, "saturation_drive", synth_get_saturation_drive, synth_set_saturation_drive, MIDI_MAPPING_SCALE_LINEAR, SYNTH_SATURATION_MIN_DRIVE, SYNTH_SATURATION_MAX_DRIVE},
    {MIDI_MAPPING_PARAM_SATURATION_MIX, "saturation_mix", synth_get_saturation_mix, synth_set_saturation_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_DISTORTION_DRIVE, "distortion_drive", synth_get_distortion_drive, synth_set_distortion_drive, MIDI_MAPPING_SCALE_LINEAR, SYNTH_DISTORTION_MIN_DRIVE, SYNTH_DISTORTION_MAX_DRIVE},
    {MIDI_MAPPING_PARAM_DISTORTION_MIX, "distortion_mix", synth_get_distortion_mix, synth_set_distortion_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_BITCRUSHER_SAMPLE_RATE, "bitcrusher_sample_rate", synth_get_bitcrusher_sample_rate, synth_set_bitcrusher_sample_rate, MIDI_MAPPING_SCALE_LOG, 100.0f, 48000.0f},
    {MIDI_MAPPING_PARAM_BITCRUSHER_BITS, "bitcrusher_bits", get_bitcrusher_bits, set_bitcrusher_bits, MIDI_MAPPING_SCALE_STEP, 1.0f, 16.0f},
    {MIDI_MAPPING_PARAM_BITCRUSHER_MIX, "bitcrusher_mix", synth_get_bitcrusher_mix, synth_set_bitcrusher_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_DELAY_TIME, "delay_time", synth_get_delay_time, synth_set_delay_time, MIDI_MAPPING_SCALE_LINEAR, 0.001f, 2.0f},
    {MIDI_MAPPING_PARAM_DELAY_FEEDBACK, "delay_feedback", synth_get_delay_feedback, synth_set_delay_feedback, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 0.95f},
    {MIDI_MAPPING_PARAM_DELAY_MIX, "delay_mix", synth_get_delay_mix, synth_set_delay_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f}
};

const midi_mapping_parameter_entry *midi_mapping_find_parameter_by_name(const char *name)
{
    for (size_t i = 0; i < sizeof(parameter_entries) / sizeof(parameter_entries[0]); ++i) {
        if (strcmp(parameter_entries[i].name, name) == 0) {
            return &parameter_entries[i];
        }
    }

    return 0;
}

const midi_mapping_chord_entry *midi_mapping_find_chord_by_name(const char *name)
{
    for (size_t i = 0; i < sizeof(chord_entries) / sizeof(chord_entries[0]); ++i) {
        if (strcmp(chord_entries[i].name, name) == 0) {
            return &chord_entries[i];
        }
    }

    return 0;
}

const midi_mapping_parameter_entry *midi_mapping_find_parameter(midi_mapping_parameter parameter)
{
    for (size_t i = 0; i < sizeof(parameter_entries) / sizeof(parameter_entries[0]); ++i) {
        if (parameter_entries[i].parameter == parameter) {
            return &parameter_entries[i];
        }
    }

    return 0;
}

void midi_mapping_fill_parameter_info(
    const midi_mapping_parameter_entry *entry,
    midi_mapping_parameter_info *info)
{
    info->parameter = entry->parameter;
    info->name = entry->name;
    info->default_scale = entry->default_scale;
    info->default_min_value = entry->default_min_value;
    info->default_max_value = entry->default_max_value;
}

// returns how many synth parameters can be mapped.
size_t midi_mapping_parameter_count(void)
{
    return sizeof(parameter_entries) / sizeof(parameter_entries[0]);
}

// returns metadata for a mappable synth parameter by index.
const midi_mapping_parameter_info *midi_mapping_parameter_info_at(size_t index)
{
    static midi_mapping_parameter_info info;

    if (index >= midi_mapping_parameter_count()) {
        return 0;
    }

    midi_mapping_fill_parameter_info(&parameter_entries[index], &info);
    return &info;
}

// returns metadata for a mappable synth parameter by name.
const midi_mapping_parameter_info *midi_mapping_parameter_info_by_name(const char *name)
{
    static midi_mapping_parameter_info info;
    const midi_mapping_parameter_entry *entry = midi_mapping_find_parameter_by_name(name);

    if (entry == 0) {
        return 0;
    }

    midi_mapping_fill_parameter_info(entry, &info);
    return &info;
}

// returns the readable name for a mapped synth parameter.
const char *midi_mapping_parameter_name(midi_mapping_parameter parameter)
{
    const midi_mapping_parameter_entry *entry = midi_mapping_find_parameter(parameter);

    return entry != 0 ? entry->name : "unknown";
}

const char *midi_mapping_chord_pad_name(midi_chord_mode_pad pad)
{
    for (size_t i = 0; i < sizeof(chord_entries) / sizeof(chord_entries[0]); ++i) {
        if (chord_entries[i].pad == pad) {
            return chord_entries[i].name;
        }
    }

    return "unknown_chord_pad";
}

// returns the config spelling for a scale.
const char *midi_mapping_scale_name(midi_mapping_scale scale)
{
    switch (scale) {
        case MIDI_MAPPING_SCALE_LOG:
            return "log";

        case MIDI_MAPPING_SCALE_STEP:
            return "step";

        case MIDI_MAPPING_SCALE_LINEAR:
        default:
            return "linear";
    }
}

// parses the config spelling for a scale.
int midi_mapping_parse_scale_name(const char *name, midi_mapping_scale *scale)
{
    if (strcmp(name, "linear") == 0) {
        *scale = MIDI_MAPPING_SCALE_LINEAR;
        return 1;
    }

    if (strcmp(name, "step") == 0) {
        *scale = MIDI_MAPPING_SCALE_STEP;
        return 1;
    }

    if (strcmp(name, "log") == 0) {
        *scale = MIDI_MAPPING_SCALE_LOG;
        return 1;
    }

    return 0;
}
