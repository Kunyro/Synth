#include "synth/lfo.h"

#include "internal/synth_internal.h"
#include "internal/wavetable.h"

// starts a sine lfo at phase zero.
void synth_lfo_init(synth_lfo *lfo, float frequency_hz)
{
    lfo->phase = 0.0f;
    lfo->morph = 0.0f;
    synth_lfo_set_frequency(lfo, frequency_hz);
    synth_wavetable_prepare();
}

// changes the lfo rate in cycles per second.
void synth_lfo_set_frequency(synth_lfo *lfo, float frequency_hz)
{
    lfo->frequency_hz = frequency_hz > 0.0f ? frequency_hz : 0.0f;
}

// changes the shape morph from sine through saw to square.
void synth_lfo_set_morph(synth_lfo *lfo, float morph)
{
    lfo->morph = synth_clampf(morph, 0.0f, 1.0f);
}

// moves the lfo to a normalized phase, where one cycle is 0 through 1.
void synth_lfo_reset(synth_lfo *lfo, float phase)
{
    lfo->phase = synth_wrap_phase(phase);
}

// returns the current bipolar value and advances the lfo by one sample.
float synth_lfo_advance(synth_lfo *lfo, float sample_rate)
{
    const float value = synth_wavetable_render(
        lfo->phase,
        lfo->frequency_hz,
        sample_rate,
        lfo->morph);

    if (sample_rate > 0.0f) {
        lfo->phase = synth_wrap_phase(lfo->phase + (lfo->frequency_hz / sample_rate));
    }

    return value;
}
