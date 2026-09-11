#ifndef SYNTH_MODULATION_H
#define SYNTH_MODULATION_H

#include "synth/parameter.h"

typedef struct synth_modulation {
    // one slot per engine ID avoids a second destination-id system slots for
    // excluded controls stay zero; this stores route strengths, not base settings
    float amounts[SYNTH_PARAM_COUNT];
} synth_modulation;

// route amounts clamp to [-1, 1]; zero disables a route
// returns zero for invalid/excluded ids, nonfinite amounts, or a null synth
int synth_set_lfo_amount(struct synth *s, synth_parameter_id target, float amount);
// reads a route strength; invalid/excluded targets return zero
float synth_get_lfo_amount(const struct synth *s, synth_parameter_id target);
// disables all routes while leaving source settings and phase alone
void synth_reset_lfo_amounts(struct synth *s);
// pure destination evaluation: does not advance the source or change base state
float synth_modulate_value(const struct synth *s, synth_parameter_id target,
                          float base, float lfo_value);

#endif
