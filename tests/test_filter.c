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

static float measure_filtered_sine_peak(int pole_count)
{
    synth_filter filter;
    const float sample_rate = 48000.0f;
    const float frequency = 2000.0f;
    float peak = 0.0f;

    synth_filter_init(&filter, 100.0f);
    synth_filter_set_poles(&filter, pole_count);

    for (int frame = 0; frame < 4096; ++frame) {
        const float phase = (float)frame * frequency / sample_rate;
        const float input = sinf(2.0f * TEST_PI * phase);
        const float output = synth_filter_process(&filter, input, sample_rate);

        if (frame > 1024 && fabsf(output) > peak) {
            peak = fabsf(output);
        }
    }

    return peak;
}

int main(void)
{
    synth_filter filter;
    float one_pole_peak;
    float four_pole_peak;

    synth_filter_init(&filter, 1000.0f);
    expect_true(filter.pole_count == SYNTH_FILTER_DEFAULT_POLES, "filter defaults to one pole");

    synth_filter_set_poles(&filter, 0);
    expect_true(filter.pole_count == 1, "filter clamps pole count low");

    synth_filter_set_poles(&filter, SYNTH_FILTER_MAX_POLES + 1);
    expect_true(filter.pole_count == SYNTH_FILTER_MAX_POLES, "filter clamps pole count high");

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
