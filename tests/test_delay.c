#include "synth/delay.h"
#include "synth/effect_chain.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

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

static void test_delay_defaults_to_dry(void)
{
    synth_delay delay;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_delay_init(&delay, 48000.0f);
    output = synth_delay_process(&delay, input);

    expect_near(synth_delay_get_time(&delay), SYNTH_DELAY_DEFAULT_TIME_SECONDS, 0.0001f, "delay starts at default time");
    expect_near(synth_delay_get_feedback(&delay), 0.0f, 0.0001f, "delay starts with no feedback");
    expect_near(synth_delay_get_mix(&delay), 0.0f, 0.0001f, "delay starts dry");
    expect_sample_near(output, input, 0.0001f, "dry delay returns the input");
}

static void test_delay_parameters_are_bounded(void)
{
    synth_delay delay;

    synth_delay_init(&delay, 48000.0f);

    synth_delay_set_time(&delay, 0.0f);
    expect_near(
        synth_delay_get_time(&delay),
        SYNTH_DELAY_MIN_TIME_SECONDS,
        0.000001f,
        "delay time clamps to the minimum time");

    synth_delay_set_time(&delay, 10.0f);
    expect_near(synth_delay_get_time(&delay), SYNTH_DELAY_MAX_TIME_SECONDS, 0.0001f, "delay time clamps high");

    synth_delay_set_feedback(&delay, -1.0f);
    expect_near(synth_delay_get_feedback(&delay), 0.0f, 0.0001f, "delay feedback clamps low");

    synth_delay_set_feedback(&delay, 2.0f);
    expect_near(synth_delay_get_feedback(&delay), SYNTH_DELAY_MAX_FEEDBACK, 0.0001f, "delay feedback clamps high");

    synth_delay_set_mix(&delay, -1.0f);
    expect_near(synth_delay_get_mix(&delay), 0.0f, 0.0001f, "delay mix clamps low");

    synth_delay_set_mix(&delay, 2.0f);
    expect_near(synth_delay_get_mix(&delay), 1.0f, 0.0001f, "delay mix clamps high");
}

static void test_delay_outputs_an_impulse_after_delay_time(void)
{
    synth_delay delay;
    synth_stereo_sample output;

    synth_delay_init(&delay, 10.0f);
    synth_delay_set_time(&delay, 0.2f);
    synth_delay_set_mix(&delay, 1.0f);

    output = synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.25f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "wet delay starts silent");

    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "wet delay waits for delay frames");

    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(output, (synth_stereo_sample){1.0f, 0.25f}, 0.0001f, "wet delay outputs the delayed stereo impulse");
}

static void test_delay_feedback_repeats_decay(void)
{
    synth_delay delay;
    synth_stereo_sample output;

    synth_delay_init(&delay, 10.0f);
    synth_delay_set_time(&delay, 0.2f);
    synth_delay_set_feedback(&delay, 0.5f);
    synth_delay_set_mix(&delay, 1.0f);

    synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.0f});
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(output.left, 1.0f, 0.0001f, "first delay repeat is full impulse");

    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(output.left, 0.5f, 0.0001f, "feedback repeat decays");
}

static void test_effect_chain_runs_delay_after_distortion(void)
{
    synth_effect_chain chain;
    synth_stereo_sample output;
    synth_stereo_sample expected;

    synth_effect_chain_init(&chain, 10.0f);
    synth_distortion_set_drive(&chain.distortion, 2.0f);
    synth_distortion_set_mix(&chain.distortion, 1.0f);
    synth_delay_set_time(&chain.delay, 0.1f);
    synth_delay_set_mix(&chain.delay, 1.0f);

    expected = synth_distortion_process(
        &chain.distortion,
        (synth_stereo_sample){0.5f, -0.5f});

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.5f, -0.5f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "delay hides the first wet sample");

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(output, expected, 0.0001f, "delay repeats the distorted sample");
}

int main(void)
{
    test_delay_defaults_to_dry();
    test_delay_parameters_are_bounded();
    test_delay_outputs_an_impulse_after_delay_time();
    test_delay_feedback_repeats_decay();
    test_effect_chain_runs_delay_after_distortion();

    if (failures != 0) {
        fprintf(stderr, "%d delay test(s) failed\n", failures);
        return 1;
    }

    printf("All delay tests passed.\n");
    return 0;
}
