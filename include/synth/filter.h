#ifndef SYNTH_FILTER_H
#define SYNTH_FILTER_H

#define SYNTH_FILTER_MAX_POLES 8
#define SYNTH_FILTER_DEFAULT_POLES 1

typedef struct synth_filter {
    float cutoff_hz;
    int pole_count;
    float state[SYNTH_FILTER_MAX_POLES];
} synth_filter;

void synth_filter_init(synth_filter *filter, float cutoff_hz);
void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz);
void synth_filter_set_poles(synth_filter *filter, int pole_count);
float synth_filter_process(synth_filter *filter, float input, float sample_rate);

#endif
