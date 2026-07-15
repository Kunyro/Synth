#include "synth/distortion.h"
#include "synth/effect_chain.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

static void expect_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "FAIL: %s: got %.6f expected %.6f\n", message, actual, expected);
        ++failures;
    }
}

static void expect_sample_near(
    synth_stereo_sample actual,
    synth_stereo_sample expected,
    float tolerance,
    const char *message)
{
    expect_near(actual.left, expected.left, tolerance, message);
    expect_near(actual.right, expected.right, tolerance, message);
}

static void test_distortion_defaults_to_bypass(void)
{
    synth_distortion distortion;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_distortion_init(&distortion);
    output = synth_distortion_process(&distortion, input);

    expect_near(
        synth_distortion_get_drive(&distortion),
        SYNTH_DISTORTION_MIN_DRIVE,
        0.0001f,
        "distortion starts at minimum drive");
    expect_near(synth_distortion_get_mix(&distortion), 0.0f, 0.0001f, "distortion starts dry");
    expect_sample_near(output, input, 0.0001f, "dry distortion returns the input");
}

static void test_distortion_parameters_are_bounded(void)
{
    synth_distortion distortion;

    synth_distortion_init(&distortion);

    synth_distortion_set_drive(&distortion, 0.0f);
    expect_near(
        synth_distortion_get_drive(&distortion),
        SYNTH_DISTORTION_MIN_DRIVE,
        0.0001f,
        "distortion drive clamps low");

    synth_distortion_set_drive(&distortion, 100.0f);
    expect_near(
        synth_distortion_get_drive(&distortion),
        SYNTH_DISTORTION_MAX_DRIVE,
        0.0001f,
        "distortion drive clamps high");

    synth_distortion_set_mix(&distortion, -1.0f);
    expect_near(synth_distortion_get_mix(&distortion), 0.0f, 0.0001f, "distortion mix clamps low");

    synth_distortion_set_mix(&distortion, 2.0f);
    expect_near(synth_distortion_get_mix(&distortion), 1.0f, 0.0001f, "distortion mix clamps high");
}

static void test_distortion_shapes_large_samples(void)
{
    synth_distortion distortion;
    const synth_stereo_sample input = {2.0f, -2.0f};
    synth_stereo_sample output;

    synth_distortion_init(&distortion);
    synth_distortion_set_drive(&distortion, 16.0f);
    synth_distortion_set_mix(&distortion, 1.0f);
    output = synth_distortion_process(&distortion, input);

    expect_true(output.left > 0.0f, "distortion preserves positive sample polarity");
    expect_true(output.right < 0.0f, "distortion preserves negative sample polarity");
    expect_true(fabsf(output.left) <= 1.0001f, "distortion bounds positive output");
    expect_true(fabsf(output.right) <= 1.0001f, "distortion bounds negative output");
    expect_true(fabsf(output.left) < fabsf(input.left), "distortion compresses large positive input");
    expect_true(fabsf(output.right) < fabsf(input.right), "distortion compresses large negative input");
}

static void test_effect_chain_runs_distortion_stage(void)
{
    synth_effect_chain chain;
    const synth_stereo_sample input = {0.75f, -0.25f};
    synth_stereo_sample dry;
    synth_stereo_sample wet;

    synth_effect_chain_init(&chain);
    dry = synth_effect_chain_process(&chain, input);
    expect_sample_near(dry, input, 0.0001f, "effect chain defaults to dry");

    synth_distortion_set_drive(&chain.distortion, 8.0f);
    synth_distortion_set_mix(&chain.distortion, 1.0f);
    wet = synth_effect_chain_process(&chain, input);
    expect_true(fabsf(wet.left - input.left) > 0.0001f, "effect chain applies distortion to left");
    expect_true(fabsf(wet.right - input.right) > 0.0001f, "effect chain applies distortion to right");
}

int main(void)
{
    test_distortion_defaults_to_bypass();
    test_distortion_parameters_are_bounded();
    test_distortion_shapes_large_samples();
    test_effect_chain_runs_distortion_stage();

    if (failures != 0) {
        fprintf(stderr, "%d distortion test(s) failed\n", failures);
        return 1;
    }

    printf("All distortion tests passed.\n");
    return 0;
}
