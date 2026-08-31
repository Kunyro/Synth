#include "synth/delay.h"
#include "synth/effect_chain.h"
#include "synth/ring_mod.h"
#include "synth/synth.h"

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

static void test_ring_mod_defaults_to_dry(void)
{
    synth_ring_mod ring_mod;
    const synth_stereo_sample input = {0.25f, -0.50f};
    synth_stereo_sample output;

    synth_ring_mod_init(&ring_mod, 48000.0f);
    output = synth_ring_mod_process(&ring_mod, input);

    expect_near(
        synth_ring_mod_get_frequency(&ring_mod),
        SYNTH_RING_MOD_DEFAULT_FREQUENCY_HZ,
        0.0001f,
        "ring mod starts at default frequency");
    expect_near(
        synth_ring_mod_get_rectify(&ring_mod),
        0.0f,
        0.0001f,
        "ring mod starts with no rectification");
    expect_near(
        synth_ring_mod_get_mix(&ring_mod),
        0.0f,
        0.0001f,
        "ring mod starts dry");
    expect_sample_near(output, input, 0.0001f, "dry ring mod keeps the carrier");
}

static void test_ring_mod_parameters_are_bounded(void)
{
    synth_ring_mod ring_mod;

    synth_ring_mod_init(&ring_mod, 0.0f);
    expect_near(
        ring_mod.sample_rate,
        SYNTH_RING_MOD_MIN_SAMPLE_RATE,
        0.0001f,
        "ring mod sample rate clamps during init");

    synth_ring_mod_set_sample_rate(&ring_mod, -1.0f);
    expect_near(
        ring_mod.sample_rate,
        SYNTH_RING_MOD_MIN_SAMPLE_RATE,
        0.0001f,
        "ring mod sample rate clamps low");

    synth_ring_mod_set_frequency(&ring_mod, 1.0f);
    expect_near(
        synth_ring_mod_get_frequency(&ring_mod),
        SYNTH_RING_MOD_MIN_FREQUENCY_HZ,
        0.0001f,
        "ring mod frequency clamps low");

    synth_ring_mod_set_frequency(&ring_mod, 20000.0f);
    expect_near(
        synth_ring_mod_get_frequency(&ring_mod),
        SYNTH_RING_MOD_MAX_FREQUENCY_HZ,
        0.0001f,
        "ring mod frequency clamps high");

    synth_ring_mod_set_rectify(&ring_mod, -2.0f);
    expect_near(
        synth_ring_mod_get_rectify(&ring_mod),
        SYNTH_RING_MOD_MIN_RECTIFY,
        0.0001f,
        "ring mod rectification clamps low");

    synth_ring_mod_set_rectify(&ring_mod, 2.0f);
    expect_near(
        synth_ring_mod_get_rectify(&ring_mod),
        SYNTH_RING_MOD_MAX_RECTIFY,
        0.0001f,
        "ring mod rectification clamps high");

    synth_ring_mod_set_mix(&ring_mod, -1.0f);
    expect_near(synth_ring_mod_get_mix(&ring_mod), 0.0f, 0.0001f, "ring mod mix clamps low");

    synth_ring_mod_set_mix(&ring_mod, 2.0f);
    expect_near(synth_ring_mod_get_mix(&ring_mod), 1.0f, 0.0001f, "ring mod mix clamps high");
}

static void test_ring_mod_multiplies_carrier_by_sine_modulator(void)
{
    synth_ring_mod ring_mod;

    synth_ring_mod_init(&ring_mod, 1760.0f);
    synth_ring_mod_set_frequency(&ring_mod, 440.0f);
    synth_ring_mod_set_mix(&ring_mod, 1.0f);

    expect_sample_near(
        synth_ring_mod_process(&ring_mod, (synth_stereo_sample){0.50f, -0.25f}),
        (synth_stereo_sample){0.0f, 0.0f},
        0.0001f,
        "ring mod starts on zero crossing");
    expect_sample_near(
        synth_ring_mod_process(&ring_mod, (synth_stereo_sample){0.50f, -0.25f}),
        (synth_stereo_sample){0.50f, -0.25f},
        0.0001f,
        "ring mod positive sine passes the carrier");
    expect_sample_near(
        synth_ring_mod_process(&ring_mod, (synth_stereo_sample){0.50f, -0.25f}),
        (synth_stereo_sample){0.0f, 0.0f},
        0.0001f,
        "ring mod returns to zero crossing");
    expect_sample_near(
        synth_ring_mod_process(&ring_mod, (synth_stereo_sample){0.50f, -0.25f}),
        (synth_stereo_sample){-0.50f, 0.25f},
        0.0001f,
        "ring mod negative sine inverts the carrier");
}

static void test_ring_mod_rectifies_the_sine_modulator(void)
{
    synth_ring_mod positive_rectify;
    synth_ring_mod negative_rectify;

    synth_ring_mod_init(&positive_rectify, 1760.0f);
    synth_ring_mod_set_frequency(&positive_rectify, 440.0f);
    synth_ring_mod_set_rectify(&positive_rectify, 1.0f);
    synth_ring_mod_set_mix(&positive_rectify, 1.0f);

    (void)synth_ring_mod_process(&positive_rectify, (synth_stereo_sample){0.50f, 0.50f});
    (void)synth_ring_mod_process(&positive_rectify, (synth_stereo_sample){0.50f, 0.50f});
    (void)synth_ring_mod_process(&positive_rectify, (synth_stereo_sample){0.50f, 0.50f});
    expect_sample_near(
        synth_ring_mod_process(&positive_rectify, (synth_stereo_sample){0.50f, -0.25f}),
        (synth_stereo_sample){0.50f, -0.25f},
        0.0001f,
        "positive full-wave rectification flips the negative sine half upward");

    synth_ring_mod_init(&negative_rectify, 1760.0f);
    synth_ring_mod_set_frequency(&negative_rectify, 440.0f);
    synth_ring_mod_set_rectify(&negative_rectify, -1.0f);
    synth_ring_mod_set_mix(&negative_rectify, 1.0f);

    (void)synth_ring_mod_process(&negative_rectify, (synth_stereo_sample){0.50f, 0.50f});
    expect_sample_near(
        synth_ring_mod_process(&negative_rectify, (synth_stereo_sample){0.50f, -0.25f}),
        (synth_stereo_sample){-0.50f, 0.25f},
        0.0001f,
        "negative full-wave rectification flips the positive sine half downward");
}

static void test_ring_mod_mix_blends_dry_and_wet(void)
{
    synth_ring_mod ring_mod;

    synth_ring_mod_init(&ring_mod, 1760.0f);
    synth_ring_mod_set_frequency(&ring_mod, 440.0f);
    synth_ring_mod_set_mix(&ring_mod, 0.25f);

    expect_sample_near(
        synth_ring_mod_process(&ring_mod, (synth_stereo_sample){0.80f, -0.40f}),
        (synth_stereo_sample){0.60f, -0.30f},
        0.0001f,
        "ring mod mix blends dry carrier with wet multiplied signal");
}

static void test_synth_ring_mod_accessors_route_to_effect(void)
{
    synth s;

    synth_init(&s, 48000.0f);

    synth_set_ring_mod_frequency(&s, 880.0f);
    synth_set_ring_mod_rectify(&s, -0.50f);
    synth_set_ring_mod_mix(&s, 0.75f);

    expect_near(synth_get_ring_mod_frequency(&s), 880.0f, 0.0001f, "synth routes ring mod frequency");
    expect_near(synth_get_ring_mod_rectify(&s), -0.50f, 0.0001f, "synth routes ring mod rectify");
    expect_near(synth_get_ring_mod_mix(&s), 0.75f, 0.0001f, "synth routes ring mod mix");

    synth_uninit(&s);
}

static void test_effect_chain_processes_ring_mod_before_delay(void)
{
    synth_effect_chain chain;
    synth_stereo_sample output;

    synth_effect_chain_init(&chain, 1000.0f);
    synth_ring_mod_set_frequency(&chain.ring_mod, 500.0f);
    synth_ring_mod_set_mix(&chain.ring_mod, 1.0f);
    chain.ring_mod.phase = 0.25f;
    synth_delay_set_time(&chain.delay, 0.001f);
    synth_delay_set_feedback(&chain.delay, 0.0f);
    synth_delay_set_mix(&chain.delay, 1.0f);

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){1.0f, 1.0f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "delay waits one frame");

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(
        output,
        (synth_stereo_sample){1.0f, 1.0f},
        0.0001f,
        "delay receives the ring-modulated carrier");

    synth_effect_chain_uninit(&chain);
}

int main(void)
{
    test_ring_mod_defaults_to_dry();
    test_ring_mod_parameters_are_bounded();
    test_ring_mod_multiplies_carrier_by_sine_modulator();
    test_ring_mod_rectifies_the_sine_modulator();
    test_ring_mod_mix_blends_dry_and_wet();
    test_synth_ring_mod_accessors_route_to_effect();
    test_effect_chain_processes_ring_mod_before_delay();

    if (failures != 0) {
        fprintf(stderr, "%d ring mod test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
