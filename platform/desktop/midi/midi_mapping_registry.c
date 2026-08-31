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

static const char *effect_names[] = {
    "saturation",
    "distortion",
    "bitcrusher",
    "delay",
    "plate_reverb",
    "flanger",
    "ring_mod"
};

static const char *effect_selector_names[MIDI_MAPPING_EFFECT_BANK_COUNT] = {
    "effect_selector_1",
    "effect_selector_2"
};

static const char *effect_macro_names
    [MIDI_MAPPING_EFFECT_BANK_COUNT][MIDI_MAPPING_EFFECT_MACRO_COUNT] = {
        {
            "effect_1_macro_1",
            "effect_1_macro_2",
            "effect_1_macro_3"
        },
        {
            "effect_2_macro_1",
            "effect_2_macro_2",
            "effect_2_macro_3"
        }
};

typedef struct midi_mapping_effect_macro_route {
    int enabled;
    midi_mapping_parameter parameter;
} midi_mapping_effect_macro_route;

typedef struct midi_mapping_effect_bank_page_entry {
    int enabled;
    midi_mapping_effect effect;
} midi_mapping_effect_bank_page_entry;

static const midi_mapping_effect_bank_page_entry effect_bank_pages
    [MIDI_MAPPING_EFFECT_BANK_COUNT][MIDI_MAPPING_EFFECTS_PER_BANK] = {
        {
            {1, MIDI_MAPPING_EFFECT_SATURATION},
            {1, MIDI_MAPPING_EFFECT_DISTORTION},
            {1, MIDI_MAPPING_EFFECT_BITCRUSHER},
            {1, MIDI_MAPPING_EFFECT_DELAY},
            {1, MIDI_MAPPING_EFFECT_PLATE_REVERB}
        },
        {
            {1, MIDI_MAPPING_EFFECT_FLANGER},
            {1, MIDI_MAPPING_EFFECT_RING_MOD},
            {0, MIDI_MAPPING_EFFECT_SATURATION},
            {0, MIDI_MAPPING_EFFECT_SATURATION},
            {0, MIDI_MAPPING_EFFECT_SATURATION}
        }
    };

static const midi_mapping_effect_macro_route effect_macro_routes
    [MIDI_MAPPING_EFFECT_COUNT][MIDI_MAPPING_EFFECT_MACRO_COUNT] = {
        {
            {1, MIDI_MAPPING_PARAM_SATURATION_DRIVE},
            {0, MIDI_MAPPING_PARAM_SATURATION_DRIVE},
            {1, MIDI_MAPPING_PARAM_SATURATION_MIX}
        },
        {
            {1, MIDI_MAPPING_PARAM_DISTORTION_DRIVE},
            {0, MIDI_MAPPING_PARAM_DISTORTION_DRIVE},
            {1, MIDI_MAPPING_PARAM_DISTORTION_MIX}
        },
        {
            {1, MIDI_MAPPING_PARAM_BITCRUSHER_SAMPLE_RATE},
            {1, MIDI_MAPPING_PARAM_BITCRUSHER_BITS},
            {1, MIDI_MAPPING_PARAM_BITCRUSHER_MIX}
        },
        {
            {1, MIDI_MAPPING_PARAM_DELAY_TIME},
            {1, MIDI_MAPPING_PARAM_DELAY_FEEDBACK},
            {1, MIDI_MAPPING_PARAM_DELAY_MIX}
        },
        {
            {1, MIDI_MAPPING_PARAM_PLATE_REVERB_DECAY},
            {1, MIDI_MAPPING_PARAM_PLATE_REVERB_DAMPING},
            {1, MIDI_MAPPING_PARAM_PLATE_REVERB_MIX}
        },
        {
            {1, MIDI_MAPPING_PARAM_FLANGER_RATE},
            {1, MIDI_MAPPING_PARAM_FLANGER_INTENSITY},
            {1, MIDI_MAPPING_PARAM_FLANGER_MIX}
        },
        {
            {1, MIDI_MAPPING_PARAM_RING_MOD_FREQUENCY},
            {1, MIDI_MAPPING_PARAM_RING_MOD_RECTIFY},
            {1, MIDI_MAPPING_PARAM_RING_MOD_MIX}
        }
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
    {MIDI_MAPPING_PARAM_FLANGER_RATE, "flanger_rate", synth_get_flanger_rate, synth_set_flanger_rate, MIDI_MAPPING_SCALE_LOG, SYNTH_FLANGER_MIN_RATE_HZ, SYNTH_FLANGER_MAX_RATE_HZ},
    {MIDI_MAPPING_PARAM_FLANGER_INTENSITY, "flanger_intensity", synth_get_flanger_intensity, synth_set_flanger_intensity, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_FLANGER_DEPTH, "flanger_depth", synth_get_flanger_depth, synth_set_flanger_depth, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_FLANGER_FEEDBACK, "flanger_feedback", synth_get_flanger_feedback, synth_set_flanger_feedback, MIDI_MAPPING_SCALE_LINEAR, -SYNTH_FLANGER_MAX_FEEDBACK, SYNTH_FLANGER_MAX_FEEDBACK},
    {MIDI_MAPPING_PARAM_FLANGER_MIX, "flanger_mix", synth_get_flanger_mix, synth_set_flanger_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_FLANGER_MANUAL, "flanger_manual", synth_get_flanger_manual, synth_set_flanger_manual, MIDI_MAPPING_SCALE_LINEAR, SYNTH_FLANGER_MIN_MANUAL_SECONDS, SYNTH_FLANGER_MAX_MANUAL_SECONDS},
    {MIDI_MAPPING_PARAM_RING_MOD_FREQUENCY, "ring_mod_frequency", synth_get_ring_mod_frequency, synth_set_ring_mod_frequency, MIDI_MAPPING_SCALE_LOG, SYNTH_RING_MOD_MIN_FREQUENCY_HZ, SYNTH_RING_MOD_MAX_FREQUENCY_HZ},
    {MIDI_MAPPING_PARAM_RING_MOD_RECTIFY, "ring_mod_rectify", synth_get_ring_mod_rectify, synth_set_ring_mod_rectify, MIDI_MAPPING_SCALE_LINEAR, SYNTH_RING_MOD_MIN_RECTIFY, SYNTH_RING_MOD_MAX_RECTIFY},
    {MIDI_MAPPING_PARAM_RING_MOD_MIX, "ring_mod_mix", synth_get_ring_mod_mix, synth_set_ring_mod_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_DELAY_TIME, "delay_time", synth_get_delay_time, synth_set_delay_time, MIDI_MAPPING_SCALE_LINEAR, 0.001f, 2.0f},
    {MIDI_MAPPING_PARAM_DELAY_FEEDBACK, "delay_feedback", synth_get_delay_feedback, synth_set_delay_feedback, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 0.95f},
    {MIDI_MAPPING_PARAM_DELAY_MIX, "delay_mix", synth_get_delay_mix, synth_set_delay_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_PLATE_REVERB_DECAY, "plate_reverb_decay", synth_get_plate_reverb_decay, synth_set_plate_reverb_decay, MIDI_MAPPING_SCALE_LINEAR, SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS, SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS},
    {MIDI_MAPPING_PARAM_PLATE_REVERB_DAMPING, "plate_reverb_damping", synth_get_plate_reverb_damping, synth_set_plate_reverb_damping, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_PLATE_REVERB_MIX, "plate_reverb_mix", synth_get_plate_reverb_mix, synth_set_plate_reverb_mix, MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    {MIDI_MAPPING_PARAM_PLATE_REVERB_PREDELAY, "plate_reverb_predelay", synth_get_plate_reverb_predelay, synth_set_plate_reverb_predelay, MIDI_MAPPING_SCALE_LINEAR, 0.0f, SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS}
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

const char *midi_mapping_effect_name(midi_mapping_effect effect)
{
    if (effect < 0 || effect >= MIDI_MAPPING_EFFECT_COUNT) {
        return "unknown_effect";
    }

    return effect_names[effect];
}

const char *midi_mapping_effect_selector_name(size_t bank_index)
{
    if (bank_index >= MIDI_MAPPING_EFFECT_BANK_COUNT) {
        return "unknown_effect_selector";
    }

    return effect_selector_names[bank_index];
}

const char *midi_mapping_effect_macro_name(size_t bank_index, size_t macro_index)
{
    if (bank_index >= MIDI_MAPPING_EFFECT_BANK_COUNT) {
        return "unknown_effect_macro";
    }

    if (macro_index >= MIDI_MAPPING_EFFECT_MACRO_COUNT) {
        return "unknown_effect_macro";
    }

    return effect_macro_names[bank_index][macro_index];
}

int midi_mapping_effect_bank_page(
    size_t bank_index,
    size_t page_index,
    midi_mapping_effect *effect)
{
    const midi_mapping_effect_bank_page_entry *page;

    if (bank_index >= MIDI_MAPPING_EFFECT_BANK_COUNT ||
        page_index >= MIDI_MAPPING_EFFECTS_PER_BANK) {
        return 0;
    }

    page = &effect_bank_pages[bank_index][page_index];
    if (!page->enabled) {
        return 0;
    }

    if (effect != 0) {
        *effect = page->effect;
    }

    return 1;
}

int midi_mapping_effect_macro_parameter(
    midi_mapping_effect effect,
    size_t macro_index,
    midi_mapping_parameter *parameter)
{
    const midi_mapping_effect_macro_route *route;

    if (effect < 0 ||
        effect >= MIDI_MAPPING_EFFECT_COUNT ||
        macro_index >= MIDI_MAPPING_EFFECT_MACRO_COUNT) {
        return 0;
    }

    route = &effect_macro_routes[effect][macro_index];
    if (!route->enabled) {
        return 0;
    }

    if (parameter != 0) {
        *parameter = route->parameter;
    }

    return 1;
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
