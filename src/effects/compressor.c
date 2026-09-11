#include "synth/compressor.h"

#include <math.h>
#include <string.h>

#include "../internal/synth_internal.h"

#define SYNTH_COMPRESSOR_MIN_DETECTOR_LEVEL 0.01f

// keeps time-to-sample calculations above the module's minimum supported rate
static float safe_sample_rate(float sample_rate)
{
    if (sample_rate < SYNTH_COMPRESSOR_MIN_SAMPLE_RATE) {
        return SYNTH_COMPRESSOR_MIN_SAMPLE_RATE;
    }

    return sample_rate;
}

// converts a response time into how much of the previous detector value survives each sample
static float smoothing_coefficient(float seconds, float sample_rate)
{
    // seconds * rate is the response time in samples this exponential retains
    // about 37% of a level gap after that time; longer times follow changes more slowly
    return expf(-1.0f / (seconds * safe_sample_rate(sample_rate)));
}

// refreshes both cached response speeds from the stored attack/release settings
static void update_smoothing_coefficients(synth_compressor *compressor)
{
    compressor->render_attack_seconds = compressor->attack_seconds;
    compressor->render_release_seconds = compressor->release_seconds;
    compressor->attack_coefficient =
        smoothing_coefficient(compressor->attack_seconds, compressor->sample_rate);
    compressor->release_coefficient =
        smoothing_coefficient(compressor->release_seconds, compressor->sample_rate);
}

// converts decibels to an amplitude multiplier; 20 db corresponds to ten times the amplitude
static float db_to_linear(float db)
{
    return powf(10.0f, db / 20.0f);
}

// converts amplitude to decibels, using a quiet-signal floor to avoid taking log(0)
static float linear_to_db(float level)
{
    const float safe_level =
        level < SYNTH_COMPRESSOR_MIN_DETECTOR_LEVEL
            ? SYNTH_COMPRESSOR_MIN_DETECTOR_LEVEL
            : level;

    return 20.0f * log10f(safe_level);
}

// a linked detector listens to both channels and makes one gain decision
static float linked_input_square(synth_stereo_sample input)
{
    const float left_square = input.left * input.left;
    const float right_square = input.right * input.right;

    return (left_square + right_square) * 0.5f;
}

// attack is used when the signal gets louder; release is used as it fades
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

// the gain computer leaves quiet signals alone and turns down sound above the threshold
static float compression_gain_db(const synth_compressor_params *params, float input_db)
{
    const float over_threshold_db = input_db - params->threshold_db;

    if (params->ratio <= SYNTH_COMPRESSOR_MIN_RATIO || over_threshold_db <= 0.0f) {
        return 0.0f;
    }

    // at 4:1, only a quarter of the db above threshold remains: remove the other
    // three quarters the negative sign turns that reduction into a gain cut
    return -over_threshold_db * (1.0f - (1.0f / params->ratio));
}

// converts the smoothed energy estimate to rms level, then combines compression and makeup gain
static float current_gain(const synth_compressor_params *params, float detector_square)
{
    const float rms = sqrtf(detector_square);
    const float detector_db = linear_to_db(rms);
    const float gain_db =
        compression_gain_db(params, detector_db) + params->makeup_gain_db;

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

// applies temporary dynamics settings while preserving the detector's running level estimate
synth_stereo_sample synth_compressor_process_with_params(
    synth_compressor *compressor,
    synth_stereo_sample input,
    const synth_compressor_params *params)
{
    synth_stereo_sample output;
    float detector_square;
    // rebuild coefficients only when effective times change the detector keeps
    // its current energy estimate, so an lfo change does not restart compression
    if (params->attack_seconds != compressor->render_attack_seconds ||
        params->release_seconds != compressor->render_release_seconds) {
        compressor->attack_coefficient = smoothing_coefficient(params->attack_seconds, compressor->sample_rate);
        compressor->release_coefficient = smoothing_coefficient(params->release_seconds, compressor->sample_rate);
        compressor->render_attack_seconds = params->attack_seconds;
        compressor->render_release_seconds = params->release_seconds;
    }
    detector_square = smooth_detector_square(compressor, linked_input_square(input));
    const float gain = current_gain(params, detector_square);

    output.left = input.left * gain;
    output.right = input.right * gain;
    return output;
}

// copies stored controls into a value struct; buffers, phases, and other history stay in the effect
synth_compressor_params synth_compressor_get_params(const synth_compressor *effect)
{
    const synth_compressor_params params = {
        effect->threshold_db,
        effect->ratio,
        effect->makeup_gain_db,
        effect->attack_seconds,
        effect->release_seconds
    };
    return params;
}

// processes a sample using the stored controls through the same path used for modulation
synth_stereo_sample synth_compressor_process(
    synth_compressor *effect,
    synth_stereo_sample input)
{
    const synth_compressor_params params = synth_compressor_get_params(effect);
    return synth_compressor_process_with_params(effect, input, &params);
}
