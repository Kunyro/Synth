#include "synth/filter.h"

#include <math.h>

#define SYNTH_PI 3.14159265358979323846f

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static int clampi(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

void synth_filter_init(synth_filter *filter, float cutoff_hz)
{
    for (int i = 0; i < SYNTH_FILTER_MAX_POLES; ++i) {
        filter->state[i] = 0.0f;
    }

    filter->cutoff_hz = cutoff_hz;
    filter->pole_count = SYNTH_FILTER_DEFAULT_POLES;
}

void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz)
{
    filter->cutoff_hz = cutoff_hz;
}

void synth_filter_set_poles(synth_filter *filter, int pole_count)
{
    filter->pole_count = clampi(pole_count, 1, SYNTH_FILTER_MAX_POLES);
}

float synth_filter_process(synth_filter *filter, float input, float sample_rate)
{
    const float nyquist = sample_rate * 0.5f;
    float cutoff = clampf(filter->cutoff_hz, 10.0f, nyquist);
    float coefficient;
    float output = input;

    if (cutoff >= nyquist) {
        return input;
    }

    coefficient = 1.0f - expf((-2.0f * SYNTH_PI * cutoff) / sample_rate);

    for (int pole = 0; pole < filter->pole_count; ++pole) {
        filter->state[pole] += coefficient * (output - filter->state[pole]);
        output = filter->state[pole];
    }

    return output;
}
