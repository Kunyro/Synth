#include "synth/saturation.h"
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

static void test_saturation_defaults_to_bypass(void)
{
    synth_saturation saturation;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_saturation_init(&saturation, 48000.0f);
    output = synth_saturation_process(&saturation, input);

    expect_near(
        synth_saturation_get_drive(&saturation),
        SYNTH_SATURATION_DEFAULT_DRIVE,
        0.0001f,
        "saturation starts at default drive");
    expect_near(synth_saturation_get_mix(&saturation), 0.0f, 0.0001f, "saturation starts dry");
    expect_sample_near(output, input, 0.0001f, "dry saturation returns the input");
}

static void test_saturation_parameters_are_bounded(void)
{
    synth_saturation saturation;

    synth_saturation_init(&saturation, 48000.0f);

    synth_saturation_set_drive(&saturation, 0.0f);
    expect_near(
        synth_saturation_get_drive(&saturation),
        SYNTH_SATURATION_MIN_DRIVE,
        0.0001f,
        "saturation drive clamps low");

    synth_saturation_set_drive(&saturation, 100.0f);
    expect_near(
        synth_saturation_get_drive(&saturation),
        SYNTH_SATURATION_MAX_DRIVE,
        0.0001f,
        "saturation drive clamps high");

    synth_saturation_set_mix(&saturation, -1.0f);
    expect_near(synth_saturation_get_mix(&saturation), 0.0f, 0.0001f, "saturation mix clamps low");

    synth_saturation_set_mix(&saturation, 2.0f);
    expect_near(synth_saturation_get_mix(&saturation), 1.0f, 0.0001f, "saturation mix clamps high");
}

static void test_saturation_mixes_dry_and_wet(void)
{
    synth_saturation dry_saturation;
    synth_saturation wet_saturation;
    synth_saturation mixed_saturation;
    const synth_stereo_sample input = {0.45f, -0.45f};
    synth_stereo_sample dry;
    synth_stereo_sample wet;
    synth_stereo_sample mixed;

    synth_saturation_init(&dry_saturation, 48000.0f);
    synth_saturation_init(&wet_saturation, 48000.0f);
    synth_saturation_init(&mixed_saturation, 48000.0f);

    synth_saturation_set_drive(&dry_saturation, 12.0f);
    synth_saturation_set_drive(&wet_saturation, 12.0f);
    synth_saturation_set_drive(&mixed_saturation, 12.0f);
    synth_saturation_set_mix(&dry_saturation, 0.0f);
    synth_saturation_set_mix(&wet_saturation, 1.0f);
    synth_saturation_set_mix(&mixed_saturation, 0.5f);

    dry = synth_saturation_process(&dry_saturation, input);
    wet = synth_saturation_process(&wet_saturation, input);
    mixed = synth_saturation_process(&mixed_saturation, input);

    expect_sample_near(dry, input, 0.0001f, "dry saturation mix is clean");
    expect_near(mixed.left, (dry.left + wet.left) * 0.5f, 0.0001f, "saturation blends left channel");
    expect_near(mixed.right, (dry.right + wet.right) * 0.5f, 0.0001f, "saturation blends right channel");
}

static void test_saturation_uses_asymmetric_curve(void)
{
    synth_saturation positive_saturation;
    synth_saturation negative_saturation;
    synth_stereo_sample positive;
    synth_stereo_sample negative;

    synth_saturation_init(&positive_saturation, 48000.0f);
    synth_saturation_init(&negative_saturation, 48000.0f);
    synth_saturation_set_drive(&positive_saturation, 12.0f);
    synth_saturation_set_drive(&negative_saturation, 12.0f);
    synth_saturation_set_mix(&positive_saturation, 1.0f);
    synth_saturation_set_mix(&negative_saturation, 1.0f);

    positive = synth_saturation_process(&positive_saturation, (synth_stereo_sample){0.5f, 0.5f});
    negative = synth_saturation_process(&negative_saturation, (synth_stereo_sample){-0.5f, -0.5f});

    expect_true(positive.left > 0.0f, "saturation preserves positive polarity");
    expect_true(negative.left < 0.0f, "saturation preserves negative polarity");
    expect_true(
        fabsf(positive.left + negative.left) > 0.02f,
        "saturation bends positive and negative samples differently");
    expect_true(fabsf(positive.left) <= 1.0001f, "saturation bounds positive output");
    expect_true(fabsf(negative.left) <= 1.0001f, "saturation bounds negative output");
}

static void test_saturation_emphasizes_second_harmonic(void)
{
    synth_saturation saturation;
    const int frame_count = 4096;
    const int warmup_frames = frame_count;
    const int fundamental_bin = 8;
    float second_real = 0.0f;
    float second_imag = 0.0f;
    float third_real = 0.0f;
    float third_imag = 0.0f;

    synth_saturation_init(&saturation, 48000.0f);
    synth_saturation_set_drive(&saturation, 12.0f);
    synth_saturation_set_mix(&saturation, 1.0f);

    for (int frame = 0; frame < warmup_frames; ++frame) {
        const float phase =
            2.0f * TEST_PI * (float)(fundamental_bin * frame) / (float)frame_count;
        const float input = 0.45f * sinf(phase);

        synth_saturation_process(&saturation, (synth_stereo_sample){input, input});
    }

    for (int frame = 0; frame < frame_count; ++frame) {
        const float phase =
            2.0f * TEST_PI * (float)(fundamental_bin * frame) / (float)frame_count;
        const float input = 0.45f * sinf(phase);
        const synth_stereo_sample output =
            synth_saturation_process(&saturation, (synth_stereo_sample){input, input});
        const float second_phase = 2.0f * phase;
        const float third_phase = 3.0f * phase;

        second_real += output.left * cosf(second_phase);
        second_imag += output.left * sinf(second_phase);
        third_real += output.left * cosf(third_phase);
        third_imag += output.left * sinf(third_phase);
    }

    {
        const float second_magnitude =
            sqrtf((second_real * second_real) + (second_imag * second_imag));
        const float third_magnitude =
            sqrtf((third_real * third_real) + (third_imag * third_imag));

        expect_true(
            second_magnitude > third_magnitude * 1.5f,
            "saturation emphasizes second harmonic over third harmonic");
    }
}

static void test_effect_chain_runs_saturation_stage(void)
{
    synth_effect_chain chain;
    const synth_stereo_sample input = {0.45f, -0.45f};
    synth_stereo_sample dry;
    synth_stereo_sample wet;

    synth_effect_chain_init(&chain, 48000.0f);
    dry = synth_effect_chain_process(&chain, input);
    expect_sample_near(dry, input, 0.0001f, "effect chain defaults to dry saturation");

    synth_saturation_set_drive(&chain.saturation, 12.0f);
    synth_saturation_set_mix(&chain.saturation, 1.0f);
    wet = synth_effect_chain_process(&chain, input);
    expect_true(fabsf(wet.left - input.left) > 0.0001f, "effect chain applies saturation to left");
    expect_true(fabsf(wet.right - input.right) > 0.0001f, "effect chain applies saturation to right");
}

int main(void)
{
    test_saturation_defaults_to_bypass();
    test_saturation_parameters_are_bounded();
    test_saturation_mixes_dry_and_wet();
    test_saturation_uses_asymmetric_curve();
    test_saturation_emphasizes_second_harmonic();
    test_effect_chain_runs_saturation_stage();

    if (failures != 0) {
        fprintf(stderr, "%d saturation test(s) failed\n", failures);
        return 1;
    }

    printf("All saturation tests passed.\n");
    return 0;
}
