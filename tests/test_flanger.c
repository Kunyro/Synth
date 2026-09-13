#include "synth/effect_chain.h"
#include "synth/flanger.h"
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

static float sample_level(synth_stereo_sample sample)
{
    const float left = fabsf(sample.left);
    const float right = fabsf(sample.right);

    return left > right ? left : right;
}

static float expected_feedback_for_intensity(float intensity)
{
    return SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK +
        (sqrtf(intensity) *
            (SYNTH_FLANGER_MAX_FEEDBACK - SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK));
}

static void test_flanger_defaults_to_dry(void)
{
    synth_flanger flanger;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_flanger_init(&flanger, 48000.0f);
    output = synth_flanger_process(&flanger, input);

    expect_near(
        synth_flanger_get_rate(&flanger),
        SYNTH_FLANGER_DEFAULT_RATE_HZ,
        0.0001f,
        "flanger starts at default rate");
    expect_near(
        synth_flanger_get_intensity(&flanger),
        SYNTH_FLANGER_DEFAULT_INTENSITY,
        0.0001f,
        "flanger starts at default intensity");
    expect_near(
        synth_flanger_get_depth(&flanger),
        SYNTH_FLANGER_DEFAULT_DEPTH,
        0.0001f,
        "flanger starts at default depth");
    expect_near(
        synth_flanger_get_feedback(&flanger),
        expected_feedback_for_intensity(SYNTH_FLANGER_DEFAULT_INTENSITY),
        0.0001f,
        "flanger starts at default feedback");
    expect_near(synth_flanger_get_mix(&flanger), 0.0f, 0.0001f, "flanger starts dry");
    expect_near(
        synth_flanger_get_manual(&flanger),
        SYNTH_FLANGER_DEFAULT_MANUAL_SECONDS,
        0.0001f,
        "flanger starts at default manual delay");
    expect_sample_near(output, input, 0.0001f, "dry flanger keeps the input");

    synth_flanger_uninit(&flanger);
}

static void test_flanger_parameters_are_bounded(void)
{
    synth_flanger flanger;

    synth_flanger_init(&flanger, 48000.0f);

    synth_flanger_set_rate(&flanger, 0.0f);
    expect_near(
        synth_flanger_get_rate(&flanger),
        SYNTH_FLANGER_MIN_RATE_HZ,
        0.0001f,
        "flanger rate clamps low");

    synth_flanger_set_rate(&flanger, 100.0f);
    expect_near(
        synth_flanger_get_rate(&flanger),
        SYNTH_FLANGER_MAX_RATE_HZ,
        0.0001f,
        "flanger rate clamps high");

    synth_flanger_set_intensity(&flanger, -1.0f);
    expect_near(
        synth_flanger_get_intensity(&flanger),
        0.0f,
        0.0001f,
        "flanger intensity clamps low");
    expect_near(synth_flanger_get_depth(&flanger), 0.0f, 0.0001f, "flanger intensity controls depth");
    expect_near(
        synth_flanger_get_feedback(&flanger),
        SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK,
        0.0001f,
        "flanger intensity low keeps a little feedback");

    synth_flanger_set_intensity(&flanger, 2.0f);
    expect_near(
        synth_flanger_get_intensity(&flanger),
        1.0f,
        0.0001f,
        "flanger intensity clamps high");
    expect_near(synth_flanger_get_depth(&flanger), 1.0f, 0.0001f, "flanger intensity maxes depth");
    expect_near(
        synth_flanger_get_feedback(&flanger),
        SYNTH_FLANGER_MAX_FEEDBACK,
        0.0001f,
        "flanger intensity maxes feedback");

    synth_flanger_set_depth(&flanger, -1.0f);
    expect_near(synth_flanger_get_depth(&flanger), 0.0f, 0.0001f, "flanger depth clamps low");

    synth_flanger_set_depth(&flanger, 2.0f);
    expect_near(synth_flanger_get_depth(&flanger), 1.0f, 0.0001f, "flanger depth clamps high");

    synth_flanger_set_feedback(&flanger, -2.0f);
    expect_near(
        synth_flanger_get_feedback(&flanger),
        -SYNTH_FLANGER_MAX_FEEDBACK,
        0.0001f,
        "flanger feedback clamps low");

    synth_flanger_set_feedback(&flanger, 2.0f);
    expect_near(
        synth_flanger_get_feedback(&flanger),
        SYNTH_FLANGER_MAX_FEEDBACK,
        0.0001f,
        "flanger feedback clamps high");

    synth_flanger_set_mix(&flanger, -1.0f);
    expect_near(synth_flanger_get_mix(&flanger), 0.0f, 0.0001f, "flanger mix clamps low");

    synth_flanger_set_mix(&flanger, 2.0f);
    expect_near(synth_flanger_get_mix(&flanger), 1.0f, 0.0001f, "flanger mix clamps high");

    synth_flanger_set_manual(&flanger, 0.0f);
    expect_near(
        synth_flanger_get_manual(&flanger),
        SYNTH_FLANGER_MIN_MANUAL_SECONDS,
        0.0001f,
        "flanger manual delay clamps low");

    synth_flanger_set_manual(&flanger, 1.0f);
    expect_near(
        synth_flanger_get_manual(&flanger),
        SYNTH_FLANGER_MAX_MANUAL_SECONDS,
        0.0001f,
        "flanger manual delay clamps high");

    synth_flanger_uninit(&flanger);
}

static void test_flanger_outputs_short_delayed_signal(void)
{
    synth_flanger flanger;
    synth_stereo_sample output;
    const synth_stereo_sample compensated_dry = {1.0f / 1.5f, 0.25f / 1.5f};
    const synth_stereo_sample compensated_wet = {-1.0f / 1.5f, -0.25f / 1.5f};

    synth_flanger_init(&flanger, 1000.0f);
    synth_flanger_set_mix(&flanger, 1.0f);
    synth_flanger_set_depth(&flanger, 0.0f);
    synth_flanger_set_manual(&flanger, 0.002f);
    synth_flanger_set_feedback(&flanger, 0.0f);

    output = synth_flanger_process(&flanger, (synth_stereo_sample){1.0f, 0.25f});
    expect_sample_near(output, compensated_dry, 0.0001f, "full mix keeps compensated dry signal");

    output = synth_flanger_process(&flanger, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "wet flanger waits for delay frames");

    output = synth_flanger_process(&flanger, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        compensated_wet,
        0.0001f,
        "wet flanger outputs an inverted delayed copy");

    synth_flanger_uninit(&flanger);
}

static void test_flanger_stereo_lfo_offsets_delay_times(void)
{
    synth_flanger flanger;
    float early_left = 0.0f;
    float early_right = 0.0f;

    synth_flanger_init(&flanger, 1000.0f);
    synth_flanger_set_rate(&flanger, SYNTH_FLANGER_MIN_RATE_HZ);
    synth_flanger_set_mix(&flanger, 1.0f);
    synth_flanger_set_depth(&flanger, 1.0f);
    synth_flanger_set_manual(&flanger, SYNTH_FLANGER_MIN_MANUAL_SECONDS);
    synth_flanger_set_feedback(&flanger, 0.0f);
    flanger.phase = 0.0f;

    (void)synth_flanger_process(&flanger, (synth_stereo_sample){1.0f, 1.0f});
    for (size_t i = 0; i < 3; ++i) {
        const synth_stereo_sample output =
            synth_flanger_process(&flanger, (synth_stereo_sample){0.0f, 0.0f});

        early_left += fabsf(output.left);
        early_right += fabsf(output.right);
    }

    expect_true(
        early_left > early_right + 0.5f,
        "stereo flanger phase offset gives the channels different delay times");

    synth_flanger_uninit(&flanger);
}

static void test_flanger_feedback_stays_bounded(void)
{
    synth_flanger flanger;
    float peak = 0.0f;

    synth_flanger_init(&flanger, 1000.0f);
    synth_flanger_set_mix(&flanger, 1.0f);
    synth_flanger_set_depth(&flanger, 0.0f);
    synth_flanger_set_manual(&flanger, 0.001f);
    synth_flanger_set_feedback(&flanger, 0.80f);

    for (size_t i = 0; i < 128; ++i) {
        const synth_stereo_sample input = i == 0 ?
            (synth_stereo_sample){1.0f, 1.0f} :
            (synth_stereo_sample){0.0f, 0.0f};
        const synth_stereo_sample output = synth_flanger_process(&flanger, input);
        const float level = sample_level(output);

        if (level > peak) {
            peak = level;
        }
    }

    expect_true(peak <= 1.01f, "flanger feedback remains bounded below unity");

    synth_flanger_uninit(&flanger);
}

static void test_synth_flanger_accessors_route_to_effect(void)
{
    synth s;

    synth_init(&s, 48000.0f);

    synth_set_flanger_rate(&s, 1.5f);
    synth_set_flanger_intensity(&s, 0.75f);
    synth_set_flanger_depth(&s, 0.75f);
    synth_set_flanger_feedback(&s, -0.25f);
    synth_set_flanger_mix(&s, 0.40f);
    synth_set_flanger_manual(&s, 0.004f);

    expect_near(synth_get_flanger_rate(&s), 1.5f, 0.0001f, "synth routes flanger rate");
    expect_near(synth_get_flanger_intensity(&s), 0.75f, 0.0001f, "synth routes flanger intensity");
    expect_near(synth_get_flanger_depth(&s), 0.75f, 0.0001f, "synth routes flanger depth");
    expect_near(
        synth_get_flanger_feedback(&s),
        -0.25f,
        0.0001f,
        "synth routes flanger feedback");
    expect_near(synth_get_flanger_mix(&s), 0.40f, 0.0001f, "synth routes flanger mix");
    expect_near(synth_get_flanger_manual(&s), 0.004f, 0.0001f, "synth routes flanger manual");

    synth_uninit(&s);
}

static void test_effect_chain_processes_flanger_before_delay(void)
{
    synth_effect_chain chain;
    synth_stereo_sample output;

    synth_effect_chain_init(&chain, 1000.0f);
    synth_flanger_set_mix(&chain.flanger, 1.0f);
    synth_flanger_set_depth(&chain.flanger, 0.0f);
    synth_flanger_set_manual(&chain.flanger, 0.001f);
    synth_flanger_set_feedback(&chain.flanger, 0.0f);
    synth_delay_set_mix(&chain.delay, 1.0f);
    synth_delay_set_time(&chain.delay, 0.002f);

    (void)synth_effect_chain_process(&chain, (synth_stereo_sample){1.0f, 1.0f});
    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        (synth_stereo_sample){0.0f, 0.0f},
        0.0001f,
        "delay waits for the flanger output");

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        (synth_stereo_sample){1.0f / 1.5f, 1.0f / 1.5f},
        0.0001f,
        "delay repeats the immediate flanger dry component");

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        (synth_stereo_sample){-1.0f / 1.5f, -1.0f / 1.5f},
        0.0001f,
        "delay repeats the inverted flanger wet component");

    synth_effect_chain_uninit(&chain);
}

int main(void)
{
    test_flanger_defaults_to_dry();
    test_flanger_parameters_are_bounded();
    test_flanger_outputs_short_delayed_signal();
    test_flanger_stereo_lfo_offsets_delay_times();
    test_flanger_feedback_stays_bounded();
    test_synth_flanger_accessors_route_to_effect();
    test_effect_chain_processes_flanger_before_delay();

    if (failures != 0) {
        fprintf(stderr, "%d flanger test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
