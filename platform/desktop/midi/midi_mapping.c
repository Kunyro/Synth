#include "midi/midi_mapping.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clamps a float without depending on synth internals from the desktop layer.
static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

// writes a formatted load error when there is room for one.
static void set_error(char *error, size_t error_size, int line_number, const char *message)
{
    if (error != 0 && error_size > 0) {
        if (line_number > 0) {
            snprintf(error, error_size, "line %d: %s", line_number, message);
        } else {
            snprintf(error, error_size, "%s", message);
        }
    }
}

// trims whitespace from both ends of a string.
static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        ++text;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return text;
}

// copies text into a fixed size string buffer.
static void copy_string(char *target, size_t target_size, const char *source)
{
    if (target_size == 0) {
        return;
    }

    snprintf(target, target_size, "%s", source);
}

typedef float (*midi_mapping_parameter_getter)(const synth *s);
typedef void (*midi_mapping_parameter_setter)(synth *s, float value);

typedef struct midi_mapping_parameter_entry {
    midi_mapping_parameter parameter;
    const char *name;
    midi_mapping_parameter_getter get;
    midi_mapping_parameter_setter set;
} midi_mapping_parameter_entry;

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

static const midi_mapping_parameter_entry parameter_entries[] = {
    {MIDI_MAPPING_PARAM_ATTACK, "attack", get_attack, set_attack},
    {MIDI_MAPPING_PARAM_DECAY, "decay", get_decay, set_decay},
    {MIDI_MAPPING_PARAM_SUSTAIN, "sustain", get_sustain, set_sustain},
    {MIDI_MAPPING_PARAM_RELEASE, "release", get_release, set_release},
    {MIDI_MAPPING_PARAM_MASTER_GAIN, "master_gain", synth_get_master_gain, synth_set_master_gain},
    {
        MIDI_MAPPING_PARAM_FILTER_CUTOFF,
        "filter_cutoff",
        synth_get_filter_cutoff,
        synth_set_filter_cutoff
    },
    {MIDI_MAPPING_PARAM_FILTER_POLES, "filter_poles", get_filter_poles, set_filter_poles},
    {
        MIDI_MAPPING_PARAM_OSCILLATOR_MORPH,
        "oscillator_morph",
        synth_get_oscillator_morph,
        synth_set_oscillator_morph
    },
    {
        MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN,
        "first_oscillator_gain",
        synth_get_first_oscillator_gain,
        synth_set_first_oscillator_gain
    },
    {
        MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN,
        "second_oscillator_gain",
        synth_get_second_oscillator_gain,
        synth_set_second_oscillator_gain
    },
    {
        MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH,
        "second_oscillator_morph",
        synth_get_second_oscillator_morph,
        synth_set_second_oscillator_morph
    },
    {
        MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE,
        "second_oscillator_octave",
        get_second_oscillator_octave,
        set_second_oscillator_octave
    },
    {
        MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH,
        "second_oscillator_pitch",
        get_second_oscillator_pitch,
        set_second_oscillator_pitch
    },
    {
        MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE,
        "second_oscillator_fine_tune",
        synth_get_second_oscillator_fine_tune,
        synth_set_second_oscillator_fine_tune
    },
    {
        MIDI_MAPPING_PARAM_STEREO_SPREAD,
        "stereo_spread",
        synth_get_stereo_spread,
        synth_set_stereo_spread
    },
    {MIDI_MAPPING_PARAM_LFO_RATE, "lfo_rate", synth_get_lfo_rate, synth_set_lfo_rate},
    {
        MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH,
        "lfo_shape_morph",
        synth_get_lfo_shape_morph,
        synth_set_lfo_shape_morph
    },
    {MIDI_MAPPING_PARAM_LFO_DEPTH, "lfo_depth", synth_get_lfo_depth, synth_set_lfo_depth},
    {
        MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_MORPH_AMOUNT,
        "lfo_first_oscillator_morph_amount",
        synth_get_lfo_first_oscillator_morph_amount,
        synth_set_lfo_first_oscillator_morph_amount
    },
    {
        MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_MORPH_AMOUNT,
        "lfo_second_oscillator_morph_amount",
        synth_get_lfo_second_oscillator_morph_amount,
        synth_set_lfo_second_oscillator_morph_amount
    },
    {
        MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT,
        "lfo_first_oscillator_gain_amount",
        synth_get_lfo_first_oscillator_gain_amount,
        synth_set_lfo_first_oscillator_gain_amount
    },
    {
        MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT,
        "lfo_second_oscillator_gain_amount",
        synth_get_lfo_second_oscillator_gain_amount,
        synth_set_lfo_second_oscillator_gain_amount
    },
    {
        MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT,
        "lfo_filter_amount",
        synth_get_lfo_filter_amount,
        synth_set_lfo_filter_amount
    },
    {
        MIDI_MAPPING_PARAM_DISTORTION_DRIVE,
        "distortion_drive",
        synth_get_distortion_drive,
        synth_set_distortion_drive
    },
    {
        MIDI_MAPPING_PARAM_DISTORTION_MIX,
        "distortion_mix",
        synth_get_distortion_mix,
        synth_set_distortion_mix
    }
};

static const midi_mapping_parameter_entry *find_parameter_by_name(const char *name)
{
    for (size_t i = 0; i < sizeof(parameter_entries) / sizeof(parameter_entries[0]); ++i) {
        if (strcmp(parameter_entries[i].name, name) == 0) {
            return &parameter_entries[i];
        }
    }

    return 0;
}

static const midi_mapping_parameter_entry *find_parameter(midi_mapping_parameter parameter)
{
    for (size_t i = 0; i < sizeof(parameter_entries) / sizeof(parameter_entries[0]); ++i) {
        if (parameter_entries[i].parameter == parameter) {
            return &parameter_entries[i];
        }
    }

    return 0;
}

// parses a synth parameter name from a config key.
static int parse_parameter(const char *name, midi_mapping_parameter *parameter)
{
    const midi_mapping_parameter_entry *entry = find_parameter_by_name(name);

    if (entry == 0) {
        return 0;
    }

    *parameter = entry->parameter;
    return 1;
}

// parses the source kind from a config value.
static int parse_source_type(const char *name, midi_mapping_source_type *source_type)
{
    if (strcmp(name, "cc") == 0) {
        *source_type = MIDI_MAPPING_SOURCE_CC;
        return 1;
    }

    return 0;
}

// parses the scaling mode from a config value.
static int parse_scale(const char *name, midi_mapping_scale *scale)
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

// parses an integer token inside an allowed range.
static int parse_int_range(const char *text, int min_value, int max_value, int *value)
{
    char *end;
    const long parsed = strtol(text, &end, 10);

    if (*text == '\0' || *end != '\0' || parsed < min_value || parsed > max_value) {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

// parses a float token.
static int parse_float_value(const char *text, float *value)
{
    char *end;
    const float parsed = (float)strtod(text, &end);

    if (*text == '\0' || *end != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

// scales a midi value into a synth value.
static float scale_midi_value(const midi_mapping_binding *binding, int midi_value)
{
    const float normalized = (float)midi_value / 127.0f;

    switch (binding->scale) {
        case MIDI_MAPPING_SCALE_LOG:
            // log scaling gives knobs more room in the low frequency range.
            return expf(
                logf(binding->min_value) +
                (normalized * (logf(binding->max_value) - logf(binding->min_value))));

        case MIDI_MAPPING_SCALE_STEP:
            // step scaling snaps continuous midi values to whole-number choices.
            return binding->min_value +
                (float)(int)((normalized * (binding->max_value - binding->min_value)) + 0.5f);

        case MIDI_MAPPING_SCALE_LINEAR:
        default:
            return binding->min_value + (normalized * (binding->max_value - binding->min_value));
    }
}

// returns the current synth-side value for a mapped parameter.
static float current_parameter_value(const synth *s, midi_mapping_parameter parameter)
{
    const midi_mapping_parameter_entry *entry = find_parameter(parameter);

    if (entry == 0 || entry->get == 0) {
        return 0.0f;
    }

    return entry->get(s);
}

// converts the current synth value back into the midi range for pickup checks.
static float synth_value_to_midi_value(const midi_mapping_binding *binding, float synth_value)
{
    const float bounded_value = clampf(synth_value, binding->min_value, binding->max_value);
    float normalized;

    if (binding->max_value == binding->min_value) {
        return 0.0f;
    }

    switch (binding->scale) {
        case MIDI_MAPPING_SCALE_LOG:
            normalized =
                (logf(bounded_value) - logf(binding->min_value)) /
                (logf(binding->max_value) - logf(binding->min_value));
            break;

        case MIDI_MAPPING_SCALE_STEP:
        case MIDI_MAPPING_SCALE_LINEAR:
        default:
            normalized =
                (bounded_value - binding->min_value) /
                (binding->max_value - binding->min_value);
            break;
    }

    return clampf(normalized, 0.0f, 1.0f) * 127.0f;
}

// checks whether a new midi value has reached or crossed the pickup point.
static int midi_value_reaches_pickup(
    const midi_mapping_pickup *pickup,
    float pickup_midi_value,
    int midi_value)
{
    const float current = (float)midi_value;

    if (fabsf(current - pickup_midi_value) <= MIDI_MAPPING_PICKUP_THRESHOLD) {
        return 1;
    }

    if (pickup->has_last_midi_value) {
        const float previous = (float)pickup->last_midi_value;

        if ((previous < pickup_midi_value && current > pickup_midi_value) ||
            (previous > pickup_midi_value && current < pickup_midi_value)) {
            return 1;
        }
    }

    return 0;
}

// tracks soft takeover state and tells the caller when this binding can write.
static int binding_has_pickup(midi_mapping_binding *binding, const synth *s, int midi_value)
{
    const float synth_value = current_parameter_value(s, binding->parameter);
    const float pickup_midi_value = synth_value_to_midi_value(binding, synth_value);

    if (binding->pickup.picked_up) {
        return 1;
    }

    if (midi_value_reaches_pickup(&binding->pickup, pickup_midi_value, midi_value)) {
        binding->pickup.picked_up = 1;
        return 1;
    }

    binding->pickup.has_last_midi_value = 1;
    binding->pickup.last_midi_value = midi_value;
    return 0;
}

// writes one mapped value into the synth.
static void apply_synth_value(synth *s, midi_mapping_parameter parameter, float synth_value)
{
    const midi_mapping_parameter_entry *entry = find_parameter(parameter);

    if (entry != 0 && entry->set != 0) {
        entry->set(s, synth_value);
    }
}

// adds one binding from a config line.
static int add_binding(
    midi_mapping *mapping,
    const char *key,
    char *value,
    char *error,
    size_t error_size,
    int line_number)
{
    char *source_name;
    char *channel_text;
    char *control_text;
    char *scale_name;
    char *min_text;
    char *max_text;
    midi_mapping_binding binding;

    memset(&binding, 0, sizeof(binding));

    if (mapping->binding_count >= MIDI_MAPPING_MAX_BINDINGS) {
        set_error(error, error_size, line_number, "too many midi bindings");
        return 0;
    }

    if (!parse_parameter(key, &binding.parameter)) {
        set_error(error, error_size, line_number, "unknown synth parameter");
        return 0;
    }

    source_name = strtok(value, ":");
    channel_text = strtok(0, ":");
    control_text = strtok(0, ":");
    scale_name = strtok(0, ":");
    min_text = strtok(0, ":");
    max_text = strtok(0, ":");

    if (source_name == 0 ||
        channel_text == 0 ||
        control_text == 0 ||
        scale_name == 0 ||
        min_text == 0 ||
        max_text == 0) {
        set_error(error, error_size, line_number, "expected cc:channel:control:scale:min:max");
        return 0;
    }

    source_name = trim(source_name);
    channel_text = trim(channel_text);
    control_text = trim(control_text);
    scale_name = trim(scale_name);
    min_text = trim(min_text);
    max_text = trim(max_text);

    if (!parse_source_type(source_name, &binding.source_type)) {
        set_error(error, error_size, line_number, "unknown midi source type");
        return 0;
    }

    if (!parse_int_range(channel_text, 1, 16, &binding.channel)) {
        set_error(error, error_size, line_number, "channel must be 1 through 16");
        return 0;
    }

    if (!parse_int_range(control_text, 0, 127, &binding.control)) {
        set_error(error, error_size, line_number, "control must be 0 through 127");
        return 0;
    }

    if (!parse_scale(scale_name, &binding.scale)) {
        set_error(error, error_size, line_number, "unknown scale type");
        return 0;
    }

    if (!parse_float_value(min_text, &binding.min_value) ||
        !parse_float_value(max_text, &binding.max_value)) {
        set_error(error, error_size, line_number, "min and max must be numbers");
        return 0;
    }

    if (binding.max_value <= binding.min_value) {
        set_error(error, error_size, line_number, "max must be greater than min");
        return 0;
    }

    if (binding.scale == MIDI_MAPPING_SCALE_LOG &&
        (binding.min_value <= 0.0f || binding.max_value <= 0.0f)) {
        set_error(error, error_size, line_number, "log scale min and max must be above zero");
        return 0;
    }

    mapping->bindings[mapping->binding_count] = binding;
    mapping->binding_count += 1;
    return 1;
}

// clears a midi mapping and gives it a fallback name.
void midi_mapping_init(midi_mapping *mapping)
{
    memset(mapping, 0, sizeof(*mapping));
    copy_string(mapping->name, sizeof(mapping->name), "unnamed midi controller");
}

// returns the readable name for a mapped synth parameter.
const char *midi_mapping_parameter_name(midi_mapping_parameter parameter)
{
    const midi_mapping_parameter_entry *entry = find_parameter(parameter);

    return entry != 0 ? entry->name : "unknown";
}

// loads a midi mapping from a config file.
int midi_mapping_load(midi_mapping *mapping, const char *path, char *error, size_t error_size)
{
    FILE *file;
    char line[256];
    int line_number = 0;

    midi_mapping_init(mapping);
    file = fopen(path, "r");
    if (file == 0) {
        set_error(error, error_size, 0, "could not open midi mapping file");
        return 0;
    }

    while (fgets(line, sizeof(line), file) != 0) {
        char *separator;
        char *key;
        char *value;

        line_number += 1;
        key = trim(line);
        if (*key == '\0' || *key == '#') {
            continue;
        }

        separator = strchr(key, '=');
        if (separator == 0) {
            fclose(file);
            set_error(error, error_size, line_number, "expected key=value");
            return 0;
        }

        *separator = '\0';
        value = trim(separator + 1);
        key = trim(key);

        if (strcmp(key, "name") == 0) {
            copy_string(mapping->name, sizeof(mapping->name), value);
        } else if (!add_binding(mapping, key, value, error, error_size, line_number)) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

// applies a raw midi message to the synth when it matches a binding.
int midi_mapping_apply_short_message(
    midi_mapping *mapping,
    const unsigned char *data,
    unsigned short length,
    synth *s,
    midi_mapping_apply_result *result)
{
    unsigned char status;
    int channel;
    int control;
    int midi_value;

    if (mapping == 0 || data == 0 || s == 0 || length < 3) {
        return 0;
    }

    status = data[0] & 0xF0;
    if (status != 0xB0) {
        return 0;
    }

    channel = (data[0] & 0x0F) + 1;
    control = data[1];
    midi_value = data[2];

    for (size_t i = 0; i < mapping->binding_count; ++i) {
        midi_mapping_binding *binding = &mapping->bindings[i];

        if (binding->source_type == MIDI_MAPPING_SOURCE_CC &&
            binding->channel == channel &&
            binding->control == control) {
            const float synth_value = scale_midi_value(binding, midi_value);

            if (!binding_has_pickup(binding, s, midi_value)) {
                return 0;
            }

            apply_synth_value(s, binding->parameter, synth_value);

            if (result != 0) {
                result->parameter = binding->parameter;
                result->channel = channel;
                result->control = control;
                result->midi_value = midi_value;
                result->synth_value = synth_value;
            }

            return 1;
        }
    }

    return 0;
}
