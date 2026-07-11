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

static int apply_cc_value(
    midi_mapping *mapping,
    synth *s,
    int control,
    int value,
    midi_mapping_apply_result *result)
{
    const unsigned char data[] = {0xB0, (unsigned char)control, (unsigned char)value};

    return midi_mapping_apply_short_message(mapping, data, sizeof(data), s, result);
}

static void pickup_cc(midi_mapping *mapping, synth *s, int control, int start_value, int end_value)
{
    const int step = start_value <= end_value ? 1 : -1;
    int value = start_value;

    for (;;) {
        if (apply_cc_value(mapping, s, control, value, 0)) {
            return;
        }

        if (value == end_value) {
            break;
        }

        value += step;
    }

    expect_true(0, "cc value reaches pickup point");
}

static void test_loads_akai_mapping(void)
{
    midi_mapping mapping;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads");
    expect_true(mapping.binding_count == 23, "akai mapping has twenty-three bindings");
}

static void test_applies_adsr_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const unsigned char wrong_channel[] = {0xB1, 1, 0};

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for apply test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 1, 0, 127);
    expect_true(
        apply_cc_value(&mapping, &s, 1, 127, &result),
        "attack cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_ATTACK, "attack cc reports attack parameter");
    expect_near(s.envelope.attack_seconds, 2.0f, 0.0001f, "attack cc scales to max attack");

    pickup_cc(&mapping, &s, 3, 127, 0);
    expect_true(
        apply_cc_value(&mapping, &s, 3, 64, &result),
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

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for master gain test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 8, 0, 127);
    expect_true(
        apply_cc_value(&mapping, &s, 8, 64, &result),
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

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for filter test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 5, 0, 127);
    expect_true(
        apply_cc_value(&mapping, &s, 5, 102, &result),
        "filter cutoff low range cc applies");
    expect_true(result.synth_value < 5200.0f, "filter cutoff keeps most of the knob under about 5000 hz");

    expect_true(
        apply_cc_value(&mapping, &s, 5, 127, &result),
        "filter cutoff cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FILTER_CUTOFF, "filter cutoff cc reports filter cutoff parameter");
    expect_near(s.filter.cutoff_hz, 20000.0f, 0.01f, "filter cutoff cc scales to max cutoff");
    expect_near(s.right_filter.cutoff_hz, 20000.0f, 0.01f, "filter cutoff keeps stereo filters in sync");

    pickup_cc(&mapping, &s, 6, 0, 127);
    expect_true(
        apply_cc_value(&mapping, &s, 6, 75, &result),
        "filter poles cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FILTER_POLES, "filter poles cc reports filter poles parameter");
    expect_true(s.filter.pole_count == 5, "filter poles cc steps to five poles");
    expect_true(s.right_filter.pole_count == 5, "filter poles keep stereo filters in sync");
    expect_near(result.synth_value, 5.0f, 0.0001f, "filter poles result reports stepped value");

    expect_true(
        apply_cc_value(&mapping, &s, 6, 0, 0),
        "filter poles min applies");
    expect_true(s.filter.pole_count == 1, "filter poles min steps to one pole");

    expect_true(
        apply_cc_value(&mapping, &s, 6, 127, 0),
        "filter poles max applies");
    expect_true(s.filter.pole_count == 8, "filter poles max steps to eight poles");
}

static void test_applies_oscillator_morph_cc_value(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for oscillator morph test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 7, 0, 127);
    expect_true(
        apply_cc_value(&mapping, &s, 7, 76, &result),
        "oscillator morph cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_OSCILLATOR_MORPH,
        "oscillator morph cc reports oscillator morph parameter");
    expect_near(result.synth_value, 76.0f / 127.0f, 0.0001f, "oscillator morph result reports normalized value");
    expect_near(s.oscillator_morph, 76.0f / 127.0f, 0.0001f, "oscillator morph cc scales to normalized morph");
}

static void test_applies_second_oscillator_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for second oscillator test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 9, 0, 64);
    expect_true(
        apply_cc_value(&mapping, &s, 9, 58, &result),
        "second oscillator octave cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE,
        "second oscillator octave cc reports octave parameter");
    expect_true(s.second_oscillator_octave == 0, "second oscillator octave cc 58 steps to zero");
    expect_near(result.synth_value, 0.0f, 0.0001f, "second oscillator octave result reports stepped value");

    pickup_cc(&mapping, &s, 10, 0, 64);
    expect_true(
        apply_cc_value(&mapping, &s, 10, 123, &result),
        "second oscillator pitch cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH,
        "second oscillator pitch cc reports pitch parameter");
    expect_true(s.second_oscillator_pitch_semitones == 6, "second oscillator pitch cc 123 steps to plus six");
    expect_near(result.synth_value, 6.0f, 0.0001f, "second oscillator pitch result reports stepped value");

    pickup_cc(&mapping, &s, 11, 0, 64);
    expect_true(
        apply_cc_value(&mapping, &s, 11, 76, &result),
        "second oscillator fine tune cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE,
        "second oscillator fine tune cc reports fine tune parameter");
    expect_near(
        s.second_oscillator_fine_tune_cents,
        -50.0f + ((76.0f / 127.0f) * 100.0f),
        0.0001f,
        "second oscillator fine tune cc scales to cents");
    expect_near(
        result.synth_value,
        s.second_oscillator_fine_tune_cents,
        0.0001f,
        "second oscillator fine tune result reports cents");
}

static void test_applies_oscillator_mix_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for oscillator mix test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 13, 127, 119);
    expect_true(
        apply_cc_value(&mapping, &s, 13, 119, &result),
        "first oscillator gain cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN,
        "first oscillator gain cc reports first oscillator gain parameter");
    expect_near(s.first_oscillator_gain, 119.0f / 127.0f, 0.0001f, "first oscillator gain cc scales to normalized gain");

    pickup_cc(&mapping, &s, 14, 127, 59);
    expect_true(
        apply_cc_value(&mapping, &s, 14, 59, &result),
        "second oscillator gain cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN,
        "second oscillator gain cc reports second oscillator gain parameter");
    expect_near(s.second_oscillator_gain, 59.0f / 127.0f, 0.0001f, "second oscillator gain cc scales to normalized gain");

    pickup_cc(&mapping, &s, 15, 127, 117);
    expect_true(
        apply_cc_value(&mapping, &s, 15, 117, &result),
        "second oscillator morph cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH,
        "second oscillator morph cc reports second oscillator morph parameter");
    expect_near(s.second_oscillator_morph, 117.0f / 127.0f, 0.0001f, "second oscillator morph cc scales to normalized morph");

    pickup_cc(&mapping, &s, 16, 0, 32);
    expect_true(
        apply_cc_value(&mapping, &s, 16, 29, &result),
        "second master gain cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_MASTER_GAIN,
        "second master gain cc reports master gain parameter");
    expect_near(s.master_gain, 29.0f / 127.0f, 0.0001f, "second master gain cc scales to normalized gain");
}

static void test_applies_stereo_spread_cc_value(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for stereo spread test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 12, 0, 0);
    expect_true(
        apply_cc_value(&mapping, &s, 12, 41, &result),
        "stereo spread cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_STEREO_SPREAD,
        "stereo spread cc reports stereo spread parameter");
    expect_near(s.stereo_spread, 41.0f / 127.0f, 0.0001f, "stereo spread cc scales to normalized width");
}

static void test_applies_lfo_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    float expected_rate;

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for lfo test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 21, 0, 127);
    expect_true(apply_cc_value(&mapping, &s, 21, 116, &result), "lfo rate cc applies");
    expected_rate = expf(logf(0.05f) + ((116.0f / 127.0f) * (logf(20.0f) - logf(0.05f))));
    expect_true(result.parameter == MIDI_MAPPING_PARAM_LFO_RATE, "lfo rate cc reports rate parameter");
    expect_near(s.lfo.frequency_hz, expected_rate, 0.0001f, "lfo rate cc uses logarithmic hz scaling");

    pickup_cc(&mapping, &s, 22, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 22, 57, &result), "lfo depth cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_LFO_DEPTH, "lfo depth cc reports depth parameter");
    expect_near(s.lfo_depth, 57.0f / 127.0f, 0.0001f, "lfo depth cc scales to normalized depth");

    pickup_cc(&mapping, &s, 20, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 20, 21, &result), "lfo morph amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_MORPH_AMOUNT,
        "lfo morph cc reports morph amount parameter");
    expect_near(s.lfo_morph_amount, 21.0f / 127.0f, 0.0001f, "lfo morph amount cc scales normally");

    pickup_cc(&mapping, &s, 23, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 23, 21, &result), "lfo shape morph cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH,
        "lfo shape cc reports shape morph parameter");
    expect_near(s.lfo.morph, 21.0f / 127.0f, 0.0001f, "lfo shape morph cc scales normally");

    pickup_cc(&mapping, &s, 17, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 17, 0, &result), "first oscillator lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT,
        "first oscillator lfo cc reports its route");
    expect_near(s.lfo_first_oscillator_gain_amount, 0.0f, 0.0001f, "first oscillator lfo amount reaches zero");

    pickup_cc(&mapping, &s, 18, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 18, 74, &result), "second oscillator lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT,
        "second oscillator lfo cc reports its route");
    expect_near(s.lfo_second_oscillator_gain_amount, 74.0f / 127.0f, 0.0001f, "second oscillator lfo amount scales normally");

    pickup_cc(&mapping, &s, 19, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 19, 75, &result), "filter lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT,
        "filter lfo cc reports its route");
    expect_near(s.lfo_filter_amount, 75.0f / 127.0f, 0.0001f, "filter lfo amount scales normally");
}

static void test_waits_for_pickup_before_first_knob_change(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads for pickup test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    expect_true(!apply_cc_value(&mapping, &s, 8, 127, &result), "first far-away gain cc waits for pickup");
    expect_near(s.master_gain, SYNTH_DEFAULT_MASTER_GAIN, 0.0001f, "waiting cc leaves master gain unchanged");

    expect_true(!apply_cc_value(&mapping, &s, 8, 64, &result), "same-side gain cc still waits for pickup");
    expect_near(s.master_gain, SYNTH_DEFAULT_MASTER_GAIN, 0.0001f, "same-side waiting cc leaves master gain unchanged");

    expect_true(apply_cc_value(&mapping, &s, 8, 25, &result), "gain cc applies when it reaches pickup");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_MASTER_GAIN, "pickup reports master gain parameter");

    expect_true(apply_cc_value(&mapping, &s, 8, 64, &result), "picked-up gain cc keeps applying");
    expect_near(s.master_gain, 64.0f / 127.0f, 0.0001f, "picked-up gain cc updates normally");
}

int main(void)
{
    test_loads_akai_mapping();
    test_applies_adsr_cc_values();
    test_applies_master_gain_cc_value();
    test_applies_filter_cc_values();
    test_applies_oscillator_morph_cc_value();
    test_applies_second_oscillator_cc_values();
    test_applies_oscillator_mix_cc_values();
    test_applies_stereo_spread_cc_value();
    test_applies_lfo_cc_values();
    test_waits_for_pickup_before_first_knob_change();
    printf("All MIDI mapping tests passed.\n");
    return 0;
}
