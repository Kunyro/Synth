#ifndef SYNTH_OSCILLATOR_H
#define SYNTH_OSCILLATOR_H

// the wave shapes the oscillator can make.
typedef enum synth_waveform {
    SYNTH_WAVEFORM_SINE = 0,
    SYNTH_WAVEFORM_SAW,
    SYNTH_WAVEFORM_SQUARE
} synth_waveform;

// a small oscillator that tracks waveform, pitch, and phase.
typedef struct synth_oscillator {
    synth_waveform waveform;
    float frequency;
    float phase;
    float morph;
} synth_oscillator;

// sets up an oscillator with a waveform and pitch.
void synth_oscillator_init(synth_oscillator *oscillator, synth_waveform waveform, float frequency);
// changes the oscillator wave shape.
void synth_oscillator_set_waveform(synth_oscillator *oscillator, synth_waveform waveform);
// changes the oscillator shape morph from sine to saw to square.
void synth_oscillator_set_morph(synth_oscillator *oscillator, float morph);
// changes the oscillator pitch in hz.
void synth_oscillator_set_frequency(synth_oscillator *oscillator, float frequency);
// renders one oscillator sample and moves its phase forward.
float synth_oscillator_render(synth_oscillator *oscillator, float sample_rate);
// renders with a temporary morph value without changing the stored base morph.
float synth_oscillator_render_with_morph(
    synth_oscillator *oscillator,
    float sample_rate,
    float morph);

#endif
