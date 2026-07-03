#include "midi/midi_mapping.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void expect_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "FAIL: %s (expected %.6f, got %.6f)\n", message, expected, actual);
        exit(1);
    }
}

static void test_loads_akai_mapping(void)
{
    midi_mapping mapping;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads");
    expect_true(mapping.binding_count == 5, "akai mapping has five bindings");
}

static void test_applies_adsr_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const unsigned char attack_max[] = {0xB0, 1, 127};
    const unsigned char sustain_mid[] = {0xB0, 3, 64};
    const unsigned char wrong_channel[] = {0xB1, 1, 0};

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for apply test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    expect_true(
        midi_mapping_apply_short_message(&mapping, attack_max, sizeof(attack_max), &s, &result),
        "attack cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_ATTACK, "attack cc reports attack parameter");
    expect_near(s.envelope.attack_seconds, 2.0f, 0.0001f, "attack cc scales to max attack");

    expect_true(
        midi_mapping_apply_short_message(&mapping, sustain_mid, sizeof(sustain_mid), &s, &result),
        "sustain cc applies");
    expect_near(s.envelope.sustain_level, 64.0f / 127.0f, 0.0001f, "sustain cc scales to normalized sustain");

    expect_true(
        !midi_mapping_apply_short_message(&mapping, wrong_channel, sizeof(wrong_channel), &s, 0),
        "wrong channel does not apply");
}

static void test_applies_master_gain_cc_value(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const unsigned char master_gain_mid[] = {0xB0, 8, 64};

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for master gain test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    expect_true(
        midi_mapping_apply_short_message(&mapping, master_gain_mid, sizeof(master_gain_mid), &s, &result),
        "master gain cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_MASTER_GAIN, "master gain cc reports master gain parameter");
    expect_near(s.master_gain, 64.0f / 127.0f, 0.0001f, "master gain cc scales to normalized gain");
}

int main(void)
{
    test_loads_akai_mapping();
    test_applies_adsr_cc_values();
    test_applies_master_gain_cc_value();
    printf("All MIDI mapping tests passed.\n");
    return 0;
}
