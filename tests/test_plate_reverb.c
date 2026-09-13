#include "synth/effect_chain.h"
#include "synth/plate_reverb.h"
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

static float sample_level(synth_stereo_sample sample)
{
    const float left = fabsf(sample.left);
    const float right = fabsf(sample.right);

    return left > right ? left : right;
}

static void test_plate_reverb_defaults_to_dry(void)
{
    synth_plate_reverb reverb;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_plate_reverb_init(&reverb, 48000.0f);
    output = synth_plate_reverb_process(&reverb, input);

    expect_near(
        synth_plate_reverb_get_decay(&reverb),
        SYNTH_PLATE_REVERB_DEFAULT_DECAY_SECONDS,
        0.0001f,
        "plate reverb starts at default decay");
    expect_near(
        synth_plate_reverb_get_damping(&reverb),
        0.35f,
        0.0001f,
        "plate reverb starts at default damping");
    expect_near(
        synth_plate_reverb_get_mix(&reverb),
        0.0f,
        0.0001f,
        "plate reverb starts dry");
    expect_near(
        synth_plate_reverb_get_predelay(&reverb),
        SYNTH_PLATE_REVERB_DEFAULT_PREDELAY_SECONDS,
        0.0001f,
        "plate reverb starts at default predelay");
    expect_near(output.left, input.left, 0.0001f, "dry plate reverb keeps left input");
    expect_near(output.right, input.right, 0.0001f, "dry plate reverb keeps right input");

    synth_plate_reverb_uninit(&reverb);
}

static void test_plate_reverb_parameters_are_bounded(void)
{
    synth_plate_reverb reverb;

    synth_plate_reverb_init(&reverb, 48000.0f);

    synth_plate_reverb_set_decay(&reverb, 0.0f);
    expect_near(
        synth_plate_reverb_get_decay(&reverb),
        SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS,
        0.0001f,
        "plate reverb decay clamps low");

    synth_plate_reverb_set_decay(&reverb, 100.0f);
    expect_near(
        synth_plate_reverb_get_decay(&reverb),
        SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS,
        0.0001f,
        "plate reverb decay clamps high");

    synth_plate_reverb_set_damping(&reverb, -1.0f);
    expect_near(
        synth_plate_reverb_get_damping(&reverb),
        0.0f,
        0.0001f,
        "plate reverb damping clamps low");

    synth_plate_reverb_set_damping(&reverb, 2.0f);
    expect_near(
        synth_plate_reverb_get_damping(&reverb),
        1.0f,
        0.0001f,
        "plate reverb damping clamps high");

    synth_plate_reverb_set_mix(&reverb, -1.0f);
    expect_near(
        synth_plate_reverb_get_mix(&reverb),
        0.0f,
        0.0001f,
        "plate reverb mix clamps low");

    synth_plate_reverb_set_mix(&reverb, 2.0f);
    expect_near(
        synth_plate_reverb_get_mix(&reverb),
        1.0f,
        0.0001f,
        "plate reverb mix clamps high");

    synth_plate_reverb_set_predelay(&reverb, -1.0f);
    expect_near(
        synth_plate_reverb_get_predelay(&reverb),
        0.0f,
        0.0001f,
        "plate reverb predelay clamps low");

    synth_plate_reverb_set_predelay(&reverb, 1.0f);
    expect_near(
        synth_plate_reverb_get_predelay(&reverb),
        SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS,
        0.0001f,
        "plate reverb predelay clamps high");

    synth_plate_reverb_uninit(&reverb);
}

static void test_plate_reverb_outputs_stereo_tail_after_impulse(void)
{
    synth_plate_reverb reverb;
    float left_energy = 0.0f;
    float right_energy = 0.0f;

    synth_plate_reverb_init(&reverb, 29761.0f);
    synth_plate_reverb_set_mix(&reverb, 1.0f);
    synth_plate_reverb_set_decay(&reverb, 2.0f);

    (void)synth_plate_reverb_process(&reverb, (synth_stereo_sample){1.0f, 1.0f});
    for (size_t i = 0; i < 8000; ++i) {
        const synth_stereo_sample output =
            synth_plate_reverb_process(&reverb, (synth_stereo_sample){0.0f, 0.0f});

        left_energy += fabsf(output.left);
        right_energy += fabsf(output.right);
    }

    expect_true(left_energy > 0.001f, "plate reverb impulse creates left tail energy");
    expect_true(right_energy > 0.001f, "plate reverb impulse creates right tail energy");
    expect_true(
        fabsf(left_energy - right_energy) > 0.0001f,
        "plate reverb tank produces stereo differences");

    synth_plate_reverb_uninit(&reverb);
}

static void test_plate_reverb_tail_decays(void)
{
    synth_plate_reverb reverb;
    float early_energy = 0.0f;
    float late_energy = 0.0f;

    synth_plate_reverb_init(&reverb, 1000.0f);
    synth_plate_reverb_set_mix(&reverb, 1.0f);
    synth_plate_reverb_set_decay(&reverb, 0.30f);

    (void)synth_plate_reverb_process(&reverb, (synth_stereo_sample){1.0f, 1.0f});
    for (size_t i = 0; i < 3000; ++i) {
        const synth_stereo_sample output =
            synth_plate_reverb_process(&reverb, (synth_stereo_sample){0.0f, 0.0f});
        const float level = sample_level(output);

        if (i >= 200 && i < 800) {
            early_energy += level;
        }

        if (i >= 2200 && i < 2800) {
            late_energy += level;
        }
    }

    expect_true(early_energy > 0.001f, "plate reverb has measurable early tail");
    expect_true(late_energy < early_energy, "plate reverb tail fades over time");

    synth_plate_reverb_uninit(&reverb);
}

static void test_plate_reverb_stays_stable_with_silence(void)
{
    synth_plate_reverb reverb;

    synth_plate_reverb_init(&reverb, 48000.0f);
    synth_plate_reverb_set_mix(&reverb, 1.0f);

    for (size_t i = 0; i < 1024; ++i) {
        const synth_stereo_sample output =
            synth_plate_reverb_process(&reverb, (synth_stereo_sample){0.0f, 0.0f});

        expect_near(output.left, 0.0f, 0.0001f, "silent plate reverb keeps left silent");
        expect_near(output.right, 0.0f, 0.0001f, "silent plate reverb keeps right silent");
    }

    synth_plate_reverb_uninit(&reverb);
}

static void test_plate_reverb_sample_rate_resizes_delay_lines(void)
{
    synth_plate_reverb reverb;
    size_t low_rate_capacity;
    size_t high_rate_capacity;

    synth_plate_reverb_init(&reverb, 24000.0f);
    low_rate_capacity = reverb.tank.left_delay_1.capacity_frames;

    synth_plate_reverb_set_sample_rate(&reverb, 48000.0f);
    high_rate_capacity = reverb.tank.left_delay_1.capacity_frames;

    expect_true(
        high_rate_capacity > low_rate_capacity,
        "plate reverb delay line lengths track sample rate");
    expect_true(
        reverb.predelay.capacity_frames >=
            (size_t)(SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS * 48000.0f),
        "plate reverb predelay capacity tracks sample rate");

    synth_plate_reverb_uninit(&reverb);
}

static void test_synth_plate_reverb_accessors_route_to_effect(void)
{
    synth s;

    synth_init(&s, 48000.0f);

    synth_set_plate_reverb_decay(&s, 4.0f);
    synth_set_plate_reverb_damping(&s, 0.75f);
    synth_set_plate_reverb_mix(&s, 0.40f);
    synth_set_plate_reverb_predelay(&s, 0.05f);

    expect_near(synth_get_plate_reverb_decay(&s), 4.0f, 0.0001f, "synth routes plate reverb decay");
    expect_near(synth_get_plate_reverb_damping(&s), 0.75f, 0.0001f, "synth routes plate reverb damping");
    expect_near(synth_get_plate_reverb_mix(&s), 0.40f, 0.0001f, "synth routes plate reverb mix");
    expect_near(synth_get_plate_reverb_predelay(&s), 0.05f, 0.0001f, "synth routes plate reverb predelay");

    synth_uninit(&s);
}

static void test_effect_chain_processes_plate_after_delay(void)
{
    synth_effect_chain chain;
    float energy = 0.0f;

    synth_effect_chain_init(&chain, 1000.0f);
    synth_delay_set_mix(&chain.delay, 0.0f);
    synth_plate_reverb_set_mix(&chain.plate_reverb, 1.0f);
    synth_plate_reverb_set_decay(&chain.plate_reverb, 0.5f);

    (void)synth_effect_chain_process(&chain, (synth_stereo_sample){1.0f, 1.0f});
    for (size_t i = 0; i < 1000; ++i) {
        const synth_stereo_sample output =
            synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});

        energy += sample_level(output);
    }

    expect_true(energy > 0.001f, "effect chain includes plate reverb output");

    synth_effect_chain_uninit(&chain);
}

int main(void)
{
    test_plate_reverb_defaults_to_dry();
    test_plate_reverb_parameters_are_bounded();
    test_plate_reverb_outputs_stereo_tail_after_impulse();
    test_plate_reverb_tail_decays();
    test_plate_reverb_stays_stable_with_silence();
    test_plate_reverb_sample_rate_resizes_delay_lines();
    test_synth_plate_reverb_accessors_route_to_effect();
    test_effect_chain_processes_plate_after_delay();
    printf("All plate reverb tests passed.\n");
    return failures == 0 ? 0 : 1;
}
