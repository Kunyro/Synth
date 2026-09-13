#include "synth/compressor.h"
#include "synth/effect_chain.h"
#include "synth/synth.h"

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

static synth_stereo_sample run_constant_signal(
    synth_compressor *compressor,
    synth_stereo_sample input,
    size_t frame_count)
{
    synth_stereo_sample output = input;

    for (size_t i = 0; i < frame_count; ++i) {
        output = synth_compressor_process(compressor, input);
    }

    return output;
}

static void test_compressor_defaults_to_neutral(void)
{
    synth_compressor compressor;
    const synth_stereo_sample input = {0.50f, -0.25f};
    synth_stereo_sample output;

    synth_compressor_init(&compressor, 48000.0f);
    output = synth_compressor_process(&compressor, input);

    expect_near(
        synth_compressor_get_threshold(&compressor),
        SYNTH_COMPRESSOR_DEFAULT_THRESHOLD_DB,
        0.0001f,
        "compressor starts at default threshold");
    expect_near(
        synth_compressor_get_ratio(&compressor),
        SYNTH_COMPRESSOR_DEFAULT_RATIO,
        0.0001f,
        "compressor starts at default ratio");
    expect_near(
        synth_compressor_get_makeup_gain(&compressor),
        SYNTH_COMPRESSOR_DEFAULT_MAKEUP_GAIN_DB,
        0.0001f,
        "compressor starts at default makeup gain");
    expect_sample_near(output, input, 0.0001f, "neutral compressor returns the input");
}

static void test_compressor_parameters_are_bounded(void)
{
    synth_compressor compressor;

    synth_compressor_init(&compressor, 48000.0f);

    synth_compressor_set_threshold(&compressor, -100.0f);
    expect_near(
        synth_compressor_get_threshold(&compressor),
        SYNTH_COMPRESSOR_MIN_THRESHOLD_DB,
        0.0001f,
        "compressor threshold clamps low");

    synth_compressor_set_threshold(&compressor, 12.0f);
    expect_near(
        synth_compressor_get_threshold(&compressor),
        SYNTH_COMPRESSOR_MAX_THRESHOLD_DB,
        0.0001f,
        "compressor threshold clamps high");

    synth_compressor_set_ratio(&compressor, 0.25f);
    expect_near(
        synth_compressor_get_ratio(&compressor),
        SYNTH_COMPRESSOR_MIN_RATIO,
        0.0001f,
        "compressor ratio clamps low");

    synth_compressor_set_ratio(&compressor, 99.0f);
    expect_near(
        synth_compressor_get_ratio(&compressor),
        SYNTH_COMPRESSOR_MAX_RATIO,
        0.0001f,
        "compressor ratio clamps high");

    synth_compressor_set_makeup_gain(&compressor, -12.0f);
    expect_near(
        synth_compressor_get_makeup_gain(&compressor),
        SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB,
        0.0001f,
        "compressor makeup gain clamps low");

    synth_compressor_set_makeup_gain(&compressor, 48.0f);
    expect_near(
        synth_compressor_get_makeup_gain(&compressor),
        SYNTH_COMPRESSOR_MAX_MAKEUP_GAIN_DB,
        0.0001f,
        "compressor makeup gain clamps high");

    synth_compressor_set_attack(&compressor, 0.0f);
    expect_near(
        synth_compressor_get_attack(&compressor),
        SYNTH_COMPRESSOR_MIN_ATTACK_SECONDS,
        0.0001f,
        "compressor attack clamps low");

    synth_compressor_set_attack(&compressor, 1.0f);
    expect_near(
        synth_compressor_get_attack(&compressor),
        SYNTH_COMPRESSOR_MAX_ATTACK_SECONDS,
        0.0001f,
        "compressor attack clamps high");

    synth_compressor_set_release(&compressor, 0.0f);
    expect_near(
        synth_compressor_get_release(&compressor),
        SYNTH_COMPRESSOR_MIN_RELEASE_SECONDS,
        0.0001f,
        "compressor release clamps low");

    synth_compressor_set_release(&compressor, 3.0f);
    expect_near(
        synth_compressor_get_release(&compressor),
        SYNTH_COMPRESSOR_MAX_RELEASE_SECONDS,
        0.0001f,
        "compressor release clamps high");
}

static void test_compressor_reduces_signal_above_threshold(void)
{
    synth_compressor compressor;
    const synth_stereo_sample input = {0.80f, -0.80f};
    synth_stereo_sample output;

    synth_compressor_init(&compressor, 48000.0f);
    synth_compressor_set_threshold(&compressor, -18.0f);
    synth_compressor_set_ratio(&compressor, 4.0f);

    output = run_constant_signal(&compressor, input, 12000);

    expect_true(
        fabsf(output.left) < fabsf(input.left) * 0.40f,
        "compressor lowers loud left signal");
    expect_true(
        fabsf(output.right) < fabsf(input.right) * 0.40f,
        "compressor lowers loud right signal");
}

static void test_compressor_leaves_signal_below_threshold_alone(void)
{
    synth_compressor compressor;
    const synth_stereo_sample input = {0.20f, -0.20f};
    synth_stereo_sample output;

    synth_compressor_init(&compressor, 48000.0f);
    synth_compressor_set_threshold(&compressor, -6.0f);
    synth_compressor_set_ratio(&compressor, 10.0f);

    output = run_constant_signal(&compressor, input, 12000);

    expect_sample_near(output, input, 0.0001f, "quiet signal stays unchanged");
}

static void test_compressor_uses_linked_stereo_gain(void)
{
    synth_compressor compressor;
    const synth_stereo_sample input = {0.80f, 0.20f};
    synth_stereo_sample output;
    float left_gain;
    float right_gain;

    synth_compressor_init(&compressor, 48000.0f);
    synth_compressor_set_threshold(&compressor, -30.0f);
    synth_compressor_set_ratio(&compressor, 8.0f);

    output = run_constant_signal(&compressor, input, 12000);
    left_gain = output.left / input.left;
    right_gain = output.right / input.right;

    expect_true(left_gain < 1.0f, "linked compressor turns both channels down");
    expect_near(left_gain, right_gain, 0.0001f, "linked compressor keeps stereo gain matched");
}

static void test_synth_compressor_accessors_route_to_effect(void)
{
    synth s;

    synth_init(&s, 48000.0f);

    synth_set_compressor_threshold(&s, -24.0f);
    synth_set_compressor_ratio(&s, 6.0f);
    synth_set_compressor_makeup_gain(&s, 3.0f);
    synth_set_compressor_attack_seconds(&s, 0.020f);
    synth_set_compressor_release_seconds(&s, 0.300f);

    expect_near(
        synth_get_compressor_threshold(&s),
        -24.0f,
        0.0001f,
        "synth threshold setter routes to compressor");
    expect_near(
        synth_get_compressor_ratio(&s),
        6.0f,
        0.0001f,
        "synth ratio setter routes to compressor");
    expect_near(
        synth_get_compressor_makeup_gain(&s),
        3.0f,
        0.0001f,
        "synth makeup gain setter routes to compressor");
    expect_near(
        synth_get_compressor_attack_seconds(&s),
        0.020f,
        0.0001f,
        "synth attack setter routes to compressor");
    expect_near(
        synth_get_compressor_release_seconds(&s),
        0.300f,
        0.0001f,
        "synth release setter routes to compressor");

    synth_uninit(&s);
}

static void test_effect_chain_runs_compressor_after_plate_reverb(void)
{
    synth_effect_chain chain;
    synth_effect_chain manual;
    const synth_stereo_sample input = {0.70f, 0.70f};

    synth_effect_chain_init(&chain, 48000.0f);
    synth_effect_chain_init(&manual, 48000.0f);

    synth_plate_reverb_set_mix(&chain.plate_reverb, 1.0f);
    synth_plate_reverb_set_mix(&manual.plate_reverb, 1.0f);
    synth_plate_reverb_set_predelay(&chain.plate_reverb, 0.0f);
    synth_plate_reverb_set_predelay(&manual.plate_reverb, 0.0f);
    synth_compressor_set_threshold(&chain.compressor, -36.0f);
    synth_compressor_set_threshold(&manual.compressor, -36.0f);
    synth_compressor_set_ratio(&chain.compressor, 12.0f);
    synth_compressor_set_ratio(&manual.compressor, 12.0f);

    for (size_t i = 0; i < 256; ++i) {
        synth_stereo_sample expected = input;
        const synth_stereo_sample actual = synth_effect_chain_process(&chain, input);

        expected = synth_saturation_process(&manual.saturation, expected);
        expected = synth_distortion_process(&manual.distortion, expected);
        expected = synth_bitcrusher_process(&manual.bitcrusher, expected);
        expected = synth_flanger_process(&manual.flanger, expected);
        expected = synth_ring_mod_process(&manual.ring_mod, expected);
        expected = synth_eq_process(&manual.eq, expected);
        expected = synth_delay_process(&manual.delay, expected);
        expected = synth_plate_reverb_process(&manual.plate_reverb, expected);
        expected = synth_compressor_process(&manual.compressor, expected);

        expect_sample_near(actual, expected, 0.0001f, "effect chain matches compressor-last order");
    }

    synth_effect_chain_uninit(&chain);
    synth_effect_chain_uninit(&manual);
}

int main(void)
{
    test_compressor_defaults_to_neutral();
    test_compressor_parameters_are_bounded();
    test_compressor_reduces_signal_above_threshold();
    test_compressor_leaves_signal_below_threshold_alone();
    test_compressor_uses_linked_stereo_gain();
    test_synth_compressor_accessors_route_to_effect();
    test_effect_chain_runs_compressor_after_plate_reverb();

    if (failures != 0) {
        fprintf(stderr, "%d compressor test(s) failed.\n", failures);
        return 1;
    }

    printf("All compressor tests passed.\n");
    return 0;
}
