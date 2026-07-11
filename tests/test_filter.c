#include "synth/filter.h"

#include <math.h>
#include <stdio.h>

#define TEST_PI 3.14159265358979323846f

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

static float measure_filtered_sine_peak(int pole_count)
{
    synth_filter filter;
    const float sample_rate = 48000.0f;
    const float frequency = 2000.0f;
    float peak = 0.0f;

    synth_filter_init(&filter, sample_rate, 100.0f);
    synth_filter_set_poles(&filter, pole_count);

    for (int frame = 0; frame < 4096; ++frame) {
        const float phase = (float)frame * frequency / sample_rate;
        const float input = sinf(2.0f * TEST_PI * phase);
        const float output = synth_filter_process(&filter, input);

        if (frame > 1024 && fabsf(output) > peak) {
            peak = fabsf(output);
        }
    }

    return peak;
}

int main(void)
{
    synth_filter filter;
    synth_filter modulated_filter;
    synth_filter reference_filter;
    float one_pole_peak;
    float four_pole_peak;

    synth_filter_init(&filter, 48000.0f, 1000.0f);
    expect_true(filter.pole_count == SYNTH_FILTER_DEFAULT_POLES, "filter defaults to one pole");
    expect_near(filter.sample_rate, 48000.0f, 0.0001f, "filter stores sample rate");
    expect_near(filter.cutoff_hz, 1000.0f, 0.0001f, "filter stores cutoff");
    expect_true(filter.coefficient > 0.0f && filter.coefficient < 1.0f, "filter caches coefficient");

    synth_filter_set_sample_rate(&filter, 96000.0f);
    expect_near(filter.sample_rate, 96000.0f, 0.0001f, "filter sample rate updates");
    expect_true(filter.coefficient > 0.0f && filter.coefficient < 0.1f, "filter coefficient updates with sample rate");

    synth_filter_set_cutoff(&filter, 100000.0f);
    expect_near(filter.cutoff_hz, 48000.0f, 0.0001f, "filter cutoff clamps to nyquist");
    expect_near(filter.coefficient, 1.0f, 0.0001f, "filter bypass coefficient at nyquist");

    synth_filter_set_poles(&filter, 0);
    expect_true(filter.pole_count == 1, "filter clamps pole count low");

    synth_filter_set_poles(&filter, SYNTH_FILTER_MAX_POLES + 1);
    expect_true(filter.pole_count == SYNTH_FILTER_MAX_POLES, "filter clamps pole count high");

    synth_filter_init(&modulated_filter, 48000.0f, 1000.0f);
    synth_filter_init(&reference_filter, 48000.0f, 100.0f);
    expect_near(
        synth_filter_process_with_cutoff(&modulated_filter, 1.0f, 100.0f),
        synth_filter_process(&reference_filter, 1.0f),
        0.0001f,
        "temporary cutoff uses the requested coefficient");
    expect_near(
        modulated_filter.cutoff_hz,
        1000.0f,
        0.0001f,
        "temporary cutoff preserves the stored base cutoff");

    one_pole_peak = measure_filtered_sine_peak(1);
    four_pole_peak = measure_filtered_sine_peak(4);
    expect_true(four_pole_peak < one_pole_peak * 0.1f, "more poles attenuate high frequencies more steeply");

    if (failures != 0) {
        fprintf(stderr, "%d filter test(s) failed\n", failures);
        return 1;
    }

    printf("All filter tests passed.\n");
    return 0;
}
