#include "synth/distortion.h"

#include <math.h>

#include "../internal/synth_internal.h"

static float distort_sample(float input, float drive)
{
    const float ceiling = tanhf(drive);

    if (ceiling == 0.0f) {
        return input;
    }

    return tanhf(input * drive) / ceiling;
}

static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

void synth_distortion_init(synth_distortion *distortion)
{
    distortion->drive = SYNTH_DISTORTION_MIN_DRIVE;
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
