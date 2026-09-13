#include "midi/midi_mapping_internal.h"

#include <string.h>
#include <stdio.h>

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
    "ring_mod",
    "chorus",
    "eq",
    "compressor"
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
            {1, MIDI_MAPPING_EFFECT_EQ},
            {1, MIDI_MAPPING_EFFECT_COMPRESSOR},
            {1, MIDI_MAPPING_EFFECT_CHORUS}
        }
    };

static const midi_mapping_effect_macro_route effect_macro_routes
    [MIDI_MAPPING_EFFECT_COUNT][MIDI_MAPPING_EFFECT_MACRO_COUNT] = {
        {
            {1, SYNTH_PARAM_SATURATION_DRIVE},
            {0, SYNTH_PARAM_SATURATION_DRIVE},
            {1, SYNTH_PARAM_SATURATION_MIX}
        },
        {
            {1, SYNTH_PARAM_DISTORTION_DRIVE},
            {0, SYNTH_PARAM_DISTORTION_DRIVE},
            {1, SYNTH_PARAM_DISTORTION_MIX}
        },
        {
            {1, SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE},
            {1, SYNTH_PARAM_BITCRUSHER_BITS},
            {1, SYNTH_PARAM_BITCRUSHER_MIX}
        },
        {
            {1, SYNTH_PARAM_DELAY_TIME},
            {1, SYNTH_PARAM_DELAY_FEEDBACK},
            {1, SYNTH_PARAM_DELAY_MIX}
        },
        {
            {1, SYNTH_PARAM_PLATE_REVERB_DECAY},
            {1, SYNTH_PARAM_PLATE_REVERB_DAMPING},
            {1, SYNTH_PARAM_PLATE_REVERB_MIX}
        },
        {
            {1, SYNTH_PARAM_FLANGER_RATE},
            {1, SYNTH_PARAM_FLANGER_INTENSITY},
            {1, SYNTH_PARAM_FLANGER_MIX}
        },
        {
            {1, SYNTH_PARAM_RING_MOD_FREQUENCY},
            {1, SYNTH_PARAM_RING_MOD_RECTIFY},
            {1, SYNTH_PARAM_RING_MOD_MIX}
        },
        {
            {1, SYNTH_PARAM_CHORUS_RATE},
            {1, SYNTH_PARAM_CHORUS_DEPTH},
            {1, SYNTH_PARAM_CHORUS_MIX}
        },
        {
            {1, SYNTH_PARAM_EQ_LOW},
            {1, SYNTH_PARAM_EQ_MID},
            {1, SYNTH_PARAM_EQ_HIGH}
        },
        {
            {1, SYNTH_PARAM_COMPRESSOR_THRESHOLD},
            {1, SYNTH_PARAM_COMPRESSOR_RATIO},
            {1, SYNTH_PARAM_COMPRESSOR_MAKEUP_GAIN}
        }
    };

// controller preferences belong to this adapter, not the engine catalog
typedef struct controller_defaults {
    midi_mapping_scale scale;
    float min_value;
    float max_value;
} controller_defaults;

static const controller_defaults defaults[SYNTH_PARAM_COUNT] = {
    // each row is {knob scale, minimum, maximum} these are controller travel
    // preferences, not engine bounds or lfo spans; a config may choose others
    [SYNTH_PARAM_ATTACK] = {MIDI_MAPPING_SCALE_LINEAR, 0.001f, 2.0f},
    [SYNTH_PARAM_DECAY] = {MIDI_MAPPING_SCALE_LINEAR, 0.001f, 2.0f},
    [SYNTH_PARAM_SUSTAIN] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_RELEASE] = {MIDI_MAPPING_SCALE_LINEAR, 0.001f, 3.0f},
    [SYNTH_PARAM_MASTER_GAIN] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_FILTER_CUTOFF] = {MIDI_MAPPING_SCALE_LOG, 20.0f, 20000.0f},
    [SYNTH_PARAM_FILTER_POLES] = {MIDI_MAPPING_SCALE_STEP, 1.0f, 8.0f},
    [SYNTH_PARAM_OSCILLATOR_MORPH] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_FIRST_OSCILLATOR_GAIN] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_SECOND_OSCILLATOR_GAIN] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_SECOND_OSCILLATOR_MORPH] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_SECOND_OSCILLATOR_OCTAVE] = {MIDI_MAPPING_SCALE_STEP, -1.0f, 1.0f},
    [SYNTH_PARAM_SECOND_OSCILLATOR_PITCH] = {MIDI_MAPPING_SCALE_STEP, -6.0f, 6.0f},
    [SYNTH_PARAM_SECOND_OSCILLATOR_FINE_TUNE] = {MIDI_MAPPING_SCALE_LINEAR, -50.0f, 50.0f},
    [SYNTH_PARAM_STEREO_SPREAD] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_LFO_RATE] = {MIDI_MAPPING_SCALE_LOG, 0.05f, 20.0f},
    [SYNTH_PARAM_LFO_SHAPE_MORPH] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_LFO_DEPTH] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_SATURATION_DRIVE] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_SATURATION_MIN_DRIVE, SYNTH_SATURATION_MAX_DRIVE},
    [SYNTH_PARAM_SATURATION_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_DISTORTION_DRIVE] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_DISTORTION_MIN_DRIVE, SYNTH_DISTORTION_MAX_DRIVE},
    [SYNTH_PARAM_DISTORTION_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE] = {MIDI_MAPPING_SCALE_LOG, 100.0f, 48000.0f},
    [SYNTH_PARAM_BITCRUSHER_BITS] = {MIDI_MAPPING_SCALE_STEP, 1.0f, 16.0f},
    [SYNTH_PARAM_BITCRUSHER_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_FLANGER_RATE] = {MIDI_MAPPING_SCALE_LOG, SYNTH_FLANGER_MIN_RATE_HZ, SYNTH_FLANGER_MAX_RATE_HZ},
    [SYNTH_PARAM_FLANGER_INTENSITY] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_FLANGER_DEPTH] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_FLANGER_FEEDBACK] = {MIDI_MAPPING_SCALE_LINEAR, -SYNTH_FLANGER_MAX_FEEDBACK, SYNTH_FLANGER_MAX_FEEDBACK},
    [SYNTH_PARAM_FLANGER_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_FLANGER_MANUAL] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_FLANGER_MIN_MANUAL_SECONDS, SYNTH_FLANGER_MAX_MANUAL_SECONDS},
    [SYNTH_PARAM_RING_MOD_FREQUENCY] = {MIDI_MAPPING_SCALE_LOG, SYNTH_RING_MOD_MIN_FREQUENCY_HZ, SYNTH_RING_MOD_MAX_FREQUENCY_HZ},
    [SYNTH_PARAM_RING_MOD_RECTIFY] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_RING_MOD_MIN_RECTIFY, SYNTH_RING_MOD_MAX_RECTIFY},
    [SYNTH_PARAM_RING_MOD_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_CHORUS_RATE] = {MIDI_MAPPING_SCALE_LOG, SYNTH_CHORUS_MIN_RATE_HZ, SYNTH_CHORUS_MAX_RATE_HZ},
    [SYNTH_PARAM_CHORUS_DEPTH] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_CHORUS_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_CHORUS_WIDTH] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_CHORUS_DELAY] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_CHORUS_MIN_DELAY_SECONDS, SYNTH_CHORUS_MAX_DELAY_SECONDS},
    [SYNTH_PARAM_CHORUS_FEEDBACK] = {MIDI_MAPPING_SCALE_LINEAR, -SYNTH_CHORUS_MAX_FEEDBACK, SYNTH_CHORUS_MAX_FEEDBACK},
    [SYNTH_PARAM_EQ_LOW] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_EQ_MIN_GAIN_DB, SYNTH_EQ_MAX_GAIN_DB},
    [SYNTH_PARAM_EQ_MID] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_EQ_MIN_GAIN_DB, SYNTH_EQ_MAX_GAIN_DB},
    [SYNTH_PARAM_EQ_HIGH] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_EQ_MIN_GAIN_DB, SYNTH_EQ_MAX_GAIN_DB},
    [SYNTH_PARAM_DELAY_TIME] = {MIDI_MAPPING_SCALE_LINEAR, 0.001f, 2.0f},
    [SYNTH_PARAM_DELAY_FEEDBACK] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 0.95f},
    [SYNTH_PARAM_DELAY_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_PLATE_REVERB_DECAY] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS, SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS},
    [SYNTH_PARAM_PLATE_REVERB_DAMPING] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_PLATE_REVERB_MIX] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, 1.0f},
    [SYNTH_PARAM_PLATE_REVERB_PREDELAY] = {MIDI_MAPPING_SCALE_LINEAR, 0.0f, SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS},
    [SYNTH_PARAM_COMPRESSOR_THRESHOLD] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_COMPRESSOR_MIN_THRESHOLD_DB, SYNTH_COMPRESSOR_MAX_THRESHOLD_DB},
    [SYNTH_PARAM_COMPRESSOR_RATIO] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_COMPRESSOR_MIN_RATIO, SYNTH_COMPRESSOR_MAX_RATIO},
    [SYNTH_PARAM_COMPRESSOR_MAKEUP_GAIN] = {MIDI_MAPPING_SCALE_LINEAR, SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB, SYNTH_COMPRESSOR_MAX_MAKEUP_GAIN_DB},
    [SYNTH_PARAM_COMPRESSOR_ATTACK_SECONDS] = {MIDI_MAPPING_SCALE_LOG, SYNTH_COMPRESSOR_MIN_ATTACK_SECONDS, SYNTH_COMPRESSOR_MAX_ATTACK_SECONDS},
    [SYNTH_PARAM_COMPRESSOR_RELEASE_SECONDS] = {MIDI_MAPPING_SCALE_LOG, SYNTH_COMPRESSOR_MIN_RELEASE_SECONDS, SYNTH_COMPRESSOR_MAX_RELEASE_SECONDS},
};

// combines engine identity with desktop knob defaults in caller-owned storage
// the target kind distinguishes a base knob from an amount knob for that same parameter
int midi_mapping_parameter_info_for(midi_mapping_parameter parameter,
    midi_mapping_target_kind kind, midi_mapping_parameter_info *info)
{
    const synth_parameter_info *core = synth_parameter_info_at(parameter);
    if (core == NULL || info == NULL ||
        (kind != MIDI_MAPPING_TARGET_BASE && kind != MIDI_MAPPING_TARGET_LFO_AMOUNT) ||
        (kind == MIDI_MAPPING_TARGET_LFO_AMOUNT && !core->modulatable)) {
        return 0;
    }
    info->parameter = parameter;
    info->target_kind = kind;
    snprintf(info->name, sizeof(info->name), "%s%s",
             kind == MIDI_MAPPING_TARGET_LFO_AMOUNT ? "lfo_amount." : "", core->name);
    // every amount knob gets the same signed defaults, regardless of its target's
    // native units this binds a control; it does not initialize the engine amount
    info->default_scale = kind == MIDI_MAPPING_TARGET_LFO_AMOUNT
        ? MIDI_MAPPING_SCALE_LINEAR : defaults[parameter].scale;
    info->default_min_value = kind == MIDI_MAPPING_TARGET_LFO_AMOUNT
        ? -1.0f : defaults[parameter].min_value;
    info->default_max_value = kind == MIDI_MAPPING_TARGET_LFO_AMOUNT
        ? 1.0f : defaults[parameter].max_value;
    return 1;
}

// counts base controls plus one amount control for every eligible lfo destination
size_t midi_mapping_parameter_count(void)
{
    size_t count = SYNTH_PARAM_COUNT;
    for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) {
        count += synth_parameter_info_at((synth_parameter_id)i)->modulatable != 0;
    }
    return count;
}

// enumerates base controls first, followed by amounts for eligible parameters only
int midi_mapping_parameter_info_at(size_t index, midi_mapping_parameter_info *info)
{
    if (index < SYNTH_PARAM_COUNT) {
        return midi_mapping_parameter_info_for((synth_parameter_id)index, MIDI_MAPPING_TARGET_BASE, info);
    }
    // now count only eligible destinations; excluded global lfo controls must
    // not leave holes or create recursive amount controls in the displayed list
    index -= SYNTH_PARAM_COUNT;
    for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) {
        if (synth_parameter_info_at((synth_parameter_id)i)->modulatable) {
            if (index == 0) {
                return midi_mapping_parameter_info_for((synth_parameter_id)i, MIDI_MAPPING_TARGET_LFO_AMOUNT, info);
            }
            --index;
        }
    }
    return 0;
}

// splits the optional route-amount prefix from the engine parameter name and validates both
int midi_mapping_parameter_info_by_name(const char *name, midi_mapping_parameter_info *info)
{
    midi_mapping_target_kind kind = MIDI_MAPPING_TARGET_BASE;
    const synth_parameter_info *core;
    if (name == NULL) return 0;
    // the 11 characters in "lfo_amount." belong to config syntax, not the engine name
    if (strncmp(name, "lfo_amount.", 11) == 0) {
        kind = MIDI_MAPPING_TARGET_LFO_AMOUNT;
        name += 11;
    }
    core = synth_parameter_info_by_name(name);
    // the engine lookup also rejects unknown names and a second nested prefix;
    // info_for then rejects a known parameter that is excluded as a destination
    return core != NULL && midi_mapping_parameter_info_for(core->id, kind, info);
}

// returns the base parameter name for display; amount formatting is handled by the caller
const char *midi_mapping_parameter_name(midi_mapping_parameter parameter)
{
    const synth_parameter_info *info = synth_parameter_info_at(parameter);
    return info != NULL ? info->name : "unknown";
}

// finds a named chord pad in the adapter's own table, separate from engine parameters
const midi_mapping_chord_entry *midi_mapping_find_chord_by_name(const char *name)
{
    for (size_t i = 0; i < sizeof(chord_entries) / sizeof(chord_entries[0]); ++i) {
        if (strcmp(chord_entries[i].name, name) == 0) return &chord_entries[i];
    }
    return NULL;
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

// returns the config spelling for a scale
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

// parses the config spelling for a scale
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
