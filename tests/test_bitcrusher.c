#include "synth/bitcrusher.h"
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

static void test_bitcrusher_defaults_to_bypass(void)
{
    synth_bitcrusher bitcrusher;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_bitcrusher_init(&bitcrusher, 48000.0f);
    output = synth_bitcrusher_process(&bitcrusher, input);

    expect_near(
        synth_bitcrusher_get_sample_rate(&bitcrusher),
        48000.0f,
        0.0001f,
        "bitcrusher starts at the host sample rate");
    expect_true(
        synth_bitcrusher_get_bits(&bitcrusher) == SYNTH_BITCRUSHER_DEFAULT_BITS,
        "bitcrusher starts at default bit depth");
    expect_near(synth_bitcrusher_get_mix(&bitcrusher), 0.0f, 0.0001f, "bitcrusher starts dry");
    expect_sample_near(output, input, 0.0001f, "dry bitcrusher returns the input");
}

static void test_bitcrusher_parameters_are_bounded(void)
{
    synth_bitcrusher bitcrusher;

    synth_bitcrusher_init(&bitcrusher, 1000.0f);

    synth_bitcrusher_set_sample_rate(&bitcrusher, 0.0f);
    expect_near(
        synth_bitcrusher_get_sample_rate(&bitcrusher),
        SYNTH_BITCRUSHER_MIN_SAMPLE_RATE,
        0.0001f,
        "bitcrusher sample rate clamps low");

    synth_bitcrusher_set_sample_rate(&bitcrusher, 48000.0f);
    expect_near(
        synth_bitcrusher_get_sample_rate(&bitcrusher),
        1000.0f,
        0.0001f,
        "bitcrusher sample rate clamps to host rate");

    synth_bitcrusher_set_bits(&bitcrusher, 0);
    expect_true(
        synth_bitcrusher_get_bits(&bitcrusher) == SYNTH_BITCRUSHER_MIN_BITS,
        "bitcrusher bits clamp low");

    synth_bitcrusher_set_bits(&bitcrusher, 32);
    expect_true(
        synth_bitcrusher_get_bits(&bitcrusher) == SYNTH_BITCRUSHER_MAX_BITS,
        "bitcrusher bits clamp high");

    synth_bitcrusher_set_mix(&bitcrusher, -1.0f);
    expect_near(synth_bitcrusher_get_mix(&bitcrusher), 0.0f, 0.0001f, "bitcrusher mix clamps low");

    synth_bitcrusher_set_mix(&bitcrusher, 2.0f);
    expect_near(synth_bitcrusher_get_mix(&bitcrusher), 1.0f, 0.0001f, "bitcrusher mix clamps high");
}

static void test_bitcrusher_quantizes_at_full_sample_rate(void)
{
    synth_bitcrusher bitcrusher;
    synth_stereo_sample output;

    synth_bitcrusher_init(&bitcrusher, 48000.0f);
    synth_bitcrusher_set_bits(&bitcrusher, 2);
    synth_bitcrusher_set_mix(&bitcrusher, 1.0f);
    output = synth_bitcrusher_process(&bitcrusher, (synth_stereo_sample){0.2f, -0.6f});

    expect_sample_near(
        output,
        (synth_stereo_sample){1.0f / 3.0f, -1.0f / 3.0f},
        0.0001f,
        "bitcrusher quantizes amplitude to bit depth");
}

static void test_bitcrusher_holds_samples_between_reduced_rate_ticks(void)
{
    synth_bitcrusher bitcrusher;
    synth_stereo_sample first;
    synth_stereo_sample second;
    synth_stereo_sample third;

    synth_bitcrusher_init(&bitcrusher, 4.0f);
    synth_bitcrusher_set_sample_rate(&bitcrusher, 2.0f);
    synth_bitcrusher_set_mix(&bitcrusher, 1.0f);

    first = synth_bitcrusher_process(&bitcrusher, (synth_stereo_sample){0.2f, -0.2f});
    second = synth_bitcrusher_process(&bitcrusher, (synth_stereo_sample){0.8f, -0.8f});
    third = synth_bitcrusher_process(&bitcrusher, (synth_stereo_sample){-0.4f, 0.4f});

    expect_sample_near(second, first, 0.0001f, "bitcrusher holds the last sample between reduced-rate ticks");
    expect_true(
        fabsf(third.left - first.left) > 0.0001f,
        "bitcrusher refreshes the held sample on the next reduced-rate tick");
}

static void test_bitcrusher_mixes_dry_and_wet(void)
{
    synth_bitcrusher bitcrusher;
    synth_stereo_sample output;

    synth_bitcrusher_init(&bitcrusher, 48000.0f);
    synth_bitcrusher_set_bits(&bitcrusher, 2);
    synth_bitcrusher_set_mix(&bitcrusher, 0.5f);
    output = synth_bitcrusher_process(&bitcrusher, (synth_stereo_sample){0.2f, -0.6f});

    expect_sample_near(
        output,
        (synth_stereo_sample){0.2666667f, -0.4666667f},
        0.0001f,
        "bitcrusher blends dry and crushed samples");
}

static void test_effect_chain_runs_bitcrusher_stage(void)
{
    synth_effect_chain chain;
    synth_stereo_sample first;
    synth_stereo_sample second;

    synth_effect_chain_init(&chain, 4.0f);
    synth_bitcrusher_set_sample_rate(&chain.bitcrusher, 2.0f);
    synth_bitcrusher_set_mix(&chain.bitcrusher, 1.0f);

    first = synth_effect_chain_process(&chain, (synth_stereo_sample){0.25f, -0.25f});
    second = synth_effect_chain_process(&chain, (synth_stereo_sample){0.75f, -0.75f});

    expect_sample_near(second, first, 0.0001f, "effect chain applies bitcrusher sample holding");
}

int main(void)
{
    test_bitcrusher_defaults_to_bypass();
    test_bitcrusher_parameters_are_bounded();
    test_bitcrusher_quantizes_at_full_sample_rate();
    test_bitcrusher_holds_samples_between_reduced_rate_ticks();
    test_bitcrusher_mixes_dry_and_wet();
    test_effect_chain_runs_bitcrusher_stage();

    if (failures != 0) {
        fprintf(stderr, "%d bitcrusher test(s) failed\n", failures);
        return 1;
    }

    printf("All bitcrusher tests passed.\n");
    return 0;
}
