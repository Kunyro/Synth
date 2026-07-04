#ifndef SYNTH_WAVETABLE_H
#define SYNTH_WAVETABLE_H

// builds the shared spectral wavetable bank if it has not already been built.
void synth_wavetable_prepare(void);

// renders a bandlimited oscillator sample from the shared spectral wavetable bank.
// phase is normalized to [0, 1). morph is normalized to [0, 1], where:
//   0.0 is sine
//   0.5 is saw
//   1.0 is square
float synth_wavetable_render(float phase, float frequency, float sample_rate, float morph);

#endif
