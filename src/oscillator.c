#include "synth/oscillator.h"

#include <math.h>

#include "synth_internal.h"
#include "wavetable.h"

// keeps the normalized phase inside one oscillator cycle.
static float wrap_phase(float phase)
{
    phase -= floorf(phase);

    if (phase < 0.0f) {
        phase += 1.0f;
    }

    return phase;
}

// sets up an oscillator with a waveform and pitch.
void synth_oscillator_init(synth_oscillator *oscillator, synth_waveform waveform, float frequency)
{
    oscillator->waveform = waveform;
    oscillator->frequency = frequency;
    oscillator->phase = 0.0f;
    oscillator->morph = synth_waveform_to_morph(waveform);
    synth_wavetable_prepare();
}

// changes the oscillator wave shape.
void synth_oscillator_set_waveform(synth_oscillator *oscillator, synth_waveform waveform)
{
    oscillator->waveform = waveform;
    oscillator->morph = synth_waveform_to_morph(waveform);
}

// changes the oscillator shape morph from sine to saw to square.
void synth_oscillator_set_morph(synth_oscillator *oscillator, float morph)
{
    oscillator->morph = synth_clampf(morph, 0.0f, 1.0f);
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
    float phase_step;

    if (sample_rate <= 0.0f) {
        return 0.0f;
    }

    sample = synth_wavetable_render(
        oscillator->phase,
        oscillator->frequency,
        sample_rate,
        oscillator->morph);

    phase_step = oscillator->frequency / sample_rate;
    oscillator->phase = wrap_phase(oscillator->phase + phase_step);

    return sample;
}
