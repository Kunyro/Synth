#include "synth/oscillator.h"

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

static int has_signal(const float *buffer, int count)
{
    for (int i = 0; i < count; ++i) {
        if (fabsf(buffer[i]) > 0.0001f) {
            return 1;
        }
    }

    return 0;
}

static float render_peak(synth_waveform waveform, float frequency, float sample_rate, int count)
{
    synth_oscillator oscillator;
    float peak = 0.0f;

    synth_oscillator_init(&oscillator, waveform, frequency);

    for (int i = 0; i < count; ++i) {
        const float sample = synth_oscillator_render(&oscillator, sample_rate);
        const float magnitude = fabsf(sample);

        if (magnitude > peak) {
            peak = magnitude;
        }
    }

    return peak;
}

static int sample_is_bounded(float sample)
{
    return sample >= -1.05f && sample <= 1.05f;
}

static void expect_bounded_render(
    synth_waveform waveform,
    float frequency,
    float sample_rate,
    int count,
    const char *message)
{
    synth_oscillator oscillator;

    synth_oscillator_init(&oscillator, waveform, frequency);

    for (int i = 0; i < count; ++i) {
        if (!sample_is_bounded(synth_oscillator_render(&oscillator, sample_rate))) {
            expect_true(0, message);
            return;
        }
    }
}

int main(void)
{
    synth_oscillator oscillator;
    float buffer[128];
    float saw_peak;
    float square_peak;
    float morphed_sample;

    synth_oscillator_init(&oscillator, SYNTH_WAVEFORM_SINE, 440.0f);
    for (int i = 0; i < 128; ++i) {
        buffer[i] = synth_oscillator_render(&oscillator, 48000.0f);
    }
    expect_true(has_signal(buffer, 128), "sine oscillator renders a signal");

    saw_peak = render_peak(SYNTH_WAVEFORM_SAW, 440.0f, 48000.0f, 512);
    expect_true(saw_peak > 0.5f, "bandlimited saw renders a strong signal");

    square_peak = render_peak(SYNTH_WAVEFORM_SQUARE, 440.0f, 48000.0f, 512);
    expect_true(square_peak > 0.5f, "bandlimited square renders a strong signal");
    expect_bounded_render(
        SYNTH_WAVEFORM_SQUARE,
        10000.0f,
        48000.0f,
        512,
        "high-frequency square render stays bounded");

    synth_oscillator_init(&oscillator, SYNTH_WAVEFORM_SINE, 440.0f);
    synth_oscillator_set_morph(&oscillator, 0.75f);
    morphed_sample = synth_oscillator_render(&oscillator, 48000.0f);
    expect_true(sample_is_bounded(morphed_sample), "morphed oscillator output stays bounded");

    synth_oscillator_set_morph(&oscillator, -1.0f);
    expect_near(oscillator.morph, 0.0f, 0.0001f, "morph clamps low");

    synth_oscillator_set_morph(&oscillator, 2.0f);
    expect_near(oscillator.morph, 1.0f, 0.0001f, "morph clamps high");

    synth_oscillator_set_morph(&oscillator, 0.25f);
    synth_oscillator_render_with_morph(&oscillator, 48000.0f, 0.75f);
    expect_near(oscillator.morph, 0.25f, 0.0001f, "render override preserves base morph");

    if (failures != 0) {
        fprintf(stderr, "%d oscillator test(s) failed\n", failures);
        return 1;
    }

    printf("All oscillator tests passed.\n");
    return 0;
}
