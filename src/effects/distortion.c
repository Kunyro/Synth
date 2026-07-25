#include "synth/distortion.h"

#include <math.h>

#include "../internal/synth_internal.h"

#define SYNTH_DISTORTION_MAX_ODD_BITE 0.12f

static float normalized_drive(float drive)
{
    return (drive - SYNTH_DISTORTION_MIN_DRIVE) /
        (SYNTH_DISTORTION_MAX_DRIVE - SYNTH_DISTORTION_MIN_DRIVE);
}

static float odd_bite_for_drive(float drive)
{
    const float amount = synth_clampf(normalized_drive(drive), 0.0f, 1.0f);

    return SYNTH_DISTORTION_MAX_ODD_BITE * powf(amount, 0.75f);
}

// tanh gives a symmetric soft clip, so it naturally produces odd harmonics.
static float soft_clip_sample(float input, float drive)
{
    const float ceiling = tanhf(drive);

    if (ceiling == 0.0f) {
        return input;
    }

    return tanhf(input * drive) / ceiling;
}

// a small cubic boost keeps the curve odd-symmetric while adding metallic bite.
static float add_odd_harmonic_bite(float input, float drive)
{
    const float amount = odd_bite_for_drive(drive);

    return synth_clampf(input + (amount * input * input * input), -1.0f, 1.0f);
}

static float distort_sample(float input, float drive)
{
    return add_odd_harmonic_bite(soft_clip_sample(input, drive), drive);
}

// blends from clean signal to fully distorted signal.
static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

void synth_distortion_init(synth_distortion *distortion)
{
    distortion->drive = SYNTH_DISTORTION_DEFAULT_DRIVE;
    distortion->mix = 0.0f;
}

void synth_distortion_set_drive(synth_distortion *distortion, float drive)
{
    distortion->drive = synth_clampf(
        drive,
        SYNTH_DISTORTION_MIN_DRIVE,
        SYNTH_DISTORTION_MAX_DRIVE);
}

void synth_distortion_set_mix(synth_distortion *distortion, float mix)
{
    distortion->mix = synth_clampf(mix, 0.0f, 1.0f);
}

float synth_distortion_get_drive(const synth_distortion *distortion)
{
    return distortion->drive;
}

float synth_distortion_get_mix(const synth_distortion *distortion)
{
    return distortion->mix;
}

synth_stereo_sample synth_distortion_process(
    const synth_distortion *distortion,
    synth_stereo_sample input)
{
    synth_stereo_sample output;

    if (distortion->mix == 0.0f) {
        return input;
    }

    output.left = mix_sample(
        input.left,
        distort_sample(input.left, distortion->drive),
        distortion->mix);
    output.right = mix_sample(
        input.right,
        distort_sample(input.right, distortion->drive),
        distortion->mix);
    return output;
}
