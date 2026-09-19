#ifndef SYNTH_MODULATION_INTERNAL_H
#define SYNTH_MODULATION_INTERNAL_H
#include "synth/synth.h"
// compound controls add their induced offsets before a single final clamp
float synth_modulate_unclamped(const synth *s, synth_parameter_id target,
                               float base, float lfo_value, float envelope_value);
#endif
