#ifndef SYNTH_OSCILLATOR_H
#define SYNTH_OSCILLATOR_H

typedef enum synth_waveform {
    SYNTH_WAVEFORM_SINE = 0,
    SYNTH_WAVEFORM_SAW,
    SYNTH_WAVEFORM_SQUARE
} synth_waveform;

typedef struct synth_oscillator {
    synth_waveform waveform;
    float frequency;
    float phase;
} synth_oscillator;

void synth_oscillator_init(synth_oscillator *oscillator, synth_waveform waveform, float frequency);
void synth_oscillator_set_waveform(synth_oscillator *oscillator, synth_waveform waveform);
void synth_oscillator_set_frequency(synth_oscillator *oscillator, float frequency);
float synth_oscillator_render(synth_oscillator *oscillator, float sample_rate);

#endif
