#ifndef SYNTH_EQ_H
#define SYNTH_EQ_H

#include "synth/audio_types.h"

#define SYNTH_EQ_MIN_SAMPLE_RATE 1.0f
#define SYNTH_EQ_MIN_GAIN_DB -12.0f
#define SYNTH_EQ_MAX_GAIN_DB 12.0f
#define SYNTH_EQ_DEFAULT_GAIN_DB 0.0f
#define SYNTH_EQ_LOW_FREQUENCY_HZ 120.0f
#define SYNTH_EQ_MID_FREQUENCY_HZ 1000.0f
#define SYNTH_EQ_HIGH_FREQUENCY_HZ 6000.0f
#define SYNTH_EQ_MID_Q 0.70710678f
#define SYNTH_EQ_SHELF_SLOPE 1.0f

typedef struct synth_eq_biquad {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float left_z1;
    float left_z2;
    float right_z1;
    float right_z2;
} synth_eq_biquad;

typedef struct synth_eq {
    float sample_rate;
    float low_gain_db;
    float mid_gain_db;
    float high_gain_db;
    synth_eq_biquad low;
    synth_eq_biquad mid;
    synth_eq_biquad high;
} synth_eq;

void synth_eq_init(synth_eq *eq, float sample_rate);
void synth_eq_set_sample_rate(synth_eq *eq, float sample_rate);
void synth_eq_set_low(synth_eq *eq, float gain_db);
void synth_eq_set_mid(synth_eq *eq, float gain_db);
void synth_eq_set_high(synth_eq *eq, float gain_db);
float synth_eq_get_low(const synth_eq *eq);
float synth_eq_get_mid(const synth_eq *eq);
float synth_eq_get_high(const synth_eq *eq);
synth_stereo_sample synth_eq_process(
    synth_eq *eq,
    synth_stereo_sample input);

#endif
