#include "synth/filter.h"

#include <math.h>

#include "internal/synth_internal.h"

// pi as a float for filter math.
#define SYNTH_PI 3.14159265358979323846f
// the lowest cutoff the filter stores when the sample rate allows it.
#define SYNTH_FILTER_MIN_CUTOFF_HZ 10.0f

// clamps a cutoff to the range supported by a sample rate.
static float synth_filter_clamp_cutoff(float sample_rate, float cutoff_hz)
{
    const float nyquist = sample_rate * 0.5f;

    if (sample_rate <= 0.0f || nyquist <= SYNTH_FILTER_MIN_CUTOFF_HZ) {
        return nyquist > 0.0f ? nyquist : 0.0f;
    }

    return synth_clampf(cutoff_hz, SYNTH_FILTER_MIN_CUTOFF_HZ, nyquist);
}

// turns a cutoff into the per sample pull toward the input.
static float synth_filter_coefficient(float sample_rate, float cutoff_hz)
{
    const float nyquist = sample_rate * 0.5f;

    if (sample_rate <= 0.0f || cutoff_hz >= nyquist) {
        return 1.0f;
    }

    return 1.0f - expf((-2.0f * SYNTH_PI * cutoff_hz) / sample_rate);
}

// refreshes the stored cutoff and its cached coefficient.
static void synth_filter_update_coefficient(synth_filter *filter)
{
    filter->cutoff_hz = synth_filter_clamp_cutoff(filter->sample_rate, filter->cutoff_hz);
    filter->coefficient = synth_filter_coefficient(filter->sample_rate, filter->cutoff_hz);
}

// runs one sample through each pole with the given coefficient.
static float synth_filter_process_coefficient(synth_filter *filter, float input, float coefficient)
{
    float output = input;

    for (int pole = 0; pole < filter->pole_count; ++pole) {
        // each pole smooths the previous pole, making the slope steeper.
        filter->state[pole] += coefficient * (output - filter->state[pole]);
        output = filter->state[pole];
    }

    return output;
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
    return synth_filter_process_coefficient(filter, input, filter->coefficient);
}

// runs one sample with a temporary cutoff without changing the stored cutoff.
float synth_filter_process_with_cutoff(synth_filter *filter, float input, float cutoff_hz)
{
    const float effective_cutoff = synth_filter_clamp_cutoff(filter->sample_rate, cutoff_hz);
    const float coefficient = synth_filter_coefficient(filter->sample_rate, effective_cutoff);

    return synth_filter_process_coefficient(filter, input, coefficient);
}
