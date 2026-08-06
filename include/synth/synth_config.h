#ifndef SYNTH_CONFIG_H
#define SYNTH_CONFIG_H

// the most voices the synth can play at once.
#define SYNTH_MAX_VOICES 12
// the default audio sample rate in hz.
#define SYNTH_DEFAULT_SAMPLE_RATE 48000.0f
// the default master output level.
#define SYNTH_DEFAULT_MASTER_GAIN 0.20f
// the linear output multiplier used when normalized master gain is at 100%.
// this corresponds to about -6dB of headroom
#define SYNTH_MASTER_GAIN_FULL_SCALE 0.5011872f
// pitch wheel travel from center to either edge, for a two-semitone span.
#define SYNTH_PITCH_BEND_MAX_SEMITONES 1.0f
// the rate used before a controller changes the global lfo.
#define SYNTH_DEFAULT_LFO_RATE_HZ 1.0f
// full filter modulation moves this many octaves in either direction.
#define SYNTH_LFO_FILTER_MAX_OCTAVES 8.0f

#endif
