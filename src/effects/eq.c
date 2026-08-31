#include "synth/eq.h"

#include <math.h>

#include "../internal/synth_internal.h"

#define SYNTH_EQ_TWO_PI 6.28318530717958647692f
#define SYNTH_EQ_NEUTRAL_GAIN_THRESHOLD 0.1f

// names the three EQ shapes this module can build.
typedef enum synth_eq_filter_type {
    SYNTH_EQ_FILTER_LOW_SHELF = 0,
    SYNTH_EQ_FILTER_PEAK,
    SYNTH_EQ_FILTER_HIGH_SHELF
} synth_eq_filter_type;

// prevents division by zero and keeps filter math valid for unusual sample rates.
static float sanitize_sample_rate(float sample_rate)
{
    return sample_rate >= SYNTH_EQ_MIN_SAMPLE_RATE ?
        sample_rate :
        SYNTH_EQ_MIN_SAMPLE_RATE;
}

// keeps the EQ band frequency below Nyquist so the digital filter stays valid.
static float clamp_frequency(float sample_rate, float frequency_hz)
{
    const float nyquist = sample_rate * 0.5f;
    const float max_frequency = nyquist > 1.0f ? nyquist - 1.0f : nyquist;

    return synth_clampf(frequency_hz, 1.0f, max_frequency);
}

// clears the two memory values each stereo channel carries between samples.
static void reset_biquad_state(synth_eq_biquad *biquad)
{
    biquad->left_z1 = 0.0f;
    biquad->left_z2 = 0.0f;
    biquad->right_z1 = 0.0f;
    biquad->right_z2 = 0.0f;
}

// makes a biquad pass audio through unchanged.
static void set_biquad_identity(synth_eq_biquad *biquad)
{
    biquad->b0 = 1.0f;
    biquad->b1 = 0.0f;
    biquad->b2 = 0.0f;
    biquad->a1 = 0.0f;
    biquad->a2 = 0.0f;
}

// divides every coefficient by a0 so processing does not need to divide per sample.
static void normalize_biquad(
    synth_eq_biquad *biquad,
    float b0,
    float b1,
    float b2,
    float a0,
    float a1,
    float a2)
{
    if (a0 == 0.0f) {
        set_biquad_identity(biquad);
        return;
    }

    biquad->b0 = b0 / a0;
    biquad->b1 = b1 / a0;
    biquad->b2 = b2 / a0;
    biquad->a1 = a1 / a0;
    biquad->a2 = a2 / a0;
}

// builds a bell filter that boosts or cuts around one center frequency.
static void configure_peak(
    synth_eq_biquad *biquad,
    float sample_rate,
    float frequency_hz,
    float q,
    float gain_db)
{
    const float frequency = clamp_frequency(sample_rate, frequency_hz);
    // omega is the center frequency expressed as radians per sample.
    const float omega = SYNTH_EQ_TWO_PI * frequency / sample_rate;
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    // amplitude is the dB gain converted into the scale used by the biquad recipe.
    const float amplitude = powf(10.0f, gain_db / 40.0f);
    // alpha controls how wide the bell is around the center frequency.
    const float alpha = sine / (2.0f * q);

    normalize_biquad(
        biquad,
        1.0f + (alpha * amplitude),
        -2.0f * cosine,
        1.0f - (alpha * amplitude),
        1.0f + (alpha / amplitude),
        -2.0f * cosine,
        1.0f - (alpha / amplitude));
}

// builds a low shelf that boosts or cuts the bass side of the spectrum.
static void configure_low_shelf(
    synth_eq_biquad *biquad,
    float sample_rate,
    float frequency_hz,
    float shelf_slope,
    float gain_db)
{
    const float frequency = clamp_frequency(sample_rate, frequency_hz);
    // omega is the shelf corner frequency expressed as radians per sample.
    const float omega = SYNTH_EQ_TWO_PI * frequency / sample_rate;
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    // amplitude is the dB gain converted into the scale used by the shelf recipe.
    const float amplitude = powf(10.0f, gain_db / 40.0f);
    const float sqrt_amplitude = sqrtf(amplitude);
    // alpha and shelf_term control how steeply the shelf moves from flat to boosted/cut.
    const float alpha = sine * 0.5f *
        sqrtf(((amplitude + (1.0f / amplitude)) * ((1.0f / shelf_slope) - 1.0f)) + 2.0f);
    const float shelf_term = 2.0f * sqrt_amplitude * alpha;

    normalize_biquad(
        biquad,
        amplitude * ((amplitude + 1.0f) - ((amplitude - 1.0f) * cosine) + shelf_term),
        2.0f * amplitude * ((amplitude - 1.0f) - ((amplitude + 1.0f) * cosine)),
        amplitude * ((amplitude + 1.0f) - ((amplitude - 1.0f) * cosine) - shelf_term),
        (amplitude + 1.0f) + ((amplitude - 1.0f) * cosine) + shelf_term,
        -2.0f * ((amplitude - 1.0f) + ((amplitude + 1.0f) * cosine)),
        (amplitude + 1.0f) + ((amplitude - 1.0f) * cosine) - shelf_term);
}

// builds a high shelf that boosts or cuts the bright side of the spectrum.
static void configure_high_shelf(
    synth_eq_biquad *biquad,
    float sample_rate,
    float frequency_hz,
    float shelf_slope,
    float gain_db)
{
    const float frequency = clamp_frequency(sample_rate, frequency_hz);
    // omega is the shelf corner frequency expressed as radians per sample.
    const float omega = SYNTH_EQ_TWO_PI * frequency / sample_rate;
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    // amplitude is the dB gain converted into the scale used by the shelf recipe.
    const float amplitude = powf(10.0f, gain_db / 40.0f);
    const float sqrt_amplitude = sqrtf(amplitude);
    // alpha and shelf_term control how steeply the shelf moves from flat to boosted/cut.
    const float alpha = sine * 0.5f *
        sqrtf(((amplitude + (1.0f / amplitude)) * ((1.0f / shelf_slope) - 1.0f)) + 2.0f);
    const float shelf_term = 2.0f * sqrt_amplitude * alpha;

    normalize_biquad(
        biquad,
        amplitude * ((amplitude + 1.0f) + ((amplitude - 1.0f) * cosine) + shelf_term),
        -2.0f * amplitude * ((amplitude - 1.0f) + ((amplitude + 1.0f) * cosine)),
        amplitude * ((amplitude + 1.0f) + ((amplitude - 1.0f) * cosine) - shelf_term),
        (amplitude + 1.0f) - ((amplitude - 1.0f) * cosine) + shelf_term,
        2.0f * ((amplitude - 1.0f) - ((amplitude + 1.0f) * cosine)),
        (amplitude + 1.0f) - ((amplitude - 1.0f) * cosine) - shelf_term);
}

// chooses the right coefficient recipe for one EQ band.
static void configure_biquad(
    synth_eq_biquad *biquad,
    synth_eq_filter_type type,
    float sample_rate,
    float frequency_hz,
    float gain_db)
{
    // tiny changes around 0 dB are treated as flat so the neutral EQ is exact pass-through.
    if (fabsf(gain_db) <= SYNTH_EQ_NEUTRAL_GAIN_THRESHOLD) {
        set_biquad_identity(biquad);
        return;
    }

    switch (type) {
        case SYNTH_EQ_FILTER_LOW_SHELF:
            configure_low_shelf(
                biquad,
                sample_rate,
                frequency_hz,
                SYNTH_EQ_SHELF_SLOPE,
                gain_db);
            break;

        case SYNTH_EQ_FILTER_HIGH_SHELF:
            configure_high_shelf(
                biquad,
                sample_rate,
                frequency_hz,
                SYNTH_EQ_SHELF_SLOPE,
                gain_db);
            break;

        case SYNTH_EQ_FILTER_PEAK:
        default:
            configure_peak(
                biquad,
                sample_rate,
                frequency_hz,
                SYNTH_EQ_MID_Q,
                gain_db);
            break;
    }
}

// refreshes all three filters after sample rate or stored gains change.
static void update_coefficients(synth_eq *eq)
{
    configure_biquad(
        &eq->low,
        SYNTH_EQ_FILTER_LOW_SHELF,
        eq->sample_rate,
        SYNTH_EQ_LOW_FREQUENCY_HZ,
        eq->low_gain_db);
    configure_biquad(
        &eq->mid,
        SYNTH_EQ_FILTER_PEAK,
        eq->sample_rate,
        SYNTH_EQ_MID_FREQUENCY_HZ,
        eq->mid_gain_db);
    configure_biquad(
        &eq->high,
        SYNTH_EQ_FILTER_HIGH_SHELF,
        eq->sample_rate,
        SYNTH_EQ_HIGH_FREQUENCY_HZ,
        eq->high_gain_db);
}

// processes one mono sample through one biquad and updates its two memory values.
static float process_biquad_sample(
    const synth_eq_biquad *coefficients,
    float input,
    float *z1,
    float *z2)
{
    const float output = (coefficients->b0 * input) + *z1;

    *z1 = (coefficients->b1 * input) - (coefficients->a1 * output) + *z2;
    *z2 = (coefficients->b2 * input) - (coefficients->a2 * output);
    return output;
}

// runs the same biquad shape on both channels, with separate left/right memory.
static synth_stereo_sample process_biquad(
    synth_eq_biquad *biquad,
    synth_stereo_sample input)
{
    synth_stereo_sample output;

    output.left = process_biquad_sample(
        biquad,
        input.left,
        &biquad->left_z1,
        &biquad->left_z2);
    output.right = process_biquad_sample(
        biquad,
        input.right,
        &biquad->right_z1,
        &biquad->right_z2);
    return output;
}

// initializes the EQ flat, with valid sample-rate-dependent filter coefficients.
void synth_eq_init(synth_eq *eq, float sample_rate)
{
    eq->sample_rate = sanitize_sample_rate(sample_rate);
    eq->low_gain_db = SYNTH_EQ_DEFAULT_GAIN_DB;
    eq->mid_gain_db = SYNTH_EQ_DEFAULT_GAIN_DB;
    eq->high_gain_db = SYNTH_EQ_DEFAULT_GAIN_DB;
    reset_biquad_state(&eq->low);
    reset_biquad_state(&eq->mid);
    reset_biquad_state(&eq->high);
    update_coefficients(eq);
}

// changes sample rate and clears old filter memory from the previous rate.
void synth_eq_set_sample_rate(synth_eq *eq, float sample_rate)
{
    eq->sample_rate = sanitize_sample_rate(sample_rate);
    reset_biquad_state(&eq->low);
    reset_biquad_state(&eq->mid);
    reset_biquad_state(&eq->high);
    update_coefficients(eq);
}

// stores the low-band gain and rebuilds only the low shelf coefficients.
void synth_eq_set_low(synth_eq *eq, float gain_db)
{
    eq->low_gain_db = synth_clampf(
        gain_db,
        SYNTH_EQ_MIN_GAIN_DB,
        SYNTH_EQ_MAX_GAIN_DB);
    configure_biquad(
        &eq->low,
        SYNTH_EQ_FILTER_LOW_SHELF,
        eq->sample_rate,
        SYNTH_EQ_LOW_FREQUENCY_HZ,
        eq->low_gain_db);
}

// stores the mid-band gain and rebuilds only the mid bell coefficients.
void synth_eq_set_mid(synth_eq *eq, float gain_db)
{
    eq->mid_gain_db = synth_clampf(
        gain_db,
        SYNTH_EQ_MIN_GAIN_DB,
        SYNTH_EQ_MAX_GAIN_DB);
    configure_biquad(
        &eq->mid,
        SYNTH_EQ_FILTER_PEAK,
        eq->sample_rate,
        SYNTH_EQ_MID_FREQUENCY_HZ,
        eq->mid_gain_db);
}

// stores the high-band gain and rebuilds only the high shelf coefficients.
void synth_eq_set_high(synth_eq *eq, float gain_db)
{
    eq->high_gain_db = synth_clampf(
        gain_db,
        SYNTH_EQ_MIN_GAIN_DB,
        SYNTH_EQ_MAX_GAIN_DB);
    configure_biquad(
        &eq->high,
        SYNTH_EQ_FILTER_HIGH_SHELF,
        eq->sample_rate,
        SYNTH_EQ_HIGH_FREQUENCY_HZ,
        eq->high_gain_db);
}

// returns the current low shelf gain in dB.
float synth_eq_get_low(const synth_eq *eq)
{
    return eq->low_gain_db;
}

// returns the current mid bell gain in dB.
float synth_eq_get_mid(const synth_eq *eq)
{
    return eq->mid_gain_db;
}

// returns the current high shelf gain in dB.
float synth_eq_get_high(const synth_eq *eq)
{
    return eq->high_gain_db;
}

// sends audio through low, mid, and high filters in that order.
synth_stereo_sample synth_eq_process(
    synth_eq *eq,
    synth_stereo_sample input)
{
    synth_stereo_sample output = input;

    output = process_biquad(&eq->low, output);
    output = process_biquad(&eq->mid, output);
    output = process_biquad(&eq->high, output);
    return output;
}
