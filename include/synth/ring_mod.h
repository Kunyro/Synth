#ifndef SYNTH_RING_MOD_H
#define SYNTH_RING_MOD_H

#include "synth/audio_types.h"

#define SYNTH_RING_MOD_MIN_SAMPLE_RATE 1.0f
#define SYNTH_RING_MOD_MIN_FREQUENCY_HZ 20.0f
#define SYNTH_RING_MOD_MAX_FREQUENCY_HZ 10000.0f
#define SYNTH_RING_MOD_DEFAULT_FREQUENCY_HZ 440.0f
#define SYNTH_RING_MOD_MIN_RECTIFY -1.0f
#define SYNTH_RING_MOD_MAX_RECTIFY 1.0f

typedef struct synth_ring_mod {
    float sample_rate;
    float frequency_hz;
    float rectify;
    float mix;
    float phase;
} synth_ring_mod;

void synth_ring_mod_init(synth_ring_mod *ring_mod, float sample_rate);
void synth_ring_mod_set_sample_rate(synth_ring_mod *ring_mod, float sample_rate);
void synth_ring_mod_set_frequency(synth_ring_mod *ring_mod, float hz);
void synth_ring_mod_set_rectify(synth_ring_mod *ring_mod, float rectify);
void synth_ring_mod_set_mix(synth_ring_mod *ring_mod, float mix);
float synth_ring_mod_get_frequency(const synth_ring_mod *ring_mod);
float synth_ring_mod_get_rectify(const synth_ring_mod *ring_mod);
float synth_ring_mod_get_mix(const synth_ring_mod *ring_mod);
synth_stereo_sample synth_ring_mod_process(
    synth_ring_mod *ring_mod,
    synth_stereo_sample input);

#endif
