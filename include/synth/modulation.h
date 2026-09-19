#ifndef SYNTH_MODULATION_H
#define SYNTH_MODULATION_H

#include "synth/parameter.h"

typedef enum synth_modulation_source {
    SYNTH_MOD_SOURCE_LFO,
    SYNTH_MOD_SOURCE_ENVELOPE,
    SYNTH_MOD_SOURCE_COUNT
} synth_modulation_source;

typedef struct synth_modulation {
    // route strengths only; parameter bases belong to the synth controls
    float amounts[SYNTH_MOD_SOURCE_COUNT][SYNTH_PARAM_COUNT];
} synth_modulation;

// shared eligibility and route access for hosts and controller adapters
int synth_modulation_supports(synth_modulation_source source, synth_parameter_id target);
int synth_set_modulation_amount(struct synth *s, synth_modulation_source source,
                                synth_parameter_id target, float amount);
float synth_get_modulation_amount(const struct synth *s, synth_modulation_source source,
                                  synth_parameter_id target);
void synth_reset_modulation_amounts(struct synth *s, synth_modulation_source source);
int synth_set_lfo_amount(struct synth *s, synth_parameter_id target, float amount);
float synth_get_lfo_amount(const struct synth *s, synth_parameter_id target);
void synth_reset_lfo_amounts(struct synth *s);
int synth_set_envelope_amount(struct synth *s, synth_parameter_id target, float amount);
float synth_get_envelope_amount(const struct synth *s, synth_parameter_id target);
void synth_reset_envelope_amounts(struct synth *s);

// pure evaluation; neither source advances and no stored controls change
float synth_modulate_value(const struct synth *s, synth_parameter_id target,
                          float base, float lfo_value);
float synth_modulate_sources(const struct synth *s, synth_parameter_id target,
                            float base, float lfo_value, float envelope_value);

#endif
