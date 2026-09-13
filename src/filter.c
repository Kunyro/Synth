#include "synth/filter.h"

#include <math.h>

#include "internal/synth_internal.h"

// pi as a float for filter math
#define SYNTH_PI 3.14159265358979323846f
// the lowest cutoff the filter stores when the sample rate allows it
#define SYNTH_FILTER_MIN_CUTOFF_HZ 10.0f

// clamps a cutoff to the range supported by a sample rate
static float synth_filter_clamp_cutoff(float sample_rate, float cutoff_hz)
{
    const float nyquist = sample_rate * 0.5f;

    if (sample_rate <= 0.0f || nyquist <= SYNTH_FILTER_MIN_CUTOFF_HZ) {
        return nyquist > 0.0f ? nyquist : 0.0f;
    }

    return synth_clampf(cutoff_hz, SYNTH_FILTER_MIN_CUTOFF_HZ, nyquist);
}

// turns a cutoff into the per sample pull toward the input
static float synth_filter_coefficient(float sample_rate, float cutoff_hz)
{
    const float nyquist = sample_rate * 0.5f;

    if (sample_rate <= 0.0f || cutoff_hz >= nyquist) {
        return 1.0f;
    }

    // convert cutoff into the fraction of the input/state gap closed each sample
    // higher cutoff means faster following; 2*pi converts cycles to radians
    return 1.0f - expf((-2.0f * SYNTH_PI * cutoff_hz) / sample_rate);
}

// refreshes the stored cutoff and its cached coefficient
static void synth_filter_update_coefficient(synth_filter *filter)
{
    filter->cutoff_hz = synth_filter_clamp_cutoff(filter->sample_rate, filter->cutoff_hz);
    filter->coefficient = synth_filter_coefficient(filter->sample_rate, filter->cutoff_hz);
}

// runs one sample through each pole with the given coefficient
static float synth_filter_process_coefficient(synth_filter *filter, float input, float coefficient)
{
    float output = input;

    for (int pole = 0; pole < filter->pole_count; ++pole) {
        // each pole smooths the previous pole, making the slope steeper
        filter->state[pole] += coefficient * (output - filter->state[pole]);
        output = filter->state[pole];
    }

    return output;
}

// sets up the filter with a sample rate, cutoff, and cleared state
void synth_filter_init(synth_filter *filter, float sample_rate, float cutoff_hz)
{
    for (int i = 0; i < SYNTH_FILTER_MAX_POLES; ++i) {
        filter->state[i] = 0.0f;
    }

    filter->sample_rate = sample_rate;
    filter->cutoff_hz = cutoff_hz;
    synth_filter_set_poles(filter, SYNTH_FILTER_DEFAULT_POLES);
    synth_filter_update_coefficient(filter);
}

// changes the filter sample rate and updates its cached coefficient
void synth_filter_set_sample_rate(synth_filter *filter, float sample_rate)
{
    filter->sample_rate = sample_rate;
    synth_filter_update_coefficient(filter);
}

// changes the filter cutoff in hz
void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz)
{
    filter->cutoff_hz = cutoff_hz;
    synth_filter_update_coefficient(filter);
}

// changes how many one pole stages the filter uses
void synth_filter_set_poles(synth_filter *filter, int pole_count)
{
    filter->pole_count = synth_clampi(pole_count, 1, SYNTH_FILTER_MAX_POLES);
    // explicit manual edits take effect immediately, as before only lfo-driven
    // topology changes use the short blend in process_with_params()
    filter->render_poles = filter->pole_count;
    filter->transition_remaining = 0;
    for (int i = 0; i < SYNTH_FILTER_MAX_POLES; ++i) {
        filter->pole_weights[i] = i == filter->pole_count - 1 ? 1.0f : 0.0f;
    }
}

// runs one sample through the filter
float synth_filter_process(synth_filter *filter, float input)
{
    return synth_filter_process_coefficient(filter, input, filter->coefficient);
}

// runs one sample with a temporary cutoff without changing the stored cutoff
float synth_filter_process_with_cutoff(synth_filter *filter, float input, float cutoff_hz)
{
    const float effective_cutoff = synth_filter_clamp_cutoff(filter->sample_rate, cutoff_hz);
    const float coefficient = synth_filter_coefficient(filter->sample_rate, effective_cutoff);

    return synth_filter_process_coefficient(filter, input, coefficient);
}

// filters one sample with temporary cutoff/poles while retaining the stored settings
float synth_filter_process_with_params(synth_filter *filter, float input,
                                       const synth_filter_params *params)
{
    const float cutoff = synth_filter_clamp_cutoff(filter->sample_rate, params->cutoff_hz);
    const float coefficient = cutoff == filter->cutoff_hz ? filter->coefficient :
        synth_filter_coefficient(filter->sample_rate, cutoff);
    const int poles = synth_clampi(params->pole_count, 1, SYNTH_FILTER_MAX_POLES);
    float stage = input;
    float output = 0.0f;
    // with the base topology settled, preserve the manual path's stage history
    if (poles == filter->pole_count && poles == filter->render_poles &&
        filter->transition_remaining == 0) {
        return synth_filter_process_coefficient(filter, input, coefficient);
    }
    if (poles != filter->render_poles) {
        filter->render_poles = poles;
        // 0.002 seconds is a 2 ms blend: short enough to keep the step distinct,
        // while softening the sudden output jump always allow at least one frame
        filter->transition_remaining = (int)fmaxf(1.0f, filter->sample_rate * 0.002f);
    }
    // during modulation, update all stages so a newly selected stage has history
    for (int i = 0; i < SYNTH_FILTER_MAX_POLES; ++i) {
        const float target = i == poles - 1 ? 1.0f : 0.0f;
        filter->state[i] += coefficient * (stage - filter->state[i]);
        stage = filter->state[i];
        if (filter->transition_remaining > 0) {
            // close an equal part of the remaining gap each remaining frame
            // the selected stage reaches weight 1; all others reach 0 a new target
            // starts from the current weights, even if the last blend is unfinished
            filter->pole_weights[i] += (target - filter->pole_weights[i]) /
                (float)filter->transition_remaining;
        }
        output += stage * filter->pole_weights[i];
    }
    if (filter->transition_remaining > 0) --filter->transition_remaining;
    return output;
}
