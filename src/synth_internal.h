#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth/envelope.h"
#include "synth/oscillator.h"

static inline float synth_clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static inline int synth_clampi(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static inline float synth_waveform_to_morph(synth_waveform waveform)
{
    switch (waveform) {
        case SYNTH_WAVEFORM_SAW:
            return 0.5f;

        case SYNTH_WAVEFORM_SQUARE:
            return 1.0f;

        case SYNTH_WAVEFORM_SINE:
        default:
            return 0.0f;
    }
}

static inline synth_adsr synth_sanitize_adsr(synth_adsr adsr)
{
    if (adsr.attack_seconds < 0.0f) {
        adsr.attack_seconds = 0.0f;
    }

    if (adsr.decay_seconds < 0.0f) {
        adsr.decay_seconds = 0.0f;
    }

    adsr.sustain_level = synth_clampf(adsr.sustain_level, 0.0f, 1.0f);

    if (adsr.release_seconds < 0.0f) {
        adsr.release_seconds = 0.0f;
    }

    return adsr;
}

#endif
