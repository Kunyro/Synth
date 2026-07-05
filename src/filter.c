#include "synth/filter.h"

#include <math.h>

#include "synth_internal.h"

// pi as a float for filter math.
#define SYNTH_PI 3.14159265358979323846f
// the lowest cutoff the filter stores when the sample rate allows it.
#define SYNTH_FILTER_MIN_CUTOFF_HZ 10.0f

// refreshes the coefficient derived from the current sample rate and cutoff.
static void synth_filter_update_coefficient(synth_filter *filter)
{
    const float nyquist = filter->sample_rate * 0.5f;

    if (filter->sample_rate <= 0.0f || nyquist <= SYNTH_FILTER_MIN_CUTOFF_HZ) {
        filter->cutoff_hz = nyquist > 0.0f ? nyquist : 0.0f;
        filter->coefficient = 1.0f;
        return;
    }

    filter->cutoff_hz = synth_clampf(filter->cutoff_hz, SYNTH_FILTER_MIN_CUTOFF_HZ, nyquist);
    if (filter->cutoff_hz >= nyquist) {
        filter->coefficient = 1.0f;
        return;
    }

    // this turns cutoff hz into the per sample pull toward the input.
    filter->coefficient = 1.0f - expf((-2.0f * SYNTH_PI * filter->cutoff_hz) / filter->sample_rate);
}

// sets up the filter with a sample rate, cutoff, and cleared state.
void synth_filter_init(synth_filter *filter, float sample_rate, float cutoff_hz)
{
    for (int i = 0; i < SYNTH_FILTER_MAX_POLES; ++i) {
        filter->state[i] = 0.0f;
    }

    filter->sample_rate = sample_rate;
    filter->cutoff_hz = cutoff_hz;
    filter->pole_count = SYNTH_FILTER_DEFAULT_POLES;
    synth_filter_update_coefficient(filter);
}

// changes the filter sample rate and updates its cached coefficient.
void synth_filter_set_sample_rate(synth_filter *filter, float sample_rate)
{
    filter->sample_rate = sample_rate;
    synth_filter_update_coefficient(filter);
}

// changes the filter cutoff in hz.
void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz)
{
    filter->cutoff_hz = cutoff_hz;
    synth_filter_update_coefficient(filter);
}

// changes how many one pole stages the filter uses.
void synth_filter_set_poles(synth_filter *filter, int pole_count)
{
    filter->pole_count = synth_clampi(pole_count, 1, SYNTH_FILTER_MAX_POLES);
}

// runs one sample through the filter.
float synth_filter_process(synth_filter *filter, float input)
{
    float output = input;

    for (int pole = 0; pole < filter->pole_count; ++pole) {
        // each pole smooths the previous pole, making the slope steeper.
        filter->state[pole] += filter->coefficient * (output - filter->state[pole]);
        output = filter->state[pole];
    }

    return output;
}
