#include "midi/midi_mapping.h"

#include <math.h>
#include <stdio.h>

// checks one binding against the shared loader/learn/save rules without changing it
int midi_mapping_validate_binding(const midi_mapping_binding *binding,
                                  char *error, size_t error_size)
{
    midi_mapping_parameter_info info;
    const char *message = NULL;
    if (binding == NULL || !midi_mapping_parameter_info_for(
            binding->parameter, binding->target_kind, &info)) {
        message = "unknown or excluded control target";
    } else if (binding->source_type != MIDI_MAPPING_SOURCE_CC ||
               binding->channel < 1 || binding->channel > 16 ||
               binding->control < 0 || binding->control > 127) {
        message = "invalid CC source, channel, or control";
    } else if (binding->scale < MIDI_MAPPING_SCALE_LINEAR ||
               binding->scale > MIDI_MAPPING_SCALE_STEP) {
        message = "unknown scale type";
    } else if (!isfinite(binding->min_value) || !isfinite(binding->max_value) ||
               !isfinite(binding->max_value - binding->min_value) ||
               binding->max_value <= binding->min_value) {
        // even finite endpoints can have an overflowing difference scaling needs
        // the whole interval to be finite and nonzero, not just the two endpoints
        message = "bounds must be finite with max greater than min";
    } else if (binding->scale == MIDI_MAPPING_SCALE_LOG && binding->min_value <= 0) {
        // logarithmic knob scaling takes log(min), which requires a positive minimum
        message = "log scale bounds must be positive";
    } else if (binding->target_kind == MIDI_MAPPING_TARGET_LFO_AMOUNT &&
               (binding->min_value < -1 || binding->max_value > 1)) {
        message = "LFO amount bounds must be within -1 through 1";
    }
    if (message != NULL && error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
    return message == NULL;
}
