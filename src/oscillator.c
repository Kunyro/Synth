#include "synth/oscillator.h"

#include <math.h>

// one full circle in radians as a float.
#define SYNTH_TAU 6.28318530717958647692f

// sets up an oscillator with a waveform and pitch.
void synth_oscillator_init(synth_oscillator *oscillator, synth_waveform waveform, float frequency)
{
    oscillator->waveform = waveform;
    oscillator->frequency = frequency;
    oscillator->phase = 0.0f;
}

// changes the oscillator wave shape.
void synth_oscillator_set_waveform(synth_oscillator *oscillator, synth_waveform waveform)
{
    oscillator->waveform = waveform;
}

// changes the oscillator pitch in hz.
void synth_oscillator_set_frequency(synth_oscillator *oscillator, float frequency)
{
    oscillator->frequency = frequency;
}

// renders one oscillator sample and moves its phase forward.
float synth_oscillator_render(synth_oscillator *oscillator, float sample_rate)
{
    float sample;

    switch (oscillator->waveform) {
        case SYNTH_WAVEFORM_SAW:
            sample = (2.0f * oscillator->phase) - 1.0f;
            break;

        case SYNTH_WAVEFORM_SQUARE:
            sample = oscillator->phase < 0.5f ? 1.0f : -1.0f;
            break;

        case SYNTH_WAVEFORM_SINE:
        default:
            sample = sinf(oscillator->phase * SYNTH_TAU);
            break;
    }

    oscillator->phase += oscillator->frequency / sample_rate;
    while (oscillator->phase >= 1.0f) {
        oscillator->phase -= 1.0f;
    }

    return sample;
}
