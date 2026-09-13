#include "synth/modulation.h"

#include <math.h>
#include <string.h>

#include "synth/synth.h"
#include "internal/synth_internal.h"

// sets one route's strength and direction; rejects controls that cannot be targets
int synth_set_lfo_amount(synth *s, synth_parameter_id target, float amount)
{
    const synth_parameter_info *info = synth_parameter_info_at(target);
    if (s == NULL || info == NULL || !info->modulatable || !isfinite(amount)) {
        return 0;
    }
    // negative amounts reverse the lfo, zero turns it off, and +/-1 is full strength
    s->modulation.amounts[target] = synth_clampf(amount, -1.0f, 1.0f);
    return 1;
}

// reads a stored route amount; an invalid or excluded target behaves like an off route
float synth_get_lfo_amount(const synth *s, synth_parameter_id target)
{
    const synth_parameter_info *info = synth_parameter_info_at(target);
    return s != NULL && info != NULL && info->modulatable
        ? s->modulation.amounts[target] : 0.0f;
}

// turns off every route without changing the lfo's phase, rate, shape, or global depth
void synth_reset_lfo_amounts(synth *s)
{
    if (s != NULL) memset(&s->modulation, 0, sizeof(s->modulation));
}

// calculates one temporary control value without changing the base or advancing time
float synth_modulate_value(const synth *s, synth_parameter_id target,
                          float base, float lfo_value)
{
    const synth_parameter_info *info = synth_parameter_info_at(target);
    const float amount = synth_get_lfo_amount(s, target);
    float offset, value;
    if (s == NULL || info == NULL || !isfinite(base) || !isfinite(lfo_value)) {
        return 0.0f;
    }
    // returning the base directly keeps an inactive route exactly unchanged
    if (amount == 0.0f || s->lfo_depth == 0.0f || lfo_value == 0.0f) {
        return base;
    }
    // the source position, master depth, and route amount scale the fixed excursion
    offset = lfo_value * s->lfo_depth * amount * info->modulation_span;
    // a log-domain offset of +1 doubles the base; -1 halves it linear offsets add
    // native units instead (for example, seconds, decibels, or normalized mix)
    value = info->domain == SYNTH_DOMAIN_LOG2 ? base * exp2f(offset) : base + offset;
    // keep the result safe for the effect and snap stepped controls independently
    return synth_parameter_clamp(target, value, s->sample_rate);
}
