#ifndef SYNTH_COMPRESSOR_H
#define SYNTH_COMPRESSOR_H

#include "synth/audio_types.h"

#define SYNTH_COMPRESSOR_MIN_THRESHOLD_DB -60.0f
#define SYNTH_COMPRESSOR_MAX_THRESHOLD_DB 0.0f
#define SYNTH_COMPRESSOR_DEFAULT_THRESHOLD_DB SYNTH_COMPRESSOR_MAX_THRESHOLD_DB
#define SYNTH_COMPRESSOR_MIN_RATIO 1.0f
#define SYNTH_COMPRESSOR_MAX_RATIO 20.0f
#define SYNTH_COMPRESSOR_DEFAULT_RATIO SYNTH_COMPRESSOR_MIN_RATIO
#define SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB 0.0f
#define SYNTH_COMPRESSOR_MAX_MAKEUP_GAIN_DB 24.0f
#define SYNTH_COMPRESSOR_DEFAULT_MAKEUP_GAIN_DB SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB
#define SYNTH_COMPRESSOR_MIN_ATTACK_SECONDS 0.001f
#define SYNTH_COMPRESSOR_MAX_ATTACK_SECONDS 0.200f
#define SYNTH_COMPRESSOR_DEFAULT_ATTACK_SECONDS 0.010f
#define SYNTH_COMPRESSOR_MIN_RELEASE_SECONDS 0.010f
#define SYNTH_COMPRESSOR_MAX_RELEASE_SECONDS 2.000f
#define SYNTH_COMPRESSOR_DEFAULT_RELEASE_SECONDS 0.100f
#define SYNTH_COMPRESSOR_MIN_SAMPLE_RATE 1.0f

typedef struct synth_compressor {
    float sample_rate;
    float threshold_db;
    float ratio;
    float makeup_gain_db;
    float attack_seconds;
    float release_seconds;
    float attack_coefficient;
    float release_coefficient;
    float detector_square;
} synth_compressor;

void synth_compressor_init(synth_compressor *compressor, float sample_rate);
void synth_compressor_set_sample_rate(synth_compressor *compressor, float sample_rate);
void synth_compressor_set_threshold(synth_compressor *compressor, float threshold_db);
void synth_compressor_set_ratio(synth_compressor *compressor, float ratio);
void synth_compressor_set_makeup_gain(synth_compressor *compressor, float makeup_gain_db);
void synth_compressor_set_attack(synth_compressor *compressor, float seconds);
void synth_compressor_set_release(synth_compressor *compressor, float seconds);
float synth_compressor_get_threshold(const synth_compressor *compressor);
float synth_compressor_get_ratio(const synth_compressor *compressor);
float synth_compressor_get_makeup_gain(const synth_compressor *compressor);
float synth_compressor_get_attack(const synth_compressor *compressor);
float synth_compressor_get_release(const synth_compressor *compressor);
synth_stereo_sample synth_compressor_process(
    synth_compressor *compressor,
    synth_stereo_sample input);

#endif
