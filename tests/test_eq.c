#include "synth/delay.h"
#include "synth/effect_chain.h"
#include "synth/eq.h"
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

static float process_constant(synth_eq *eq, float input, size_t frame_count)
{
    synth_stereo_sample output = {0.0f, 0.0f};

    for (size_t i = 0; i < frame_count; ++i) {
        output = synth_eq_process(eq, (synth_stereo_sample){input, input});
    }

    return output.left;
}

static float process_alternating_peak(synth_eq *eq, float input, size_t frame_count)
{
    float peak = 0.0f;

    for (size_t i = 0; i < frame_count; ++i) {
        const float carrier = (i % 2) == 0 ? input : -input;
        const synth_stereo_sample output =
            synth_eq_process(eq, (synth_stereo_sample){carrier, carrier});
        const float level = sample_level(output);

        if (i > frame_count / 2 && level > peak) {
            peak = level;
        }
    }

    return peak;
}

static float sine_energy(synth_eq *eq, float frequency_hz, float sample_rate, size_t frame_count)
{
    float energy = 0.0f;

    for (size_t i = 0; i < frame_count; ++i) {
        const float phase = (float)i * frequency_hz / sample_rate;
        const float input = 0.10f * sinf(6.28318530717958647692f * phase);
        const synth_stereo_sample output =
            synth_eq_process(eq, (synth_stereo_sample){input, input});

        if (i > frame_count / 4) {
            energy += output.left * output.left;
        }
    }

    return energy;
}

static void test_eq_defaults_to_flat(void)
{
    synth_eq eq;
    const synth_stereo_sample input = {0.25f, -0.50f};
    synth_stereo_sample output;

    synth_eq_init(&eq, 48000.0f);
    output = synth_eq_process(&eq, input);

    expect_near(synth_eq_get_low(&eq), SYNTH_EQ_DEFAULT_GAIN_DB, 0.0001f, "eq starts with flat low band");
    expect_near(synth_eq_get_mid(&eq), SYNTH_EQ_DEFAULT_GAIN_DB, 0.0001f, "eq starts with flat mid band");
    expect_near(synth_eq_get_high(&eq), SYNTH_EQ_DEFAULT_GAIN_DB, 0.0001f, "eq starts with flat high band");
    expect_sample_near(output, input, 0.0001f, "flat eq keeps the input");
}

static void test_eq_parameters_are_bounded(void)
{
    synth_eq eq;

    synth_eq_init(&eq, 0.0f);
    expect_near(eq.sample_rate, SYNTH_EQ_MIN_SAMPLE_RATE, 0.0001f, "eq sample rate clamps during init");

    synth_eq_set_sample_rate(&eq, -1.0f);
    expect_near(eq.sample_rate, SYNTH_EQ_MIN_SAMPLE_RATE, 0.0001f, "eq sample rate clamps low");

    synth_eq_set_low(&eq, -24.0f);
    expect_near(synth_eq_get_low(&eq), SYNTH_EQ_MIN_GAIN_DB, 0.0001f, "eq low gain clamps low");

    synth_eq_set_low(&eq, 24.0f);
    expect_near(synth_eq_get_low(&eq), SYNTH_EQ_MAX_GAIN_DB, 0.0001f, "eq low gain clamps high");

    synth_eq_set_mid(&eq, -24.0f);
    expect_near(synth_eq_get_mid(&eq), SYNTH_EQ_MIN_GAIN_DB, 0.0001f, "eq mid gain clamps low");

    synth_eq_set_mid(&eq, 24.0f);
    expect_near(synth_eq_get_mid(&eq), SYNTH_EQ_MAX_GAIN_DB, 0.0001f, "eq mid gain clamps high");

    synth_eq_set_high(&eq, -24.0f);
    expect_near(synth_eq_get_high(&eq), SYNTH_EQ_MIN_GAIN_DB, 0.0001f, "eq high gain clamps low");

    synth_eq_set_high(&eq, 24.0f);
    expect_near(synth_eq_get_high(&eq), SYNTH_EQ_MAX_GAIN_DB, 0.0001f, "eq high gain clamps high");
}

static void test_eq_low_band_shapes_steady_low_content(void)
{
    synth_eq boost;
    synth_eq cut;

    synth_eq_init(&boost, 48000.0f);
    synth_eq_set_low(&boost, SYNTH_EQ_MAX_GAIN_DB);
    expect_true(
        process_constant(&boost, 0.10f, 24000) > 0.25f,
        "eq low boost raises steady low content");

    synth_eq_init(&cut, 48000.0f);
    synth_eq_set_low(&cut, SYNTH_EQ_MIN_GAIN_DB);
    expect_true(
        process_constant(&cut, 0.10f, 24000) < 0.05f,
        "eq low cut lowers steady low content");
}

static void test_eq_mid_band_shapes_center_frequency(void)
{
    synth_eq flat;
    synth_eq boost;
    synth_eq cut;
    float flat_energy;

    synth_eq_init(&flat, 48000.0f);
    flat_energy = sine_energy(&flat, SYNTH_EQ_MID_FREQUENCY_HZ, 48000.0f, 48000);

    synth_eq_init(&boost, 48000.0f);
    synth_eq_set_mid(&boost, SYNTH_EQ_MAX_GAIN_DB);
    expect_true(
        sine_energy(&boost, SYNTH_EQ_MID_FREQUENCY_HZ, 48000.0f, 48000) > flat_energy * 4.0f,
        "eq mid boost raises center-frequency energy");

    synth_eq_init(&cut, 48000.0f);
    synth_eq_set_mid(&cut, SYNTH_EQ_MIN_GAIN_DB);
    expect_true(
        sine_energy(&cut, SYNTH_EQ_MID_FREQUENCY_HZ, 48000.0f, 48000) < flat_energy * 0.35f,
        "eq mid cut lowers center-frequency energy");
}

static void test_eq_high_band_shapes_bright_content(void)
{
    synth_eq boost;
    synth_eq cut;

    synth_eq_init(&boost, 48000.0f);
    synth_eq_set_high(&boost, SYNTH_EQ_MAX_GAIN_DB);
    expect_true(
        process_alternating_peak(&boost, 0.10f, 48000) > 0.25f,
        "eq high boost raises bright content");

    synth_eq_init(&cut, 48000.0f);
    synth_eq_set_high(&cut, SYNTH_EQ_MIN_GAIN_DB);
    expect_true(
        process_alternating_peak(&cut, 0.10f, 48000) < 0.05f,
        "eq high cut lowers bright content");
}

static void test_synth_eq_accessors_route_to_effect(void)
{
    synth s;

    synth_init(&s, 48000.0f);

    synth_set_eq_low(&s, 3.0f);
    synth_set_eq_mid(&s, -4.5f);
    synth_set_eq_high(&s, 6.0f);

    expect_near(synth_get_eq_low(&s), 3.0f, 0.0001f, "synth routes eq low");
    expect_near(synth_get_eq_mid(&s), -4.5f, 0.0001f, "synth routes eq mid");
    expect_near(synth_get_eq_high(&s), 6.0f, 0.0001f, "synth routes eq high");

    synth_uninit(&s);
}

static void test_effect_chain_processes_eq_before_delay(void)
{
    synth_effect_chain chain;
    synth_stereo_sample output;

    synth_effect_chain_init(&chain, 1000.0f);
    synth_eq_set_low(&chain.eq, SYNTH_EQ_MAX_GAIN_DB);
    synth_delay_set_time(&chain.delay, 0.001f);
    synth_delay_set_feedback(&chain.delay, 0.0f);
    synth_delay_set_mix(&chain.delay, 1.0f);

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.10f, 0.10f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "delay waits one frame");

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.10f, 0.10f});
    expect_true(output.left > 0.10f, "delay receives boosted eq output");
    expect_true(output.right > 0.10f, "delay receives boosted stereo eq output");

    synth_effect_chain_uninit(&chain);
}

int main(void)
{
    test_eq_defaults_to_flat();
    test_eq_parameters_are_bounded();
    test_eq_low_band_shapes_steady_low_content();
    test_eq_mid_band_shapes_center_frequency();
    test_eq_high_band_shapes_bright_content();
    test_synth_eq_accessors_route_to_effect();
    test_effect_chain_processes_eq_before_delay();

    if (failures != 0) {
        fprintf(stderr, "%d eq test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
