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

void synth_filter_init(synth_filter *filter, float cutoff_hz)
{
    filter->cutoff_hz = cutoff_hz;
    filter->state = 0.0f;
}

void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz)
{
    filter->cutoff_hz = cutoff_hz;
}

float synth_filter_process(synth_filter *filter, float input, float sample_rate)
{
    const float nyquist = sample_rate * 0.5f;
    float cutoff = clampf(filter->cutoff_hz, 10.0f, nyquist);
    float coefficient;

    if (cutoff >= nyquist) {
        return input;
    }

    coefficient = 1.0f - expf((-2.0f * SYNTH_PI * cutoff) / sample_rate);
    filter->state += coefficient * (input - filter->state);
    return filter->state;
}
