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
    expect_true(mapping.binding_count == 8, "akai mapping has eight bindings");
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

static void test_applies_filter_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const unsigned char filter_cutoff_low_range[] = {0xB0, 5, 102};
    const unsigned char filter_cutoff_max[] = {0xB0, 5, 127};
    const unsigned char filter_poles_mid[] = {0xB0, 6, 75};
    const unsigned char filter_poles_min[] = {0xB0, 6, 0};
    const unsigned char filter_poles_max[] = {0xB0, 6, 127};

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for filter test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    expect_true(
        midi_mapping_apply_short_message(&mapping, filter_cutoff_low_range, sizeof(filter_cutoff_low_range), &s, &result),
        "filter cutoff low range cc applies");
    expect_true(result.synth_value < 5200.0f, "filter cutoff keeps most of the knob under about 5000 hz");

    expect_true(
        midi_mapping_apply_short_message(&mapping, filter_cutoff_max, sizeof(filter_cutoff_max), &s, &result),
        "filter cutoff cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FILTER_CUTOFF, "filter cutoff cc reports filter cutoff parameter");
    expect_near(s.filter.cutoff_hz, 20000.0f, 0.01f, "filter cutoff cc scales to max cutoff");

    expect_true(
        midi_mapping_apply_short_message(&mapping, filter_poles_mid, sizeof(filter_poles_mid), &s, &result),
        "filter poles cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FILTER_POLES, "filter poles cc reports filter poles parameter");
    expect_true(s.filter.pole_count == 5, "filter poles cc steps to five poles");
    expect_near(result.synth_value, 5.0f, 0.0001f, "filter poles result reports stepped value");

    expect_true(
        midi_mapping_apply_short_message(&mapping, filter_poles_min, sizeof(filter_poles_min), &s, 0),
        "filter poles min applies");
    expect_true(s.filter.pole_count == 1, "filter poles min steps to one pole");

    expect_true(
        midi_mapping_apply_short_message(&mapping, filter_poles_max, sizeof(filter_poles_max), &s, 0),
        "filter poles max applies");
    expect_true(s.filter.pole_count == 8, "filter poles max steps to eight poles");
}

static void test_applies_oscillator_morph_cc_value(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const unsigned char oscillator_morph_value[] = {0xB0, 7, 76};

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for oscillator morph test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    expect_true(
        midi_mapping_apply_short_message(&mapping, oscillator_morph_value, sizeof(oscillator_morph_value), &s, &result),
        "oscillator morph cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_OSCILLATOR_MORPH,
        "oscillator morph cc reports oscillator morph parameter");
    expect_near(result.synth_value, 76.0f / 127.0f, 0.0001f, "oscillator morph result reports normalized value");
    expect_near(s.oscillator_morph, 76.0f / 127.0f, 0.0001f, "oscillator morph cc scales to normalized morph");
}

int main(void)
{
    test_loads_akai_mapping();
    test_applies_adsr_cc_values();
    test_applies_master_gain_cc_value();
    test_applies_filter_cc_values();
    test_applies_oscillator_morph_cc_value();
    printf("All MIDI mapping tests passed.\n");
    return 0;
}
