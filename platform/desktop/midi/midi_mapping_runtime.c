#include "midi/midi_mapping.h"
#include "midi/midi_mapping_internal.h"

#include <math.h>
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
    const midi_mapping_parameter_entry *entry = midi_mapping_find_parameter(parameter);

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
    const midi_mapping_parameter_entry *entry = midi_mapping_find_parameter(parameter);

    if (entry != 0 && entry->set != 0) {
        entry->set(s, synth_value);
    }
}

// returns the effect page selected by the selector knob's four fixed ranges.
static midi_mapping_effect effect_for_selector_value(int midi_value)
{
    if (midi_value < 32) {
        return MIDI_MAPPING_EFFECT_SATURATION;
    }

    if (midi_value < 64) {
        return MIDI_MAPPING_EFFECT_DISTORTION;
    }

    if (midi_value < 96) {
        return MIDI_MAPPING_EFFECT_BITCRUSHER;
    }

    return MIDI_MAPPING_EFFECT_DELAY;
}

static int control_binding_matches(
    const midi_mapping_control_binding *binding,
    int channel,
    int control)
{
    return binding->enabled &&
        binding->source_type == MIDI_MAPPING_SOURCE_CC &&
        binding->channel == channel &&
        binding->control == control;
}

static void reset_effect_macro_pickups(midi_mapping *mapping, midi_mapping_effect effect)
{
    if (effect < 0 || effect >= MIDI_MAPPING_EFFECT_COUNT) {
        return;
    }

    memset(
        mapping->effect_macro_pickups[effect],
        0,
        sizeof(mapping->effect_macro_pickups[effect]));
}

static void select_effect(midi_mapping *mapping, midi_mapping_effect effect)
{
    if (effect == mapping->selected_effect) {
        return;
    }

    mapping->selected_effect = effect;
    reset_effect_macro_pickups(mapping, effect);
}

static int fill_macro_parameter_binding(
    midi_mapping *mapping,
    size_t macro_index,
    midi_mapping_binding *binding)
{
    const midi_mapping_effect effect = mapping->selected_effect;
    midi_mapping_parameter parameter;
    const midi_mapping_parameter_entry *entry;

    if (!midi_mapping_effect_macro_parameter(effect, macro_index, &parameter)) {
        return 0;
    }

    entry = midi_mapping_find_parameter(parameter);
    if (entry == 0) {
        return 0;
    }

    binding->parameter = entry->parameter;
    binding->source_type = MIDI_MAPPING_SOURCE_CC;
    binding->channel = mapping->effect_macros[macro_index].channel;
    binding->control = mapping->effect_macros[macro_index].control;
    binding->scale = entry->default_scale;
    binding->min_value = entry->default_min_value;
    binding->max_value = entry->default_max_value;
    binding->pickup = mapping->effect_macro_pickups[effect][macro_index];
    return 1;
}

static int apply_parameter_binding(
    midi_mapping_binding *binding,
    int channel,
    int control,
    int midi_value,
    synth *s,
    midi_mapping_apply_result *result)
{
    const float synth_value = scale_midi_value(binding, midi_value);

    if (!binding_has_pickup(binding, s, midi_value)) {
        return 0;
    }

    apply_synth_value(s, binding->parameter, synth_value);

    if (result != 0) {
        result->kind = MIDI_MAPPING_APPLY_PARAMETER;
        result->parameter = binding->parameter;
        result->channel = channel;
        result->control = control;
        result->midi_value = midi_value;
        result->synth_value = synth_value;
    }

    return 1;
}

static int apply_effect_macro(
    midi_mapping *mapping,
    size_t macro_index,
    int channel,
    int control,
    int midi_value,
    synth *s,
    midi_mapping_apply_result *result)
{
    midi_mapping_binding binding;
    const midi_mapping_effect effect = mapping->selected_effect;
    int applied;

    if (!fill_macro_parameter_binding(mapping, macro_index, &binding)) {
        return 0;
    }

    applied = apply_parameter_binding(&binding, channel, control, midi_value, s, result);
    mapping->effect_macro_pickups[effect][macro_index] = binding.pickup;
    return applied;
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

    if (control_binding_matches(&mapping->effect_selector, channel, control)) {
        select_effect(mapping, effect_for_selector_value(midi_value));
        if (result != 0) {
            result->kind = MIDI_MAPPING_APPLY_EFFECT_SELECT;
            result->effect = mapping->selected_effect;
            result->channel = channel;
            result->control = control;
            result->midi_value = midi_value;
            result->synth_value = 0.0f;
        }
        return 1;
    }

    for (size_t i = 0; i < MIDI_MAPPING_EFFECT_MACRO_COUNT; ++i) {
        if (control_binding_matches(&mapping->effect_macros[i], channel, control)) {
            return apply_effect_macro(mapping, i, channel, control, midi_value, s, result);
        }
    }

    for (size_t i = 0; i < mapping->binding_count; ++i) {
        midi_mapping_binding *binding = &mapping->bindings[i];

        if (binding->source_type == MIDI_MAPPING_SOURCE_CC &&
            binding->channel == channel &&
            binding->control == control) {
            return apply_parameter_binding(binding, channel, control, midi_value, s, result);
        }
    }

    return 0;
}
