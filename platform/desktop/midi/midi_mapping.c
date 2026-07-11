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

    if (strcmp(name, "master_gain") == 0) {
        *parameter = MIDI_MAPPING_PARAM_MASTER_GAIN;
        return 1;
    }

    if (strcmp(name, "filter_cutoff") == 0) {
        *parameter = MIDI_MAPPING_PARAM_FILTER_CUTOFF;
        return 1;
    }

    if (strcmp(name, "filter_poles") == 0) {
        *parameter = MIDI_MAPPING_PARAM_FILTER_POLES;
        return 1;
    }

    if (strcmp(name, "oscillator_morph") == 0) {
        *parameter = MIDI_MAPPING_PARAM_OSCILLATOR_MORPH;
        return 1;
    }

    if (strcmp(name, "first_oscillator_gain") == 0) {
        *parameter = MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN;
        return 1;
    }

    if (strcmp(name, "second_oscillator_gain") == 0) {
        *parameter = MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN;
        return 1;
    }

    if (strcmp(name, "second_oscillator_morph") == 0) {
        *parameter = MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH;
        return 1;
    }

    if (strcmp(name, "second_oscillator_octave") == 0) {
        *parameter = MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE;
        return 1;
    }

    if (strcmp(name, "second_oscillator_pitch") == 0) {
        *parameter = MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH;
        return 1;
    }

    if (strcmp(name, "second_oscillator_fine_tune") == 0) {
        *parameter = MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE;
        return 1;
    }

    if (strcmp(name, "stereo_spread") == 0) {
        *parameter = MIDI_MAPPING_PARAM_STEREO_SPREAD;
        return 1;
    }

    if (strcmp(name, "lfo_rate") == 0) {
        *parameter = MIDI_MAPPING_PARAM_LFO_RATE;
        return 1;
    }

    if (strcmp(name, "lfo_shape_morph") == 0) {
        *parameter = MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH;
        return 1;
    }

    if (strcmp(name, "lfo_depth") == 0) {
        *parameter = MIDI_MAPPING_PARAM_LFO_DEPTH;
        return 1;
    }

    if (strcmp(name, "lfo_morph_amount") == 0) {
        *parameter = MIDI_MAPPING_PARAM_LFO_MORPH_AMOUNT;
        return 1;
    }

    if (strcmp(name, "lfo_first_oscillator_gain_amount") == 0) {
        *parameter = MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT;
        return 1;
    }

    if (strcmp(name, "lfo_second_oscillator_gain_amount") == 0) {
        *parameter = MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT;
        return 1;
    }

    if (strcmp(name, "lfo_filter_amount") == 0) {
        *parameter = MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT;
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
            return expf(logf(binding->min_value) + (normalized * (logf(binding->max_value) - logf(binding->min_value))));

        case MIDI_MAPPING_SCALE_STEP:
            // step scaling snaps continuous midi values to whole-number choices.
            return binding->min_value + (float)(int)((normalized * (binding->max_value - binding->min_value)) + 0.5f);

        case MIDI_MAPPING_SCALE_LINEAR:
        default:
            return binding->min_value + (normalized * (binding->max_value - binding->min_value));
    }
}

// returns the current synth-side value for a mapped parameter.
static float current_parameter_value(const synth *s, midi_mapping_parameter parameter)
{
    const synth_adsr adsr = synth_get_adsr(s);

    switch (parameter) {
        case MIDI_MAPPING_PARAM_ATTACK:
            return adsr.attack_seconds;

        case MIDI_MAPPING_PARAM_DECAY:
            return adsr.decay_seconds;

        case MIDI_MAPPING_PARAM_SUSTAIN:
            return adsr.sustain_level;

        case MIDI_MAPPING_PARAM_RELEASE:
            return adsr.release_seconds;

        case MIDI_MAPPING_PARAM_MASTER_GAIN:
            return s->master_gain;

        case MIDI_MAPPING_PARAM_FILTER_CUTOFF:
            return s->filter.cutoff_hz;

        case MIDI_MAPPING_PARAM_FILTER_POLES:
            return (float)s->filter.pole_count;

        case MIDI_MAPPING_PARAM_OSCILLATOR_MORPH:
            return s->oscillator_morph;

        case MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN:
            return s->first_oscillator_gain;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN:
            return s->second_oscillator_gain;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH:
            return s->second_oscillator_morph;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE:
            return (float)s->second_oscillator_octave;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH:
            return (float)s->second_oscillator_pitch_semitones;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE:
            return s->second_oscillator_fine_tune_cents;

        case MIDI_MAPPING_PARAM_STEREO_SPREAD:
            return s->stereo_spread;

        case MIDI_MAPPING_PARAM_LFO_RATE:
            return s->lfo.frequency_hz;

        case MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH:
            return s->lfo.morph;

        case MIDI_MAPPING_PARAM_LFO_DEPTH:
            return s->lfo_depth;

        case MIDI_MAPPING_PARAM_LFO_MORPH_AMOUNT:
            return s->lfo_morph_amount;

        case MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT:
            return s->lfo_first_oscillator_gain_amount;

        case MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT:
            return s->lfo_second_oscillator_gain_amount;

        case MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT:
            return s->lfo_filter_amount;

        default:
            return 0.0f;
    }
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
            normalized = (bounded_value - binding->min_value) / (binding->max_value - binding->min_value);
            break;
    }

    return clampf(normalized, 0.0f, 1.0f) * 127.0f;
}

// checks whether a new midi value has reached or crossed the pickup point.
static int midi_value_reaches_pickup(const midi_mapping_pickup *pickup, float pickup_midi_value, int midi_value)
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
    // adsr is updated as one struct so the unchanged parts stay intact.
    synth_adsr adsr = synth_get_adsr(s);
    int should_set_adsr = 0;

    switch (parameter) {
        case MIDI_MAPPING_PARAM_ATTACK:
            adsr.attack_seconds = synth_value;
            should_set_adsr = 1;
            break;

        case MIDI_MAPPING_PARAM_DECAY:
            adsr.decay_seconds = synth_value;
            should_set_adsr = 1;
            break;

        case MIDI_MAPPING_PARAM_SUSTAIN:
            adsr.sustain_level = synth_value;
            should_set_adsr = 1;
            break;

        case MIDI_MAPPING_PARAM_RELEASE:
            adsr.release_seconds = synth_value;
            should_set_adsr = 1;
            break;

        case MIDI_MAPPING_PARAM_MASTER_GAIN:
            synth_set_master_gain(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_FILTER_CUTOFF:
            synth_set_filter_cutoff(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_FILTER_POLES:
            synth_set_filter_poles(s, (int)synth_value);
            break;

        case MIDI_MAPPING_PARAM_OSCILLATOR_MORPH:
            synth_set_oscillator_morph(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN:
            synth_set_first_oscillator_gain(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN:
            synth_set_second_oscillator_gain(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH:
            synth_set_second_oscillator_morph(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE:
            synth_set_second_oscillator_octave(s, (int)synth_value);
            break;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH:
            synth_set_second_oscillator_pitch(s, (int)synth_value);
            break;

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE:
            synth_set_second_oscillator_fine_tune(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_STEREO_SPREAD:
            synth_set_stereo_spread(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_LFO_RATE:
            synth_set_lfo_rate(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH:
            synth_set_lfo_shape_morph(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_LFO_DEPTH:
            synth_set_lfo_depth(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_LFO_MORPH_AMOUNT:
            synth_set_lfo_morph_amount(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT:
            synth_set_lfo_first_oscillator_gain_amount(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT:
            synth_set_lfo_second_oscillator_gain_amount(s, synth_value);
            break;

        case MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT:
            synth_set_lfo_filter_amount(s, synth_value);
            break;
    }

    if (should_set_adsr) {
        synth_set_adsr(s, adsr);
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

    if (binding.max_value <= binding.min_value) {
        set_error(error, error_size, line_number, "max must be greater than min");
        return 0;
    }

    if (binding.scale == MIDI_MAPPING_SCALE_LOG && (binding.min_value <= 0.0f || binding.max_value <= 0.0f)) {
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
    switch (parameter) {
        case MIDI_MAPPING_PARAM_ATTACK:
            return "attack";

        case MIDI_MAPPING_PARAM_DECAY:
            return "decay";

        case MIDI_MAPPING_PARAM_SUSTAIN:
            return "sustain";

        case MIDI_MAPPING_PARAM_RELEASE:
            return "release";

        case MIDI_MAPPING_PARAM_MASTER_GAIN:
            return "master_gain";

        case MIDI_MAPPING_PARAM_FILTER_CUTOFF:
            return "filter_cutoff";

        case MIDI_MAPPING_PARAM_FILTER_POLES:
            return "filter_poles";

        case MIDI_MAPPING_PARAM_OSCILLATOR_MORPH:
            return "oscillator_morph";

        case MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN:
            return "first_oscillator_gain";

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN:
            return "second_oscillator_gain";

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH:
            return "second_oscillator_morph";

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE:
            return "second_oscillator_octave";

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH:
            return "second_oscillator_pitch";

        case MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE:
            return "second_oscillator_fine_tune";

        case MIDI_MAPPING_PARAM_STEREO_SPREAD:
            return "stereo_spread";

        case MIDI_MAPPING_PARAM_LFO_RATE:
            return "lfo_rate";

        case MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH:
            return "lfo_shape_morph";

        case MIDI_MAPPING_PARAM_LFO_DEPTH:
            return "lfo_depth";

        case MIDI_MAPPING_PARAM_LFO_MORPH_AMOUNT:
            return "lfo_morph_amount";

        case MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT:
            return "lfo_first_oscillator_gain_amount";

        case MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT:
            return "lfo_second_oscillator_gain_amount";

        case MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT:
            return "lfo_filter_amount";

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
