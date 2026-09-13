#ifndef SYNTH_BITCRUSHER_H
#define SYNTH_BITCRUSHER_H

#include "synth/audio_types.h"

#define SYNTH_BITCRUSHER_MIN_SAMPLE_RATE 1.0f
#define SYNTH_BITCRUSHER_MIN_BITS 1
#define SYNTH_BITCRUSHER_MAX_BITS 16
#define SYNTH_BITCRUSHER_DEFAULT_BITS SYNTH_BITCRUSHER_MAX_BITS

// effective controls, separate from persistent dsp history
typedef struct synth_bitcrusher_params {
    float sample_rate;
    int bits;
    float mix;
} synth_bitcrusher_params;

typedef struct synth_bitcrusher {
    float host_sample_rate;
    float sample_rate;
    int bits;
    float mix;
    float phase;
    int has_held_sample;
    synth_stereo_sample held_sample;
} synth_bitcrusher;

void synth_bitcrusher_init(synth_bitcrusher *bitcrusher, float host_sample_rate);
void synth_bitcrusher_set_sample_rate(synth_bitcrusher *bitcrusher, float sample_rate);
void synth_bitcrusher_set_bits(synth_bitcrusher *bitcrusher, int bits);
void synth_bitcrusher_set_mix(synth_bitcrusher *bitcrusher, float mix);
float synth_bitcrusher_get_sample_rate(const synth_bitcrusher *bitcrusher);
int synth_bitcrusher_get_bits(const synth_bitcrusher *bitcrusher);
float synth_bitcrusher_get_mix(const synth_bitcrusher *bitcrusher);
synth_stereo_sample synth_bitcrusher_process(
    synth_bitcrusher *bitcrusher,
    synth_stereo_sample input);

// returns a copy of stored controls; processing overrides never change those bases
synth_bitcrusher_params synth_bitcrusher_get_params(const synth_bitcrusher *effect);
// effective controls must be finite and within the module bounds
// advances dsp history without storing controls or calling parameter setters
synth_stereo_sample synth_bitcrusher_process_with_params(
    synth_bitcrusher *effect,
    synth_stereo_sample input,
    const synth_bitcrusher_params *params);

#endif
