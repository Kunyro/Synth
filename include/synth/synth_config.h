#ifndef SYNTH_CONFIG_H
#define SYNTH_CONFIG_H

// the most voices the synth can play at once.
#define SYNTH_MAX_VOICES 8
// the default audio sample rate in hz.
#define SYNTH_DEFAULT_SAMPLE_RATE 48000.0f
// the default master output level.
#define SYNTH_DEFAULT_MASTER_GAIN 0.20f
// pitch wheel travel from center to either edge, for a two-semitone span.
#define SYNTH_PITCH_BEND_MAX_SEMITONES 1.0f

#endif
