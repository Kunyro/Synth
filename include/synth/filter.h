#ifndef SYNTH_FILTER_H
#define SYNTH_FILTER_H

typedef struct synth_filter {
    float cutoff_hz;
    float state;
} synth_filter;

void synth_filter_init(synth_filter *filter, float cutoff_hz);
void synth_filter_set_cutoff(synth_filter *filter, float cutoff_hz);
float synth_filter_process(synth_filter *filter, float input, float sample_rate);

#endif
