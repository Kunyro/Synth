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

int main(void)
{
    synth_oscillator oscillator;
    float buffer[128];

    synth_oscillator_init(&oscillator, SYNTH_WAVEFORM_SINE, 440.0f);
    for (int i = 0; i < 128; ++i) {
        buffer[i] = synth_oscillator_render(&oscillator, 48000.0f);
    }
    expect_true(has_signal(buffer, 128), "sine oscillator renders a signal");

    synth_oscillator_init(&oscillator, SYNTH_WAVEFORM_SAW, 440.0f);
    expect_near(synth_oscillator_render(&oscillator, 48000.0f), -1.0f, 0.0001f, "saw starts at -1");

    synth_oscillator_init(&oscillator, SYNTH_WAVEFORM_SQUARE, 440.0f);
    expect_near(synth_oscillator_render(&oscillator, 48000.0f), 1.0f, 0.0001f, "square starts high");

    if (failures != 0) {
        fprintf(stderr, "%d oscillator test(s) failed\n", failures);
        return 1;
    }

    printf("All oscillator tests passed.\n");
    return 0;
}
