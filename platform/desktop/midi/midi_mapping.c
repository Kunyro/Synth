#include "midi/midi_mapping.h"

#include <ctype.h>
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

// parses a synth parameter name from a config key.
static int parse_parameter(const char *name, midi_mapping_parameter *parameter)
{
    if (strcmp(name, "attack") == 0) {
        *parameter = MIDI_MAPPING_PARAM_ATTACK;
        return 1;
    }

    if (strcmp(name, "decay") == 0) {
        *parameter = MIDI_MAPPING_PARAM_DECAY;
        return 1;
    }

    if (strcmp(name, "sustain") == 0) {
        *parameter = MIDI_MAPPING_PARAM_SUSTAIN;
        return 1;
    }

    if (strcmp(name, "release") == 0) {
        *parameter = MIDI_MAPPING_PARAM_RELEASE;
        return 1;
    }

    return 0;
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
        case MIDI_MAPPING_SCALE_LINEAR:
        default:
            return binding->min_value + (normalized * (binding->max_value - binding->min_value));
    }
}

// adds one binding from a config line.
static int add_binding(midi_mapping *mapping, const char *key, char *value, char *error, size_t error_size, int line_number)
{
    char *source_name;
    char *channel_text;
    char *control_text;
    char *scale_name;
    char *min_text;
    char *max_text;
    midi_mapping_binding binding;

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

    if (source_name == 0 || channel_text == 0 || control_text == 0 || scale_name == 0 || min_text == 0 || max_text == 0) {
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

    if (!parse_float_value(min_text, &binding.min_value) || !parse_float_value(max_text, &binding.max_value)) {
        set_error(error, error_size, line_number, "min and max must be numbers");
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
    switch (parameter) {
        case MIDI_MAPPING_PARAM_ATTACK:
            return "attack";

        case MIDI_MAPPING_PARAM_DECAY:
            return "decay";

        case MIDI_MAPPING_PARAM_SUSTAIN:
            return "sustain";

        case MIDI_MAPPING_PARAM_RELEASE:
            return "release";

        default:
            return "unknown";
    }
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
    const midi_mapping *mapping,
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
        const midi_mapping_binding *binding = &mapping->bindings[i];

        if (binding->source_type == MIDI_MAPPING_SOURCE_CC &&
            binding->channel == channel &&
            binding->control == control) {
            synth_adsr adsr = s->envelope;
            const float synth_value = scale_midi_value(binding, midi_value);

            switch (binding->parameter) {
                case MIDI_MAPPING_PARAM_ATTACK:
                    adsr.attack_seconds = synth_value;
                    break;

                case MIDI_MAPPING_PARAM_DECAY:
                    adsr.decay_seconds = synth_value;
                    break;

                case MIDI_MAPPING_PARAM_SUSTAIN:
                    adsr.sustain_level = synth_value;
                    break;

                case MIDI_MAPPING_PARAM_RELEASE:
                    adsr.release_seconds = synth_value;
                    break;
            }

            synth_set_adsr(s, adsr);

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
