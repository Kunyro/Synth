#ifndef SYNTH_FILTER_H
#define SYNTH_FILTER_H

// the most one pole stages the filter can chain.
#define SYNTH_FILTER_MAX_POLES 8
// the number of filter poles used by default.
#define SYNTH_FILTER_DEFAULT_POLES 1

// a simple low pass filter with a small state slot for each pole.
typedef struct synth_filter {
    float cutoff_hz;
    int pole_count;
    float state[SYNTH_FILTER_MAX_POLES];
} synth_filter;

// sets up the filter with a cutoff and cleared state.
void synth_filter_init(synth_filter *filter, float cutoff_hz);
// changes the filter cutoff in hz.
void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz);
// changes how many one pole stages the filter uses.
void synth_filter_set_poles(synth_filter *filter, int pole_count);
// runs one sample through the filter.
float synth_filter_process(synth_filter *filter, float input, float sample_rate);

#endif
