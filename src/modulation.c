#include "synth/modulation.h"

#include <math.h>
#include <string.h>

#include "synth/synth.h"
#include "internal/synth_internal.h"
#include "internal/modulation_internal.h"

int synth_modulation_supports(synth_modulation_source source, synth_parameter_id target)
{
    const synth_parameter_info *info = synth_parameter_info_at(target);
    if (info == NULL || !info->modulatable) return 0;
    if (source == SYNTH_MOD_SOURCE_LFO) return 1;
    return source == SYNTH_MOD_SOURCE_ENVELOPE && target > SYNTH_PARAM_RELEASE;
}

int synth_set_modulation_amount(synth *s, synth_modulation_source source,
                                synth_parameter_id target, float amount)
{
    if (s == NULL || !synth_modulation_supports(source, target) || !isfinite(amount)) return 0;
    s->modulation.amounts[source][target] = synth_clampf(amount, -1.0f, 1.0f);
    return 1;
}

float synth_get_modulation_amount(const synth *s, synth_modulation_source source,
                                  synth_parameter_id target)
{
    return s != NULL && synth_modulation_supports(source, target)
        ? s->modulation.amounts[source][target] : 0.0f;
}

void synth_reset_modulation_amounts(synth *s, synth_modulation_source source)
{
    if (s != NULL && source >= 0 && source < SYNTH_MOD_SOURCE_COUNT)
        memset(s->modulation.amounts[source], 0, sizeof(s->modulation.amounts[source]));
}

int synth_set_lfo_amount(synth *s, synth_parameter_id target, float amount)
{
    return synth_set_modulation_amount(s, SYNTH_MOD_SOURCE_LFO, target, amount);
}
float synth_get_lfo_amount(const synth *s, synth_parameter_id target)
{
    return synth_get_modulation_amount(s, SYNTH_MOD_SOURCE_LFO, target);
}
void synth_reset_lfo_amounts(synth *s)
{
    synth_reset_modulation_amounts(s, SYNTH_MOD_SOURCE_LFO);
}
int synth_set_envelope_amount(synth *s, synth_parameter_id target, float amount)
{
    return synth_set_modulation_amount(s, SYNTH_MOD_SOURCE_ENVELOPE, target, amount);
}
float synth_get_envelope_amount(const synth *s, synth_parameter_id target)
{
    return synth_get_modulation_amount(s, SYNTH_MOD_SOURCE_ENVELOPE, target);
}
void synth_reset_envelope_amounts(synth *s)
{
    synth_reset_modulation_amounts(s, SYNTH_MOD_SOURCE_ENVELOPE);
}

// leave clamping to the caller so compound controls can combine all offsets first
float synth_modulate_unclamped(const synth *s, synth_parameter_id target,
                               float base, float lfo_value, float envelope_value)
{
    const synth_parameter_info *info = synth_parameter_info_at(target);
    float low, high, endpoint, envelope_offset = 0.0f;
    if (s == NULL || info == NULL || !isfinite(base) ||
        !isfinite(lfo_value) || !isfinite(envelope_value)) return 0.0f;
    const float lfo_offset = lfo_value * s->lfo_depth *
        synth_get_lfo_amount(s, target) * info->modulation_span;
    const float amount = synth_get_envelope_amount(s, target);
    const float weight = synth_clampf(envelope_value, 0.0f, 1.0f) *
        s->mod_envelope_depth * fabsf(amount);
    if (weight != 0.0f) {
        synth_parameter_bounds(target, s->sample_rate, &low, &high);
        endpoint = amount < 0.0f ? low : high;
        if (lfo_offset == 0.0f && weight == 1.0f) return endpoint;
        if (info->domain == SYNTH_DOMAIN_LOG2 && base > 0.0f && endpoint > 0.0f)
            envelope_offset = weight * (log2f(endpoint) - log2f(base));
        else if (info->domain == SYNTH_DOMAIN_LINEAR)
            envelope_offset = weight * (endpoint - base);
        else return endpoint;
    }
    const float offset = lfo_offset + envelope_offset;
    if (offset == 0.0f) return base;
    return info->domain == SYNTH_DOMAIN_LOG2 ? base * exp2f(offset) : base + offset;
}

float synth_modulate_sources(const synth *s, synth_parameter_id target,
                            float base, float lfo_value, float envelope_value)
{
    if (s == NULL || synth_parameter_info_at(target) == NULL || !isfinite(base) ||
        !isfinite(lfo_value) || !isfinite(envelope_value)) return 0.0f;
    const float value = synth_modulate_unclamped(s, target, base, lfo_value, envelope_value);
    // preserve exact inactive values, including unbounded timing parameters
    return value == base ? base : synth_parameter_clamp(target, value, s->sample_rate);
}

float synth_modulate_value(const synth *s, synth_parameter_id target,
                          float base, float lfo_value)
{
    return synth_modulate_sources(s, target, base, lfo_value, 0.0f);
}
