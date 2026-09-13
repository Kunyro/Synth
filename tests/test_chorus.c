#include "synth/chorus.h"
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

static float sample_level(synth_stereo_sample sample)
{
    const float left = fabsf(sample.left);
    const float right = fabsf(sample.right);

    return left > right ? left : right;
}

static void test_chorus_defaults_to_dry(void)
{
    synth_chorus chorus;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_chorus_init(&chorus, 48000.0f);
    output = synth_chorus_process(&chorus, input);

    expect_near(
        synth_chorus_get_rate(&chorus),
        SYNTH_CHORUS_DEFAULT_RATE_HZ,
        0.0001f,
        "chorus starts at default rate");
    expect_near(
        synth_chorus_get_depth(&chorus),
        SYNTH_CHORUS_DEFAULT_DEPTH,
        0.0001f,
        "chorus starts at default depth");
    expect_near(synth_chorus_get_mix(&chorus), 0.0f, 0.0001f, "chorus starts dry");
    expect_near(
        synth_chorus_get_width(&chorus),
        SYNTH_CHORUS_DEFAULT_WIDTH,
        0.0001f,
        "chorus starts at default width");
    expect_near(
        synth_chorus_get_delay(&chorus),
        SYNTH_CHORUS_DEFAULT_DELAY_SECONDS,
        0.0001f,
        "chorus starts at default delay");
    expect_near(
        synth_chorus_get_feedback(&chorus),
        0.0f,
        0.0001f,
        "chorus starts without feedback");
    expect_sample_near(output, input, 0.0001f, "dry chorus keeps the input");

    synth_chorus_uninit(&chorus);
}

static void test_chorus_parameters_are_bounded(void)
{
    synth_chorus chorus;

    synth_chorus_init(&chorus, 48000.0f);

    synth_chorus_set_rate(&chorus, 0.0f);
    expect_near(
        synth_chorus_get_rate(&chorus),
        SYNTH_CHORUS_MIN_RATE_HZ,
        0.0001f,
        "chorus rate clamps low");

    synth_chorus_set_rate(&chorus, 100.0f);
    expect_near(
        synth_chorus_get_rate(&chorus),
        SYNTH_CHORUS_MAX_RATE_HZ,
        0.0001f,
        "chorus rate clamps high");

    synth_chorus_set_depth(&chorus, -1.0f);
    expect_near(synth_chorus_get_depth(&chorus), 0.0f, 0.0001f, "chorus depth clamps low");

    synth_chorus_set_depth(&chorus, 2.0f);
    expect_near(synth_chorus_get_depth(&chorus), 1.0f, 0.0001f, "chorus depth clamps high");

    synth_chorus_set_mix(&chorus, -1.0f);
    expect_near(synth_chorus_get_mix(&chorus), 0.0f, 0.0001f, "chorus mix clamps low");

    synth_chorus_set_mix(&chorus, 2.0f);
    expect_near(synth_chorus_get_mix(&chorus), 1.0f, 0.0001f, "chorus mix clamps high");

    synth_chorus_set_width(&chorus, -1.0f);
    expect_near(synth_chorus_get_width(&chorus), 0.0f, 0.0001f, "chorus width clamps low");

    synth_chorus_set_width(&chorus, 2.0f);
    expect_near(synth_chorus_get_width(&chorus), 1.0f, 0.0001f, "chorus width clamps high");

    synth_chorus_set_delay(&chorus, 0.0f);
    expect_near(
        synth_chorus_get_delay(&chorus),
        SYNTH_CHORUS_MIN_DELAY_SECONDS,
        0.0001f,
        "chorus delay clamps low");

    synth_chorus_set_delay(&chorus, 1.0f);
    expect_near(
        synth_chorus_get_delay(&chorus),
        SYNTH_CHORUS_MAX_DELAY_SECONDS,
        0.0001f,
        "chorus delay clamps high");

    synth_chorus_set_feedback(&chorus, -2.0f);
    expect_near(
        synth_chorus_get_feedback(&chorus),
        -SYNTH_CHORUS_MAX_FEEDBACK,
        0.0001f,
        "chorus feedback clamps low");

    synth_chorus_set_feedback(&chorus, 2.0f);
    expect_near(
        synth_chorus_get_feedback(&chorus),
        SYNTH_CHORUS_MAX_FEEDBACK,
        0.0001f,
        "chorus feedback clamps high");

    synth_chorus_uninit(&chorus);
}

static void test_chorus_outputs_multi_voice_delayed_signal(void)
{
    synth_chorus chorus;
    synth_stereo_sample output;
    const synth_stereo_sample compensated_dry = {1.0f / 1.65f, 0.25f / 1.65f};
    const synth_stereo_sample compensated_wet = {1.0f / 1.65f, 0.25f / 1.65f};

    synth_chorus_init(&chorus, 1000.0f);
    synth_chorus_set_mix(&chorus, 1.0f);
    synth_chorus_set_depth(&chorus, 0.0f);
    synth_chorus_set_delay(&chorus, 0.006f);
    synth_chorus_set_feedback(&chorus, 0.0f);

    output = synth_chorus_process(&chorus, (synth_stereo_sample){1.0f, 0.25f});
    expect_sample_near(output, compensated_dry, 0.0001f, "full mix keeps compensated dry signal");

    for (size_t i = 0; i < 5; ++i) {
        output = synth_chorus_process(&chorus, (synth_stereo_sample){0.0f, 0.0f});
        expect_sample_near(
            output,
            (synth_stereo_sample){0.0f, 0.0f},
            0.0001f,
            "wet chorus waits for delay frames");
    }

    output = synth_chorus_process(&chorus, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        compensated_wet,
        0.0001f,
        "wet chorus outputs a delayed multi-voice copy");

    synth_chorus_uninit(&chorus);
}

static void test_chorus_stereo_width_offsets_delay_times(void)
{
    synth_chorus chorus;
    float channel_difference = 0.0f;

    synth_chorus_init(&chorus, 1000.0f);
    synth_chorus_set_rate(&chorus, SYNTH_CHORUS_MIN_RATE_HZ);
    synth_chorus_set_mix(&chorus, 1.0f);
    synth_chorus_set_depth(&chorus, 1.0f);
    synth_chorus_set_width(&chorus, 1.0f);
    synth_chorus_set_delay(&chorus, SYNTH_CHORUS_MIN_DELAY_SECONDS);
    synth_chorus_set_feedback(&chorus, 0.0f);

    (void)synth_chorus_process(&chorus, (synth_stereo_sample){1.0f, 1.0f});
    for (size_t i = 0; i < 64; ++i) {
        const synth_stereo_sample output =
            synth_chorus_process(&chorus, (synth_stereo_sample){0.0f, 0.0f});

        channel_difference += fabsf(output.left - output.right);
    }

    expect_true(
        channel_difference > 0.10f,
        "chorus width gives the channels different delay times");

    synth_chorus_uninit(&chorus);
}

static void test_chorus_feedback_stays_bounded(void)
{
    synth_chorus chorus;
    float peak = 0.0f;

    synth_chorus_init(&chorus, 1000.0f);
    synth_chorus_set_mix(&chorus, 1.0f);
    synth_chorus_set_depth(&chorus, 0.0f);
    synth_chorus_set_delay(&chorus, 0.006f);
    synth_chorus_set_feedback(&chorus, 0.30f);

    for (size_t i = 0; i < 256; ++i) {
        const synth_stereo_sample input = i == 0 ?
            (synth_stereo_sample){1.0f, 1.0f} :
            (synth_stereo_sample){0.0f, 0.0f};
        const synth_stereo_sample output = synth_chorus_process(&chorus, input);
        const float level = sample_level(output);

        if (level > peak) {
            peak = level;
        }
    }

    expect_true(peak <= 0.62f, "chorus feedback remains bounded below dry level");

    synth_chorus_uninit(&chorus);
}

static void test_synth_chorus_accessors_route_to_effect(void)
{
    synth s;

    synth_init(&s, 48000.0f);

    synth_set_chorus_rate(&s, 1.25f);
    synth_set_chorus_depth(&s, 0.80f);
    synth_set_chorus_mix(&s, 0.40f);
    synth_set_chorus_width(&s, 0.55f);
    synth_set_chorus_delay(&s, 0.022f);
    synth_set_chorus_feedback(&s, -0.20f);

    expect_near(synth_get_chorus_rate(&s), 1.25f, 0.0001f, "synth routes chorus rate");
    expect_near(synth_get_chorus_depth(&s), 0.80f, 0.0001f, "synth routes chorus depth");
    expect_near(synth_get_chorus_mix(&s), 0.40f, 0.0001f, "synth routes chorus mix");
    expect_near(synth_get_chorus_width(&s), 0.55f, 0.0001f, "synth routes chorus width");
    expect_near(synth_get_chorus_delay(&s), 0.022f, 0.0001f, "synth routes chorus delay");
    expect_near(synth_get_chorus_feedback(&s), -0.20f, 0.0001f, "synth routes chorus feedback");

    synth_uninit(&s);
}

static void test_effect_chain_processes_chorus_before_delay(void)
{
    synth_effect_chain chain;
    synth_stereo_sample output;

    synth_effect_chain_init(&chain, 1000.0f);
    synth_chorus_set_mix(&chain.chorus, 1.0f);
    synth_chorus_set_depth(&chain.chorus, 0.0f);
    synth_chorus_set_delay(&chain.chorus, 0.006f);
    synth_chorus_set_feedback(&chain.chorus, 0.0f);
    synth_delay_set_mix(&chain.delay, 1.0f);
    synth_delay_set_time(&chain.delay, 0.002f);

    (void)synth_effect_chain_process(&chain, (synth_stereo_sample){1.0f, 1.0f});
    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        (synth_stereo_sample){0.0f, 0.0f},
        0.0001f,
        "delay waits for the chorus output");

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        (synth_stereo_sample){1.0f / 1.65f, 1.0f / 1.65f},
        0.0001f,
        "delay repeats the immediate chorus dry component");

    synth_effect_chain_uninit(&chain);
}

int main(void)
{
    test_chorus_defaults_to_dry();
    test_chorus_parameters_are_bounded();
    test_chorus_outputs_multi_voice_delayed_signal();
    test_chorus_stereo_width_offsets_delay_times();
    test_chorus_feedback_stays_bounded();
    test_synth_chorus_accessors_route_to_effect();
    test_effect_chain_processes_chorus_before_delay();

    if (failures != 0) {
        fprintf(stderr, "%d chorus test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
