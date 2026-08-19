#include "midi/midi_mapping.h"
#include "midi/midi_mapping_internal.h"
#include "midi/midi_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// parses a chord-mode pad name from a config key.
static int parse_chord_pad(const char *name, midi_chord_mode_pad *pad)
{
    const midi_mapping_chord_entry *entry = midi_mapping_find_chord_by_name(name);

    if (entry == 0) {
        return 0;
    }

    *pad = entry->pad;
    return 1;
}

// parses a synth parameter name from a config key.
static int parse_parameter(const char *name, midi_mapping_parameter *parameter)
{
    const midi_mapping_parameter_entry *entry = midi_mapping_find_parameter_by_name(name);

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
    return midi_mapping_parse_scale_name(name, scale);
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

// parses a simple cc:channel:control binding from a config value.
static int parse_control_binding(
    char *value,
    midi_mapping_control_binding *binding,
    char *error,
    size_t error_size,
    int line_number)
{
    char *source_name;
    char *channel_text;
    char *control_text;
    char *extra_text;

    source_name = strtok(value, ":");
    channel_text = strtok(0, ":");
    control_text = strtok(0, ":");
    extra_text = strtok(0, ":");

    if (source_name == 0 || channel_text == 0 || control_text == 0 || extra_text != 0) {
        set_error(error, error_size, line_number, "expected cc:channel:control");
        return 0;
    }

    source_name = midi_text_trim(source_name);
    channel_text = midi_text_trim(channel_text);
    control_text = midi_text_trim(control_text);

    if (!parse_source_type(source_name, &binding->source_type)) {
        set_error(error, error_size, line_number, "unknown midi source type");
        return 0;
    }

    if (!parse_int_range(channel_text, 1, 16, &binding->channel)) {
        set_error(error, error_size, line_number, "channel must be 1 through 16");
        return 0;
    }

    if (!parse_int_range(control_text, 0, 127, &binding->control)) {
        set_error(error, error_size, line_number, "control must be 0 through 127");
        return 0;
    }

    binding->enabled = 1;
    return 1;
}

// parses an effect macro config key into a zero-based macro index.
static int parse_effect_macro_key(const char *key, size_t *macro_index)
{
    for (size_t i = 0; i < MIDI_MAPPING_EFFECT_MACRO_COUNT; ++i) {
        if (strcmp(key, midi_mapping_effect_macro_name(i)) == 0) {
            if (macro_index != 0) {
                *macro_index = i;
            }
            return 1;
        }
    }

    return 0;
}

// adds one chord-mode pad binding from a config line.
static int add_chord_binding(
    midi_mapping *mapping,
    const char *key,
    char *value,
    char *error,
    size_t error_size,
    int line_number)
{
    midi_mapping_chord_binding binding;
    midi_mapping_control_binding control_binding;

    memset(&binding, 0, sizeof(binding));
    memset(&control_binding, 0, sizeof(control_binding));

    if (!parse_chord_pad(key, &binding.pad)) {
        return 0;
    }

    if (!parse_control_binding(value, &control_binding, error, error_size, line_number)) {
        return 0;
    }

    binding.enabled = control_binding.enabled;
    binding.source_type = control_binding.source_type;
    binding.channel = control_binding.channel;
    binding.control = control_binding.control;
    mapping->chord_bindings[binding.pad] = binding;
    return 1;
}

// adds one effect macro control binding from a config line.
static int add_effect_control_binding(
    midi_mapping *mapping,
    const char *key,
    char *value,
    char *error,
    size_t error_size,
    int line_number)
{
    size_t macro_index;
    midi_mapping_control_binding binding;

    memset(&binding, 0, sizeof(binding));

    if (!parse_control_binding(value, &binding, error, error_size, line_number)) {
        return 0;
    }

    if (strcmp(key, "effect_selector") == 0) {
        mapping->effect_selector = binding;
        return 1;
    }

    if (parse_effect_macro_key(key, &macro_index)) {
        mapping->effect_macros[macro_index] = binding;
        return 1;
    }

    set_error(error, error_size, line_number, "unknown effect macro control");
    return 0;
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

    source_name = midi_text_trim(source_name);
    channel_text = midi_text_trim(channel_text);
    control_text = midi_text_trim(control_text);
    scale_name = midi_text_trim(scale_name);
    min_text = midi_text_trim(min_text);
    max_text = midi_text_trim(max_text);

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
    midi_text_copy(mapping->name, sizeof(mapping->name), "unnamed midi controller");
    mapping->selected_effect = MIDI_MAPPING_EFFECT_SATURATION;
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
        key = midi_text_trim(line);
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
        value = midi_text_trim(separator + 1);
        key = midi_text_trim(key);

        if (strcmp(key, "name") == 0) {
            midi_text_copy(mapping->name, sizeof(mapping->name), value);
        } else if (strcmp(key, "effect_selector") == 0 ||
                   parse_effect_macro_key(key, 0)) {
            if (!add_effect_control_binding(mapping, key, value, error, error_size, line_number)) {
                fclose(file);
                return 0;
            }
        } else if (midi_mapping_find_chord_by_name(key) != 0) {
            if (!add_chord_binding(mapping, key, value, error, error_size, line_number)) {
                fclose(file);
                return 0;
            }
        } else if (!add_binding(mapping, key, value, error, error_size, line_number)) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

// copies loaded chord pad bindings into a chord-mode processor.
void midi_mapping_configure_chord_mode(const midi_mapping *mapping, midi_chord_mode *mode)
{
    size_t index;

    if (mapping == 0 || mode == 0) {
        return;
    }

    midi_chord_mode_clear_pad_bindings(mode);
    for (index = 0; index < MIDI_CHORD_MODE_PAD_COUNT; ++index) {
        const midi_mapping_chord_binding *binding = &mapping->chord_bindings[index];

        if (binding->enabled && binding->source_type == MIDI_MAPPING_SOURCE_CC) {
            midi_chord_mode_bind_pad(mode, binding->pad, binding->channel, binding->control);
        }
    }
}
