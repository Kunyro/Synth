#include "synth/lfo.h"

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

int main(void)
{
    synth_lfo lfo;
    float sine_value;
    float saw_value;
    float square_value;

    synth_lfo_init(&lfo, 1.0f);
    expect_near(synth_lfo_advance(&lfo, 4.0f), 0.0f, 0.0001f, "sine lfo starts at zero");
    expect_near(synth_lfo_advance(&lfo, 4.0f), 1.0f, 0.0001f, "sine lfo reaches positive peak");
    expect_near(synth_lfo_advance(&lfo, 4.0f), 0.0f, 0.0001f, "sine lfo crosses zero");
    expect_near(synth_lfo_advance(&lfo, 4.0f), -1.0f, 0.0001f, "sine lfo reaches negative peak");
    expect_near(lfo.phase, 0.0f, 0.0001f, "lfo phase wraps after one cycle");

    synth_lfo_set_frequency(&lfo, -2.0f);
    expect_near(lfo.frequency_hz, 0.0f, 0.0001f, "lfo rate clamps at zero");

    synth_lfo_set_frequency(&lfo, 1.0f);
    synth_lfo_set_morph(&lfo, -1.0f);
    expect_near(lfo.morph, 0.0f, 0.0001f, "lfo shape morph clamps low");
    synth_lfo_reset(&lfo, 0.125f);
    sine_value = synth_lfo_advance(&lfo, 48000.0f);

    synth_lfo_set_morph(&lfo, 0.5f);
    synth_lfo_reset(&lfo, 0.125f);
    saw_value = synth_lfo_advance(&lfo, 48000.0f);

    synth_lfo_set_morph(&lfo, 2.0f);
    expect_near(lfo.morph, 1.0f, 0.0001f, "lfo shape morph clamps high");
    synth_lfo_reset(&lfo, 0.125f);
    square_value = synth_lfo_advance(&lfo, 48000.0f);
    expect_true(fabsf(sine_value - saw_value) > 0.01f, "saw lfo shape differs from sine");
    expect_true(fabsf(saw_value - square_value) > 0.01f, "square lfo shape differs from saw");

    synth_lfo_set_morph(&lfo, 0.0f);
    synth_lfo_reset(&lfo, -0.25f);
    expect_near(lfo.phase, 0.75f, 0.0001f, "negative reset phase wraps into one cycle");
    expect_near(synth_lfo_advance(&lfo, 0.0f), -1.0f, 0.0001f, "lfo renders without a valid sample rate");
    expect_near(lfo.phase, 0.75f, 0.0001f, "invalid sample rate leaves phase unchanged");

    expect_true(lfo.phase >= 0.0f && lfo.phase < 1.0f, "lfo keeps normalized phase");

    if (failures != 0) {
        fprintf(stderr, "%d lfo test(s) failed\n", failures);
        return 1;
    }

    printf("All LFO tests passed.\n");
    return 0;
}
