#ifndef SYNTH_FILTER_H
#define SYNTH_FILTER_H

// the most one pole stages the filter can chain
#define SYNTH_FILTER_MAX_POLES 8
// the number of filter poles used by default
#define SYNTH_FILTER_DEFAULT_POLES 1

typedef struct synth_filter_params {
    float cutoff_hz;
    int pole_count;
} synth_filter_params;

// a simple low pass filter with a cached coefficient and state for each pole
typedef struct synth_filter {
    float sample_rate;
    float cutoff_hz;
    float coefficient;
    int pole_count;
    float state[SYNTH_FILTER_MAX_POLES];
    // temporary output blend: weight 1 selects a stage, weight 0 leaves it inaudible
    float pole_weights[SYNTH_FILTER_MAX_POLES];
    // the current lfo-selected topology and samples left in its short transition
    int render_poles;
    int transition_remaining;
} synth_filter;

// sets up the filter with a sample rate, cutoff, and cleared state
void synth_filter_init(synth_filter *filter, float sample_rate, float cutoff_hz);
// changes the filter sample rate and updates its cached coefficient
void synth_filter_set_sample_rate(synth_filter *filter, float sample_rate);
// changes the filter cutoff in hz
void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz);
// changes how many one pole stages the filter uses
void synth_filter_set_poles(synth_filter *filter, int pole_count);
// runs one sample through the filter
float synth_filter_process(synth_filter *filter, float input);
// runs one sample with a temporary cutoff without changing the stored cutoff
float synth_filter_process_with_cutoff(synth_filter *filter, float input, float cutoff_hz);

// temporary cutoff and integer topology; topology changes blend over 2 ms
// base settings remain unchanged, and filter history is retained
float synth_filter_process_with_params(synth_filter *filter, float input,
                                       const synth_filter_params *params);

#endif
