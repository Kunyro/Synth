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
// the rate used before a controller changes the global lfo.
#define SYNTH_DEFAULT_LFO_RATE_HZ 1.0f
// full filter modulation moves this many octaves in either direction.
#define SYNTH_LFO_FILTER_MAX_OCTAVES 8.0f

#endif
