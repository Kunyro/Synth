#include "synth/compressor.h"

#include <math.h>
#include <string.h>

#include "../internal/synth_internal.h"

#define SYNTH_COMPRESSOR_MIN_DETECTOR_LEVEL 0.01f

static float safe_sample_rate(float sample_rate)
{
    if (sample_rate < SYNTH_COMPRESSOR_MIN_SAMPLE_RATE) {
        return SYNTH_COMPRESSOR_MIN_SAMPLE_RATE;
    }

    return sample_rate;
}

static float smoothing_coefficient(float seconds, float sample_rate)
{
    return expf(-1.0f / (seconds * safe_sample_rate(sample_rate)));
}

static void update_smoothing_coefficients(synth_compressor *compressor)
{
    compressor->attack_coefficient =
        smoothing_coefficient(compressor->attack_seconds, compressor->sample_rate);
    compressor->release_coefficient =
        smoothing_coefficient(compressor->release_seconds, compressor->sample_rate);
}

static float db_to_linear(float db)
{
    return powf(10.0f, db / 20.0f);
}

static float linear_to_db(float level)
{
    const float safe_level =
        level < SYNTH_COMPRESSOR_MIN_DETECTOR_LEVEL
            ? SYNTH_COMPRESSOR_MIN_DETECTOR_LEVEL
            : level;

    return 20.0f * log10f(safe_level);
}

// a linked detector listens to both channels and makes one gain decision.
static float linked_input_square(synth_stereo_sample input)
{
    const float left_square = input.left * input.left;
    const float right_square = input.right * input.right;

    return (left_square + right_square) * 0.5f;
}

// attack is used when the signal gets louder; release is used as it fades.
static float smooth_detector_square(synth_compressor *compressor, float target_square)
{
    const float coefficient =
        target_square > compressor->detector_square
            ? compressor->attack_coefficient
            : compressor->release_coefficient;

    compressor->detector_square =
        target_square + (coefficient * (compressor->detector_square - target_square));
    return compressor->detector_square;
}

// the gain computer leaves quiet signals alone and turns down sound above the threshold.
static float compression_gain_db(const synth_compressor *compressor, float input_db)
{
    const float over_threshold_db = input_db - compressor->threshold_db;

    if (compressor->ratio <= SYNTH_COMPRESSOR_MIN_RATIO || over_threshold_db <= 0.0f) {
        return 0.0f;
    }

    return -over_threshold_db * (1.0f - (1.0f / compressor->ratio));
}

static float current_gain(const synth_compressor *compressor, float detector_square)
{
    const float rms = sqrtf(detector_square);
    const float detector_db = linear_to_db(rms);
    const float gain_db =
        compression_gain_db(compressor, detector_db) + compressor->makeup_gain_db;

    return db_to_linear(gain_db);
}

void synth_compressor_init(synth_compressor *compressor, float sample_rate)
{
    memset(compressor, 0, sizeof(*compressor));
    compressor->threshold_db = SYNTH_COMPRESSOR_DEFAULT_THRESHOLD_DB;
    compressor->ratio = SYNTH_COMPRESSOR_DEFAULT_RATIO;
    compressor->makeup_gain_db = SYNTH_COMPRESSOR_DEFAULT_MAKEUP_GAIN_DB;
    compressor->attack_seconds = SYNTH_COMPRESSOR_DEFAULT_ATTACK_SECONDS;
    compressor->release_seconds = SYNTH_COMPRESSOR_DEFAULT_RELEASE_SECONDS;
    synth_compressor_set_sample_rate(compressor, sample_rate);
}

void synth_compressor_set_sample_rate(synth_compressor *compressor, float sample_rate)
{
    compressor->sample_rate = safe_sample_rate(sample_rate);
    update_smoothing_coefficients(compressor);
    compressor->detector_square = 0.0f;
}

void synth_compressor_set_threshold(synth_compressor *compressor, float threshold_db)
{
    compressor->threshold_db = synth_clampf(
        threshold_db,
        SYNTH_COMPRESSOR_MIN_THRESHOLD_DB,
        SYNTH_COMPRESSOR_MAX_THRESHOLD_DB);
}

void synth_compressor_set_ratio(synth_compressor *compressor, float ratio)
{
    compressor->ratio = synth_clampf(
        ratio,
        SYNTH_COMPRESSOR_MIN_RATIO,
        SYNTH_COMPRESSOR_MAX_RATIO);
}

void synth_compressor_set_makeup_gain(synth_compressor *compressor, float makeup_gain_db)
{
    compressor->makeup_gain_db = synth_clampf(
        makeup_gain_db,
        SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB,
        SYNTH_COMPRESSOR_MAX_MAKEUP_GAIN_DB);
}

void synth_compressor_set_attack(synth_compressor *compressor, float seconds)
{
    compressor->attack_seconds = synth_clampf(
        seconds,
        SYNTH_COMPRESSOR_MIN_ATTACK_SECONDS,
        SYNTH_COMPRESSOR_MAX_ATTACK_SECONDS);
    update_smoothing_coefficients(compressor);
}

void synth_compressor_set_release(synth_compressor *compressor, float seconds)
{
    compressor->release_seconds = synth_clampf(
        seconds,
        SYNTH_COMPRESSOR_MIN_RELEASE_SECONDS,
        SYNTH_COMPRESSOR_MAX_RELEASE_SECONDS);
    update_smoothing_coefficients(compressor);
}

float synth_compressor_get_threshold(const synth_compressor *compressor)
{
    return compressor->threshold_db;
}

float synth_compressor_get_ratio(const synth_compressor *compressor)
{
    return compressor->ratio;
}

float synth_compressor_get_makeup_gain(const synth_compressor *compressor)
{
    return compressor->makeup_gain_db;
}

float synth_compressor_get_attack(const synth_compressor *compressor)
{
    return compressor->attack_seconds;
}

float synth_compressor_get_release(const synth_compressor *compressor)
{
    return compressor->release_seconds;
}

synth_stereo_sample synth_compressor_process(
    synth_compressor *compressor,
    synth_stereo_sample input)
{
    synth_stereo_sample output;
    const float detector_square =
        smooth_detector_square(compressor, linked_input_square(input));
    const float gain = current_gain(compressor, detector_square);

    output.left = input.left * gain;
    output.right = input.right * gain;
    return output;
}
