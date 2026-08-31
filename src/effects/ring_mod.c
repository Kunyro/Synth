#include "synth/ring_mod.h"

#include <math.h>

#include "../internal/synth_internal.h"

#define SYNTH_RING_MOD_TWO_PI 6.28318530717958647692f

static float sanitize_sample_rate(float sample_rate)
{
    return sample_rate >= SYNTH_RING_MOD_MIN_SAMPLE_RATE ?
        sample_rate :
        SYNTH_RING_MOD_MIN_SAMPLE_RATE;
}

// blends the sine modulator toward positive or negative full-wave rectification.
static float rectify_modulator(float modulator, float rectify)
{
    const float full_wave = fabsf(modulator);

    if (rectify > 0.0f) {
        return modulator + ((full_wave - modulator) * rectify);
    }

    if (rectify < 0.0f) {
        const float amount = -rectify;

        return modulator + (((-full_wave) - modulator) * amount);
    }

    return modulator;
}

static float modulator_at_phase(float phase, float rectify)
{
    const float sine = sinf(synth_wrap_phase(phase) * SYNTH_RING_MOD_TWO_PI);

    return rectify_modulator(sine, rectify);
}

// blends from the clean carrier to the multiplied carrier/modulator signal.
static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

static void advance_phase(synth_ring_mod *ring_mod)
{
    ring_mod->phase = synth_wrap_phase(
        ring_mod->phase + (ring_mod->frequency_hz / ring_mod->sample_rate));
}

void synth_ring_mod_init(synth_ring_mod *ring_mod, float sample_rate)
{
    ring_mod->sample_rate = sanitize_sample_rate(sample_rate);
    ring_mod->frequency_hz = SYNTH_RING_MOD_DEFAULT_FREQUENCY_HZ;
    ring_mod->rectify = 0.0f;
    ring_mod->mix = 0.0f;
    ring_mod->phase = 0.0f;
}

void synth_ring_mod_set_sample_rate(synth_ring_mod *ring_mod, float sample_rate)
{
    ring_mod->sample_rate = sanitize_sample_rate(sample_rate);
}

void synth_ring_mod_set_frequency(synth_ring_mod *ring_mod, float hz)
{
    ring_mod->frequency_hz = synth_clampf(
        hz,
        SYNTH_RING_MOD_MIN_FREQUENCY_HZ,
        SYNTH_RING_MOD_MAX_FREQUENCY_HZ);
}

void synth_ring_mod_set_rectify(synth_ring_mod *ring_mod, float rectify)
{
    ring_mod->rectify = synth_clampf(
        rectify,
        SYNTH_RING_MOD_MIN_RECTIFY,
        SYNTH_RING_MOD_MAX_RECTIFY);
}

void synth_ring_mod_set_mix(synth_ring_mod *ring_mod, float mix)
{
    ring_mod->mix = synth_clampf(mix, 0.0f, 1.0f);
}

float synth_ring_mod_get_frequency(const synth_ring_mod *ring_mod)
{
    return ring_mod->frequency_hz;
}

float synth_ring_mod_get_rectify(const synth_ring_mod *ring_mod)
{
    return ring_mod->rectify;
}

float synth_ring_mod_get_mix(const synth_ring_mod *ring_mod)
{
    return ring_mod->mix;
}

synth_stereo_sample synth_ring_mod_process(
    synth_ring_mod *ring_mod,
    synth_stereo_sample input)
{
    synth_stereo_sample output;
    const float modulator = modulator_at_phase(ring_mod->phase, ring_mod->rectify);

    advance_phase(ring_mod);

    output.left = mix_sample(input.left, input.left * modulator, ring_mod->mix);
    output.right = mix_sample(input.right, input.right * modulator, ring_mod->mix);
    return output;
}
