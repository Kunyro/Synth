#include "synth/saturation.h"

#include <math.h>
#include <string.h>

#include "../internal/synth_internal.h"

#define SYNTH_SATURATION_TWO_PI 6.28318530717958647692f
#define SYNTH_SATURATION_MIN_BIAS 0.08f
#define SYNTH_SATURATION_MAX_BIAS 0.40f

// blends from clean signal to fully saturated signal.
static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

static float normalized_drive(float drive)
{
    return (drive - SYNTH_SATURATION_MIN_DRIVE) /
        (SYNTH_SATURATION_MAX_DRIVE - SYNTH_SATURATION_MIN_DRIVE);
}

// raises the curve bias as drive increases, which strengthens even harmonics.
static float harmonic_bias_for_drive(float drive)
{
    const float amount = synth_clampf(normalized_drive(drive), 0.0f, 1.0f);

    return SYNTH_SATURATION_MIN_BIAS +
        ((SYNTH_SATURATION_MAX_BIAS - SYNTH_SATURATION_MIN_BIAS) * amount);
}

// converts a cutoff into the feedback coefficient for a one-pole dc blocker.
static float dc_block_coefficient_for_sample_rate(float sample_rate)
{
    const float safe_sample_rate =
        sample_rate < SYNTH_SATURATION_MIN_SAMPLE_RATE
            ? SYNTH_SATURATION_MIN_SAMPLE_RATE
            : sample_rate;

    return expf(
        -SYNTH_SATURATION_TWO_PI *
        SYNTH_SATURATION_DC_BLOCK_CUTOFF_HZ /
        safe_sample_rate);
}

// removes the small dc offset left by asymmetric saturation curves.
static float block_dc(
    synth_saturation_channel *channel,
    float input,
    float coefficient)
{
    const float output =
        input - channel->previous_input + (coefficient * channel->previous_output);

    channel->previous_input = input;
    channel->previous_output = output;
    return output;
}

// tanh gives soft saturation; the positive bias makes the + and - halves bend
// differently, emphasizing even harmonics like a warm asymmetric analog stage.
static float saturate_sample(float input, float drive)
{
    const float bias = harmonic_bias_for_drive(drive);
    const float zero_offset = tanhf(bias * drive);
    const float positive_span = tanhf((1.0f + bias) * drive) - zero_offset;
    const float negative_span = zero_offset - tanhf((-1.0f + bias) * drive);
    const float ceiling = fmaxf(positive_span, negative_span);
    const float shaped = tanhf((input + bias) * drive) - zero_offset;

    if (ceiling <= 0.0f) {
        return input;
    }

    return synth_clampf(shaped / ceiling, -1.0f, 1.0f);
}

static void reset_channels(synth_saturation *saturation)
{
    memset(&saturation->left, 0, sizeof(saturation->left));
    memset(&saturation->right, 0, sizeof(saturation->right));
}

void synth_saturation_init(synth_saturation *saturation, float sample_rate)
{
    memset(saturation, 0, sizeof(*saturation));
    saturation->drive = SYNTH_SATURATION_DEFAULT_DRIVE;
    saturation->mix = 0.0f;
    synth_saturation_set_sample_rate(saturation, sample_rate);
}

void synth_saturation_set_sample_rate(synth_saturation *saturation, float sample_rate)
{
    saturation->sample_rate =
        sample_rate < SYNTH_SATURATION_MIN_SAMPLE_RATE
            ? SYNTH_SATURATION_MIN_SAMPLE_RATE
            : sample_rate;
    saturation->dc_block_coefficient =
        dc_block_coefficient_for_sample_rate(saturation->sample_rate);
    reset_channels(saturation);
}

void synth_saturation_set_drive(synth_saturation *saturation, float drive)
{
    saturation->drive = synth_clampf(
        drive,
        SYNTH_SATURATION_MIN_DRIVE,
        SYNTH_SATURATION_MAX_DRIVE);
}

void synth_saturation_set_mix(synth_saturation *saturation, float mix)
{
    saturation->mix = synth_clampf(mix, 0.0f, 1.0f);
}

float synth_saturation_get_drive(const synth_saturation *saturation)
{
    return saturation->drive;
}

float synth_saturation_get_mix(const synth_saturation *saturation)
{
    return saturation->mix;
}

synth_stereo_sample synth_saturation_process(
    synth_saturation *saturation,
    synth_stereo_sample input)
{
    synth_stereo_sample wet;
    synth_stereo_sample output;

    if (saturation->mix == 0.0f) {
        return input;
    }

    wet.left = block_dc(
        &saturation->left,
        saturate_sample(input.left, saturation->drive),
        saturation->dc_block_coefficient);
    wet.right = block_dc(
        &saturation->right,
        saturate_sample(input.right, saturation->drive),
        saturation->dc_block_coefficient);

    output.left = mix_sample(input.left, wet.left, saturation->mix);
    output.right = mix_sample(input.right, wet.right, saturation->mix);
    return output;
}
