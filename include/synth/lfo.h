#ifndef SYNTH_LFO_H
#define SYNTH_LFO_H

// a free-running low frequency oscillator with normalized phase.
typedef struct synth_lfo {
    float frequency_hz;
    float phase;
    float morph;
} synth_lfo;

// starts a sine lfo at phase zero.
void synth_lfo_init(synth_lfo *lfo, float frequency_hz);
// changes the lfo rate in cycles per second.
void synth_lfo_set_frequency(synth_lfo *lfo, float frequency_hz);
// changes the shape morph from sine through saw to square.
void synth_lfo_set_morph(synth_lfo *lfo, float morph);
// moves the lfo to a normalized phase, where one cycle is 0 through 1.
void synth_lfo_reset(synth_lfo *lfo, float phase);
// returns the current bipolar value and advances the lfo by one sample.
float synth_lfo_advance(synth_lfo *lfo, float sample_rate);

#endif
