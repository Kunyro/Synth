#include "synth/distortion.h"
#include "synth/effect_chain.h"

#include <math.h>
#include <stdio.h>

#define TEST_PI 3.14159265358979323846f

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

static void test_distortion_stays_odd_symmetric(void)
{
    synth_distortion distortion;
    synth_stereo_sample positive;
    synth_stereo_sample negative;

    synth_distortion_init(&distortion);
    synth_distortion_set_drive(&distortion, 16.0f);
    synth_distortion_set_mix(&distortion, 1.0f);

    positive = synth_distortion_process(&distortion, (synth_stereo_sample){0.4f, 0.7f});
    negative = synth_distortion_process(&distortion, (synth_stereo_sample){-0.4f, -0.7f});

    expect_near(
        positive.left + negative.left,
        0.0f,
        0.0001f,
        "distortion keeps left channel odd symmetric");
    expect_near(
        positive.right + negative.right,
        0.0f,
        0.0001f,
        "distortion keeps right channel odd symmetric");
}

static void test_distortion_emphasizes_odd_harmonics(void)
{
    synth_distortion distortion;
    const int frame_count = 4096;
    const int fundamental_bin = 8;
    float second_real = 0.0f;
    float second_imag = 0.0f;
    float third_real = 0.0f;
    float third_imag = 0.0f;

    synth_distortion_init(&distortion);
    synth_distortion_set_drive(&distortion, 16.0f);
    synth_distortion_set_mix(&distortion, 1.0f);

    for (int frame = 0; frame < frame_count; ++frame) {
        const float phase =
            2.0f * TEST_PI * (float)(fundamental_bin * frame) / (float)frame_count;
        const float input = 0.45f * sinf(phase);
        const synth_stereo_sample output =
            synth_distortion_process(&distortion, (synth_stereo_sample){input, input});
        const float second_phase = 2.0f * phase;
        const float third_phase = 3.0f * phase;

        second_real += output.left * cosf(second_phase);
        second_imag += output.left * sinf(second_phase);
        third_real += output.left * cosf(third_phase);
        third_imag += output.left * sinf(third_phase);
    }

    {
        const float second_magnitude =
            sqrtf((second_real * second_real) + (second_imag * second_imag)) * 2.0f /
            (float)frame_count;
        const float third_magnitude =
            sqrtf((third_real * third_real) + (third_imag * third_imag)) * 2.0f /
            (float)frame_count;

        expect_true(
            third_magnitude > 0.4f,
            "distortion adds a strong third harmonic");
        expect_true(
            third_magnitude > second_magnitude * 100.0f,
            "distortion favors odd harmonics over even harmonics");
    }
}

static void test_effect_chain_runs_distortion_stage(void)
{
    synth_effect_chain chain;
    const synth_stereo_sample input = {0.75f, -0.25f};
    synth_stereo_sample dry;
    synth_stereo_sample wet;

    synth_effect_chain_init(&chain, 48000.0f);
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
    test_distortion_stays_odd_symmetric();
    test_distortion_emphasizes_odd_harmonics();
    test_effect_chain_runs_distortion_stage();

    if (failures != 0) {
        fprintf(stderr, "%d distortion test(s) failed\n", failures);
        return 1;
    }

    printf("All distortion tests passed.\n");
    return 0;
}
