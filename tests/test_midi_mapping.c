#include "midi/midi_mapping.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const midi_mapping_binding *matched_binding = 0;

    for (;;) {
        if (apply_cc_value(mapping, s, control, value, 0)) {
            return;
        }

        if (value == end_value) {
            break;
        }

        value += step;
    }

    for (size_t i = 0; i < mapping->binding_count; ++i) {
        if (mapping->bindings[i].control == control) {
            matched_binding = &mapping->bindings[i];
            break;
        }
    }

    if (matched_binding != 0) {
        fprintf(
            stderr,
            "FAIL: cc %d sweep %d..%d reaches pickup point for %s range %.6f..%.6f\n",
            control,
            start_value,
            end_value,
            midi_mapping_parameter_name(matched_binding->parameter),
            matched_binding->min_value,
            matched_binding->max_value);
    } else {
        fprintf(
            stderr,
            "FAIL: cc %d sweep %d..%d reaches pickup point; no direct binding found\n",
            control,
            start_value,
            end_value);
    }
    exit(1);
}

static void test_loads_akai_mapping(void)
{
    midi_mapping mapping;
    midi_chord_mode mode;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const unsigned char major_pad[] = {0xB0, 35, 127};
    const unsigned char major_pad_wrong_channel[] = {0xB1, 35, 127};
    size_t chord_binding_count = 0;

    expect_true(
        midi_mapping_load(&mapping, "config/midi/akai_mpk_mini_mk2.conf", error, sizeof(error)),
        "akai mapping loads");
    expect_true(mapping.binding_count == 24, "akai mapping has twenty-four direct parameter bindings");
    expect_true(mapping.effect_banks[0].selector.enabled, "akai mapping has bank one effect selector");
    expect_true(mapping.effect_banks[0].macros[0].enabled, "akai mapping has bank one effect macro one");
    expect_true(mapping.effect_banks[0].macros[1].enabled, "akai mapping has bank one effect macro two");
    expect_true(mapping.effect_banks[0].macros[2].enabled, "akai mapping has bank one effect macro three");
    expect_true(mapping.effect_banks[1].selector.enabled, "akai mapping has bank two effect selector");
    expect_true(mapping.effect_banks[1].macros[0].enabled, "akai mapping has bank two effect macro one");
    expect_true(mapping.effect_banks[1].macros[1].enabled, "akai mapping has bank two effect macro two");
    expect_true(mapping.effect_banks[1].macros[2].enabled, "akai mapping has bank two effect macro three");

    for (size_t i = 0; i < MIDI_CHORD_MODE_PAD_COUNT; ++i) {
        if (mapping.chord_bindings[i].enabled) {
            chord_binding_count += 1;
        }
    }

    expect_true(chord_binding_count == MIDI_CHORD_MODE_PAD_COUNT, "akai mapping has eight chord pad bindings");
    expect_true(mapping.chord_bindings[MIDI_CHORD_MODE_PAD_MAJOR].channel == 1, "major chord pad uses channel one");
    expect_true(mapping.chord_bindings[MIDI_CHORD_MODE_PAD_MAJOR].control == 35, "major chord pad uses cc thirty-five");

    midi_chord_mode_init(&mode);
    midi_mapping_configure_chord_mode(&mapping, &mode);
    expect_true(
        midi_chord_mode_handle_short_message(&mode, major_pad, sizeof(major_pad), 0, 0),
        "configured major chord pad is consumed");
    expect_true(
        !midi_chord_mode_handle_short_message(&mode, major_pad_wrong_channel, sizeof(major_pad_wrong_channel), 0, 0),
        "configured major chord pad requires the configured channel");
}

static void test_parameter_metadata(void)
{
    const midi_mapping_parameter_info *cutoff_info;
    const midi_mapping_parameter_info *bits_info;
    const midi_mapping_parameter_info *saturation_drive_info;
    midi_mapping_scale scale;

    expect_true(midi_mapping_parameter_count() == 54, "metadata lists every mappable parameter");

    cutoff_info = midi_mapping_parameter_info_by_name("filter_cutoff");
    expect_true(cutoff_info != 0, "filter cutoff metadata is findable");
    expect_true(cutoff_info->parameter == MIDI_MAPPING_PARAM_FILTER_CUTOFF, "filter cutoff metadata names parameter");
    expect_true(cutoff_info->default_scale == MIDI_MAPPING_SCALE_LOG, "filter cutoff defaults to log scale");
    expect_near(cutoff_info->default_min_value, 20.0f, 0.0001f, "filter cutoff metadata min");
    expect_near(cutoff_info->default_max_value, 20000.0f, 0.0001f, "filter cutoff metadata max");

    saturation_drive_info = midi_mapping_parameter_info_by_name("saturation_drive");
    expect_true(saturation_drive_info != 0, "saturation drive metadata is findable");
    expect_true(
        saturation_drive_info->parameter == MIDI_MAPPING_PARAM_SATURATION_DRIVE,
        "saturation drive metadata names parameter");
    expect_true(
        saturation_drive_info->default_scale == MIDI_MAPPING_SCALE_LINEAR,
        "saturation drive defaults to linear scale");
    expect_near(
        saturation_drive_info->default_min_value,
        SYNTH_SATURATION_MIN_DRIVE,
        0.0001f,
        "saturation drive metadata min");
    expect_near(
        saturation_drive_info->default_max_value,
        SYNTH_SATURATION_MAX_DRIVE,
        0.0001f,
        "saturation drive metadata max");

    bits_info = midi_mapping_parameter_info_at(28);
    expect_true(bits_info != 0, "bitcrusher bits metadata is findable by index");
    expect_true(
        strcmp(bits_info->name, "bitcrusher_bits") == 0,
        "bitcrusher bits metadata has config name");
    expect_true(bits_info->default_scale == MIDI_MAPPING_SCALE_STEP, "bitcrusher bits defaults to step scale");

    expect_true(strcmp(midi_mapping_scale_name(MIDI_MAPPING_SCALE_LINEAR), "linear") == 0, "linear scale has name");
    expect_true(midi_mapping_parse_scale_name("log", &scale), "log scale parses");
    expect_true(scale == MIDI_MAPPING_SCALE_LOG, "log scale parse result");
    expect_true(!midi_mapping_parse_scale_name("curve", &scale), "unknown scale does not parse");
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
    expect_near(synth_get_adsr(&s).attack_seconds, 2.0f, 0.0001f, "attack cc scales to max attack");

    pickup_cc(&mapping, &s, 3, 127, 0);
    expect_true(
        apply_cc_value(&mapping, &s, 3, 64, &result),
        "sustain cc applies");
    expect_near(synth_get_adsr(&s).sustain_level, 64.0f / 127.0f, 0.0001f, "sustain cc scales to normalized sustain");

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
    expect_near(synth_get_master_gain(&s), 64.0f / 127.0f, 0.0001f, "master gain cc scales to normalized gain");
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
    expect_near(synth_get_filter_cutoff(&s), 20000.0f, 0.01f, "filter cutoff cc scales to max cutoff");
    expect_near(s.right_filter.cutoff_hz, 20000.0f, 0.01f, "filter cutoff keeps stereo filters in sync");

    pickup_cc(&mapping, &s, 6, 0, 127);
    expect_true(
        apply_cc_value(&mapping, &s, 6, 75, &result),
        "filter poles cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FILTER_POLES, "filter poles cc reports filter poles parameter");
    expect_true(synth_get_filter_poles(&s) == 5, "filter poles cc steps to five poles");
    expect_true(s.right_filter.pole_count == 5, "filter poles keep stereo filters in sync");
    expect_near(result.synth_value, 5.0f, 0.0001f, "filter poles result reports stepped value");

    expect_true(
        apply_cc_value(&mapping, &s, 6, 0, 0),
        "filter poles min applies");
    expect_true(synth_get_filter_poles(&s) == 1, "filter poles min steps to one pole");

    expect_true(
        apply_cc_value(&mapping, &s, 6, 127, 0),
        "filter poles max applies");
    expect_true(synth_get_filter_poles(&s) == 8, "filter poles max steps to eight poles");
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
    expect_near(synth_get_oscillator_morph(&s), 76.0f / 127.0f, 0.0001f, "oscillator morph cc scales to normalized morph");
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
    expect_true(synth_get_second_oscillator_octave(&s) == 0, "second oscillator octave cc 58 steps to zero");
    expect_near(result.synth_value, 0.0f, 0.0001f, "second oscillator octave result reports stepped value");

    pickup_cc(&mapping, &s, 10, 0, 64);
    expect_true(
        apply_cc_value(&mapping, &s, 10, 123, &result),
        "second oscillator pitch cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH,
        "second oscillator pitch cc reports pitch parameter");
    expect_true(synth_get_second_oscillator_pitch(&s) == 6, "second oscillator pitch cc 123 steps to plus six");
    expect_near(result.synth_value, 6.0f, 0.0001f, "second oscillator pitch result reports stepped value");

    pickup_cc(&mapping, &s, 11, 0, 64);
    expect_true(
        apply_cc_value(&mapping, &s, 11, 76, &result),
        "second oscillator fine tune cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE,
        "second oscillator fine tune cc reports fine tune parameter");
    expect_near(
        synth_get_second_oscillator_fine_tune(&s),
        -50.0f + ((76.0f / 127.0f) * 100.0f),
        0.0001f,
        "second oscillator fine tune cc scales to cents");
    expect_near(
        result.synth_value,
        synth_get_second_oscillator_fine_tune(&s),
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
    expect_near(synth_get_first_oscillator_gain(&s), 119.0f / 127.0f, 0.0001f, "first oscillator gain cc scales to normalized gain");

    pickup_cc(&mapping, &s, 14, 0, 59);
    expect_true(
        apply_cc_value(&mapping, &s, 14, 59, &result),
        "second oscillator gain cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN,
        "second oscillator gain cc reports second oscillator gain parameter");
    expect_near(synth_get_second_oscillator_gain(&s), 59.0f / 127.0f, 0.0001f, "second oscillator gain cc scales to normalized gain");

    pickup_cc(&mapping, &s, 15, 127, 117);
    expect_true(
        apply_cc_value(&mapping, &s, 15, 117, &result),
        "second oscillator morph cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH,
        "second oscillator morph cc reports second oscillator morph parameter");
    expect_near(synth_get_second_oscillator_morph(&s), 117.0f / 127.0f, 0.0001f, "second oscillator morph cc scales to normalized morph");

    pickup_cc(&mapping, &s, 16, 0, 32);
    expect_true(
        apply_cc_value(&mapping, &s, 16, 29, &result),
        "second master gain cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_MASTER_GAIN,
        "second master gain cc reports master gain parameter");
    expect_near(synth_get_master_gain(&s), 29.0f / 127.0f, 0.0001f, "second master gain cc scales to normalized gain");
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
    expect_near(synth_get_stereo_spread(&s), 41.0f / 127.0f, 0.0001f, "stereo spread cc scales to normalized width");
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
    expect_near(synth_get_lfo_rate(&s), expected_rate, 0.0001f, "lfo rate cc uses logarithmic hz scaling");

    pickup_cc(&mapping, &s, 22, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 22, 57, &result), "lfo depth cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_LFO_DEPTH, "lfo depth cc reports depth parameter");
    expect_near(synth_get_lfo_depth(&s), 57.0f / 127.0f, 0.0001f, "lfo depth cc scales to normalized depth");

    pickup_cc(&mapping, &s, 19, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 19, 75, &result), "first oscillator morph lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_MORPH_AMOUNT,
        "first oscillator morph lfo cc reports its route");
    expect_near(
        synth_get_lfo_first_oscillator_morph_amount(&s),
        75.0f / 127.0f,
        0.0001f,
        "first oscillator morph lfo amount scales normally");

    pickup_cc(&mapping, &s, 20, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 20, 21, &result), "second oscillator morph lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_MORPH_AMOUNT,
        "second oscillator morph lfo cc reports its route");
    expect_near(
        synth_get_lfo_second_oscillator_morph_amount(&s),
        21.0f / 127.0f,
        0.0001f,
        "second oscillator morph lfo amount scales normally");

    pickup_cc(&mapping, &s, 24, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 24, 21, &result), "lfo shape morph cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH,
        "lfo shape cc reports shape morph parameter");
    expect_near(synth_get_lfo_shape_morph(&s), 21.0f / 127.0f, 0.0001f, "lfo shape morph cc scales normally");

    pickup_cc(&mapping, &s, 17, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 17, 0, &result), "first oscillator lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT,
        "first oscillator lfo cc reports its route");
    expect_near(synth_get_lfo_first_oscillator_gain_amount(&s), 0.0f, 0.0001f, "first oscillator lfo amount reaches zero");

    pickup_cc(&mapping, &s, 18, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 18, 74, &result), "second oscillator lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT,
        "second oscillator lfo cc reports its route");
    expect_near(synth_get_lfo_second_oscillator_gain_amount(&s), 74.0f / 127.0f, 0.0001f, "second oscillator lfo amount scales normally");

    pickup_cc(&mapping, &s, 23, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 23, 75, &result), "filter lfo amount cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT,
        "filter lfo cc reports its route");
    expect_near(synth_get_lfo_filter_amount(&s), 75.0f / 127.0f, 0.0001f, "filter lfo amount scales normally");
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
    expect_near(synth_get_master_gain(&s), SYNTH_DEFAULT_MASTER_GAIN, 0.0001f, "waiting cc leaves master gain unchanged");

    expect_true(!apply_cc_value(&mapping, &s, 8, 64, &result), "same-side gain cc still waits for pickup");
    expect_near(synth_get_master_gain(&s), SYNTH_DEFAULT_MASTER_GAIN, 0.0001f, "same-side waiting cc leaves master gain unchanged");

    expect_true(apply_cc_value(&mapping, &s, 8, 25, &result), "gain cc applies when it reaches pickup");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_MASTER_GAIN, "pickup reports master gain parameter");

    expect_true(apply_cc_value(&mapping, &s, 8, 64, &result), "picked-up gain cc keeps applying");
    expect_near(synth_get_master_gain(&s), 64.0f / 127.0f, 0.0001f, "picked-up gain cc updates normally");
}

static void test_names_distortion_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_DISTORTION_DRIVE), "distortion_drive") == 0,
        "distortion drive has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_DISTORTION_MIX), "distortion_mix") == 0,
        "distortion mix has a midi mapping name");
}

static void test_names_saturation_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_SATURATION_DRIVE), "saturation_drive") == 0,
        "saturation drive has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_SATURATION_MIX), "saturation_mix") == 0,
        "saturation mix has a midi mapping name");
}

static void test_names_delay_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_DELAY_TIME), "delay_time") == 0,
        "delay time has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_DELAY_FEEDBACK), "delay_feedback") == 0,
        "delay feedback has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_DELAY_MIX), "delay_mix") == 0,
        "delay mix has a midi mapping name");
}

static void test_names_plate_reverb_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_PLATE_REVERB_DECAY), "plate_reverb_decay") == 0,
        "plate reverb decay has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_PLATE_REVERB_DAMPING), "plate_reverb_damping") == 0,
        "plate reverb damping has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_PLATE_REVERB_MIX), "plate_reverb_mix") == 0,
        "plate reverb mix has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_PLATE_REVERB_PREDELAY), "plate_reverb_predelay") == 0,
        "plate reverb predelay has a midi mapping name");
}

static void test_names_bitcrusher_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_BITCRUSHER_SAMPLE_RATE), "bitcrusher_sample_rate") == 0,
        "bitcrusher sample rate has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_BITCRUSHER_BITS), "bitcrusher_bits") == 0,
        "bitcrusher bits has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_BITCRUSHER_MIX), "bitcrusher_mix") == 0,
        "bitcrusher mix has a midi mapping name");
}

static void test_names_flanger_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_FLANGER_RATE), "flanger_rate") == 0,
        "flanger rate has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_FLANGER_INTENSITY), "flanger_intensity") == 0,
        "flanger intensity has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_FLANGER_DEPTH), "flanger_depth") == 0,
        "flanger depth has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_FLANGER_FEEDBACK), "flanger_feedback") == 0,
        "flanger feedback has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_FLANGER_MIX), "flanger_mix") == 0,
        "flanger mix has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_FLANGER_MANUAL), "flanger_manual") == 0,
        "flanger manual delay has a midi mapping name");
}

static void test_names_ring_mod_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_RING_MOD_FREQUENCY), "ring_mod_frequency") == 0,
        "ring mod frequency has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_RING_MOD_RECTIFY), "ring_mod_rectify") == 0,
        "ring mod rectify has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_RING_MOD_MIX), "ring_mod_mix") == 0,
        "ring mod mix has a midi mapping name");
}

static void test_names_eq_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_EQ_LOW), "eq_low") == 0,
        "eq low has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_EQ_MID), "eq_mid") == 0,
        "eq mid has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_EQ_HIGH), "eq_high") == 0,
        "eq high has a midi mapping name");
}

static void test_names_compressor_parameters(void)
{
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_COMPRESSOR_THRESHOLD), "compressor_threshold") == 0,
        "compressor threshold has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_COMPRESSOR_RATIO), "compressor_ratio") == 0,
        "compressor ratio has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_COMPRESSOR_MAKEUP_GAIN), "compressor_makeup_gain") == 0,
        "compressor makeup gain has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_COMPRESSOR_ATTACK_SECONDS), "compressor_attack_seconds") == 0,
        "compressor attack has a midi mapping name");
    expect_true(
        strcmp(midi_mapping_parameter_name(MIDI_MAPPING_PARAM_COMPRESSOR_RELEASE_SECONDS), "compressor_release_seconds") == 0,
        "compressor release has a midi mapping name");
}

static void test_applies_distortion_mix_cc_value(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for distortion mix test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 25, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 25, 44, &result), "distortion mix cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_DISTORTION_MIX,
        "distortion mix cc reports distortion mix parameter");
    expect_near(
        synth_get_distortion_mix(&s),
        44.0f / 127.0f,
        0.0001f,
        "distortion mix cc scales to normalized dry wet");
    expect_near(result.synth_value, 44.0f / 127.0f, 0.0001f, "distortion mix result reports normalized value");
}

static void test_applies_distortion_drive_cc_value(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_drive =
        SYNTH_DISTORTION_MIN_DRIVE +
        ((96.0f / 127.0f) * (SYNTH_DISTORTION_MAX_DRIVE - SYNTH_DISTORTION_MIN_DRIVE));

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for distortion drive test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 26, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 26, 96, &result), "distortion drive cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_DISTORTION_DRIVE,
        "distortion drive cc reports distortion drive parameter");
    expect_near(
        synth_get_distortion_drive(&s),
        expected_drive,
        0.0001f,
        "distortion drive cc scales to the drive range");
    expect_near(result.synth_value, expected_drive, 0.0001f, "distortion drive result reports scaled value");
}

static void test_applies_saturation_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    const float expected_drive =
        SYNTH_SATURATION_MIN_DRIVE +
        ((96.0f / 127.0f) * (SYNTH_SATURATION_MAX_DRIVE - SYNTH_SATURATION_MIN_DRIVE));

    midi_mapping_init(&mapping);
    mapping.binding_count = 2;
    mapping.bindings[0].parameter = MIDI_MAPPING_PARAM_SATURATION_MIX;
    mapping.bindings[0].source_type = MIDI_MAPPING_SOURCE_CC;
    mapping.bindings[0].channel = 1;
    mapping.bindings[0].control = 33;
    mapping.bindings[0].scale = MIDI_MAPPING_SCALE_LINEAR;
    mapping.bindings[0].min_value = 0.0f;
    mapping.bindings[0].max_value = 1.0f;
    mapping.bindings[0].pickup.picked_up = 1;
    mapping.bindings[1].parameter = MIDI_MAPPING_PARAM_SATURATION_DRIVE;
    mapping.bindings[1].source_type = MIDI_MAPPING_SOURCE_CC;
    mapping.bindings[1].channel = 1;
    mapping.bindings[1].control = 34;
    mapping.bindings[1].scale = MIDI_MAPPING_SCALE_LINEAR;
    mapping.bindings[1].min_value = SYNTH_SATURATION_MIN_DRIVE;
    mapping.bindings[1].max_value = SYNTH_SATURATION_MAX_DRIVE;
    mapping.bindings[1].pickup.picked_up = 1;
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    expect_true(apply_cc_value(&mapping, &s, 33, 44, &result), "saturation mix cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SATURATION_MIX,
        "saturation mix cc reports saturation mix parameter");
    expect_near(
        synth_get_saturation_mix(&s),
        44.0f / 127.0f,
        0.0001f,
        "saturation mix cc scales to normalized dry wet");
    expect_near(result.synth_value, 44.0f / 127.0f, 0.0001f, "saturation mix result reports normalized value");

    expect_true(apply_cc_value(&mapping, &s, 34, 96, &result), "saturation drive cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_SATURATION_DRIVE,
        "saturation drive cc reports saturation drive parameter");
    expect_near(
        synth_get_saturation_drive(&s),
        expected_drive,
        0.0001f,
        "saturation drive cc scales to the drive range");
    expect_near(result.synth_value, expected_drive, 0.0001f, "saturation drive result reports scaled value");
}

static void test_applies_bitcrusher_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_rate =
        expf(logf(100.0f) + ((64.0f / 127.0f) * (logf(48000.0f) - logf(100.0f))));

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for bitcrusher test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 27, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 27, 44, &result), "bitcrusher mix cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_BITCRUSHER_MIX,
        "bitcrusher mix cc reports bitcrusher mix parameter");
    expect_near(
        synth_get_bitcrusher_mix(&s),
        44.0f / 127.0f,
        0.0001f,
        "bitcrusher mix cc scales to normalized dry wet");
    expect_near(result.synth_value, 44.0f / 127.0f, 0.0001f, "bitcrusher mix result reports normalized value");

    pickup_cc(&mapping, &s, 28, 0, 127);
    expect_true(apply_cc_value(&mapping, &s, 28, 64, &result), "bitcrusher sample rate cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_BITCRUSHER_SAMPLE_RATE,
        "bitcrusher sample rate cc reports bitcrusher sample rate parameter");
    expect_near(
        synth_get_bitcrusher_sample_rate(&s),
        expected_rate,
        0.01f,
        "bitcrusher sample rate cc uses logarithmic hz scaling");
    expect_near(result.synth_value, expected_rate, 0.01f, "bitcrusher sample rate result reports hz");

    pickup_cc(&mapping, &s, 29, 127, 127);
    expect_true(apply_cc_value(&mapping, &s, 29, 85, &result), "bitcrusher bits cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_BITCRUSHER_BITS,
        "bitcrusher bits cc reports bitcrusher bits parameter");
    expect_true(synth_get_bitcrusher_bits(&s) == 11, "bitcrusher bits cc steps to eleven bits");
    expect_near(result.synth_value, 11.0f, 0.0001f, "bitcrusher bits result reports stepped value");
}

static void test_applies_flanger_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_rate =
        expf(
            logf(SYNTH_FLANGER_MIN_RATE_HZ) +
            ((64.0f / 127.0f) *
                (logf(SYNTH_FLANGER_MAX_RATE_HZ) - logf(SYNTH_FLANGER_MIN_RATE_HZ))));
    const float expected_feedback =
        -SYNTH_FLANGER_MAX_FEEDBACK +
        ((96.0f / 127.0f) * (SYNTH_FLANGER_MAX_FEEDBACK * 2.0f));
    const float expected_manual =
        SYNTH_FLANGER_MIN_MANUAL_SECONDS +
        ((44.0f / 127.0f) *
            (SYNTH_FLANGER_MAX_MANUAL_SECONDS - SYNTH_FLANGER_MIN_MANUAL_SECONDS));
    const float expected_intensity_feedback =
        SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK +
        (sqrtf(96.0f / 127.0f) *
            (SYNTH_FLANGER_MAX_FEEDBACK - SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK));

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for flanger test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 45, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 45, 64, &result), "flanger rate cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_FLANGER_RATE,
        "flanger rate cc reports flanger rate parameter");
    expect_near(
        synth_get_flanger_rate(&s),
        expected_rate,
        0.0001f,
        "flanger rate cc uses logarithmic hz scaling");
    expect_near(result.synth_value, expected_rate, 0.0001f, "flanger rate result reports hz");

    pickup_cc(&mapping, &s, 50, 0, 83);
    expect_true(apply_cc_value(&mapping, &s, 50, 96, &result), "flanger intensity cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_FLANGER_INTENSITY,
        "flanger intensity cc reports flanger intensity parameter");
    expect_near(
        synth_get_flanger_intensity(&s),
        96.0f / 127.0f,
        0.0001f,
        "flanger intensity scales");
    expect_near(
        synth_get_flanger_depth(&s),
        96.0f / 127.0f,
        0.0001f,
        "flanger intensity controls depth");
    expect_near(
        synth_get_flanger_feedback(&s),
        expected_intensity_feedback,
        0.0001f,
        "flanger intensity controls feedback");

    pickup_cc(&mapping, &s, 46, 0, 96);
    expect_true(apply_cc_value(&mapping, &s, 46, 96, &result), "flanger depth cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_FLANGER_DEPTH,
        "flanger depth cc reports flanger depth parameter");
    expect_near(synth_get_flanger_depth(&s), 96.0f / 127.0f, 0.0001f, "flanger depth scales");

    pickup_cc(&mapping, &s, 47, 127, 86);
    expect_true(apply_cc_value(&mapping, &s, 47, 96, &result), "flanger feedback cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_FLANGER_FEEDBACK,
        "flanger feedback cc reports flanger feedback parameter");
    expect_near(
        synth_get_flanger_feedback(&s),
        expected_feedback,
        0.0001f,
        "flanger feedback scales across negative and positive feedback");

    pickup_cc(&mapping, &s, 48, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 48, 44, &result), "flanger mix cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_FLANGER_MIX,
        "flanger mix cc reports flanger mix parameter");
    expect_near(synth_get_flanger_mix(&s), 44.0f / 127.0f, 0.0001f, "flanger mix scales");

    pickup_cc(&mapping, &s, 49, 0, 29);
    expect_true(apply_cc_value(&mapping, &s, 49, 44, &result), "flanger manual cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_FLANGER_MANUAL,
        "flanger manual cc reports flanger manual parameter");
    expect_near(
        synth_get_flanger_manual(&s),
        expected_manual,
        0.0001f,
        "flanger manual delay scales to seconds");
}

static void test_applies_ring_mod_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_frequency =
        expf(
            logf(SYNTH_RING_MOD_MIN_FREQUENCY_HZ) +
            ((64.0f / 127.0f) *
                (logf(SYNTH_RING_MOD_MAX_FREQUENCY_HZ) -
                    logf(SYNTH_RING_MOD_MIN_FREQUENCY_HZ))));
    const float expected_rectify =
        SYNTH_RING_MOD_MIN_RECTIFY +
        ((96.0f / 127.0f) *
            (SYNTH_RING_MOD_MAX_RECTIFY - SYNTH_RING_MOD_MIN_RECTIFY));

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for ring mod test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 51, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 51, 64, &result), "ring mod frequency cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_RING_MOD_FREQUENCY,
        "ring mod frequency cc reports ring mod frequency parameter");
    expect_near(
        synth_get_ring_mod_frequency(&s),
        expected_frequency,
        0.0001f,
        "ring mod frequency cc uses logarithmic hz scaling");
    expect_near(result.synth_value, expected_frequency, 0.0001f, "ring mod frequency result reports hz");

    pickup_cc(&mapping, &s, 52, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 52, 96, &result), "ring mod rectify cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_RING_MOD_RECTIFY,
        "ring mod rectify cc reports ring mod rectify parameter");
    expect_near(
        synth_get_ring_mod_rectify(&s),
        expected_rectify,
        0.0001f,
        "ring mod rectify cc scales across negative and positive rectification");
    expect_near(result.synth_value, expected_rectify, 0.0001f, "ring mod rectify result reports scaled value");

    pickup_cc(&mapping, &s, 53, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 53, 44, &result), "ring mod mix cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_RING_MOD_MIX,
        "ring mod mix cc reports ring mod mix parameter");
    expect_near(synth_get_ring_mod_mix(&s), 44.0f / 127.0f, 0.0001f, "ring mod mix scales");
    expect_near(result.synth_value, 44.0f / 127.0f, 0.0001f, "ring mod mix result reports normalized value");
}

static void test_applies_eq_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_low =
        SYNTH_EQ_MIN_GAIN_DB +
        ((96.0f / 127.0f) * (SYNTH_EQ_MAX_GAIN_DB - SYNTH_EQ_MIN_GAIN_DB));
    const float expected_mid =
        SYNTH_EQ_MIN_GAIN_DB +
        ((32.0f / 127.0f) * (SYNTH_EQ_MAX_GAIN_DB - SYNTH_EQ_MIN_GAIN_DB));
    const float expected_high =
        SYNTH_EQ_MIN_GAIN_DB +
        ((80.0f / 127.0f) * (SYNTH_EQ_MAX_GAIN_DB - SYNTH_EQ_MIN_GAIN_DB));

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for eq test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 54, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 54, 96, &result), "eq low cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_EQ_LOW, "eq low cc reports eq low parameter");
    expect_near(synth_get_eq_low(&s), expected_low, 0.0001f, "eq low cc scales to decibels");
    expect_near(result.synth_value, expected_low, 0.0001f, "eq low result reports decibels");

    pickup_cc(&mapping, &s, 55, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 55, 32, &result), "eq mid cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_EQ_MID, "eq mid cc reports eq mid parameter");
    expect_near(synth_get_eq_mid(&s), expected_mid, 0.0001f, "eq mid cc scales to decibels");
    expect_near(result.synth_value, expected_mid, 0.0001f, "eq mid result reports decibels");

    pickup_cc(&mapping, &s, 56, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 56, 80, &result), "eq high cc applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_EQ_HIGH, "eq high cc reports eq high parameter");
    expect_near(synth_get_eq_high(&s), expected_high, 0.0001f, "eq high cc scales to decibels");
    expect_near(result.synth_value, expected_high, 0.0001f, "eq high result reports decibels");
}

static void test_applies_compressor_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_threshold =
        SYNTH_COMPRESSOR_MIN_THRESHOLD_DB +
        ((64.0f / 127.0f) *
            (SYNTH_COMPRESSOR_MAX_THRESHOLD_DB - SYNTH_COMPRESSOR_MIN_THRESHOLD_DB));
    const float expected_ratio =
        SYNTH_COMPRESSOR_MIN_RATIO +
        ((96.0f / 127.0f) *
            (SYNTH_COMPRESSOR_MAX_RATIO - SYNTH_COMPRESSOR_MIN_RATIO));
    const float expected_makeup_gain =
        SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB +
        ((44.0f / 127.0f) *
            (SYNTH_COMPRESSOR_MAX_MAKEUP_GAIN_DB - SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB));
    const float expected_attack =
        expf(
            logf(SYNTH_COMPRESSOR_MIN_ATTACK_SECONDS) +
            ((64.0f / 127.0f) *
                (logf(SYNTH_COMPRESSOR_MAX_ATTACK_SECONDS) -
                    logf(SYNTH_COMPRESSOR_MIN_ATTACK_SECONDS))));
    const float expected_release =
        expf(
            logf(SYNTH_COMPRESSOR_MIN_RELEASE_SECONDS) +
            ((96.0f / 127.0f) *
                (logf(SYNTH_COMPRESSOR_MAX_RELEASE_SECONDS) -
                    logf(SYNTH_COMPRESSOR_MIN_RELEASE_SECONDS))));

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for compressor test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 57, 127, 127);
    expect_true(apply_cc_value(&mapping, &s, 57, 64, &result), "compressor threshold cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_THRESHOLD,
        "compressor threshold cc reports compressor threshold parameter");
    expect_near(
        synth_get_compressor_threshold(&s),
        expected_threshold,
        0.0001f,
        "compressor threshold cc scales to decibels");
    expect_near(result.synth_value, expected_threshold, 0.0001f, "compressor threshold result reports decibels");

    expect_near(
        synth_get_compressor_ratio(&s),
        SYNTH_COMPRESSOR_DEFAULT_RATIO,
        0.0001f,
        "compressor ratio remains at default before ratio cc");
    pickup_cc(&mapping, &s, 58, 0, 127);
    expect_true(apply_cc_value(&mapping, &s, 58, 96, &result), "compressor ratio cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_RATIO,
        "compressor ratio cc reports compressor ratio parameter");
    expect_near(
        synth_get_compressor_ratio(&s),
        expected_ratio,
        0.0001f,
        "compressor ratio cc scales to ratio range");
    expect_near(result.synth_value, expected_ratio, 0.0001f, "compressor ratio result reports scaled ratio");

    pickup_cc(&mapping, &s, 59, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 59, 44, &result), "compressor makeup gain cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_MAKEUP_GAIN,
        "compressor makeup gain cc reports compressor makeup gain parameter");
    expect_near(
        synth_get_compressor_makeup_gain(&s),
        expected_makeup_gain,
        0.0001f,
        "compressor makeup gain cc scales to decibels");
    expect_near(result.synth_value, expected_makeup_gain, 0.0001f, "compressor makeup result reports decibels");

    pickup_cc(&mapping, &s, 60, 0, 127);
    expect_true(apply_cc_value(&mapping, &s, 60, 64, &result), "compressor attack cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_ATTACK_SECONDS,
        "compressor attack cc reports compressor attack parameter");
    expect_near(
        synth_get_compressor_attack_seconds(&s),
        expected_attack,
        0.0001f,
        "compressor attack cc uses logarithmic seconds scaling");
    expect_near(result.synth_value, expected_attack, 0.0001f, "compressor attack result reports seconds");

    pickup_cc(&mapping, &s, 61, 0, 127);
    expect_true(apply_cc_value(&mapping, &s, 61, 96, &result), "compressor release cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_RELEASE_SECONDS,
        "compressor release cc reports compressor release parameter");
    expect_near(
        synth_get_compressor_release_seconds(&s),
        expected_release,
        0.0001f,
        "compressor release cc uses logarithmic seconds scaling");
    expect_near(result.synth_value, expected_release, 0.0001f, "compressor release result reports seconds");

    synth_uninit(&s);
}

static void test_applies_delay_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_time = 0.001f + ((64.0f / 127.0f) * (2.0f - 0.001f));
    const float expected_feedback = (96.0f / 127.0f) * 0.95f;

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for delay test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 30, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 30, 44, &result), "delay mix cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_DELAY_MIX,
        "delay mix cc reports delay mix parameter");
    expect_near(synth_get_delay_mix(&s), 44.0f / 127.0f, 0.0001f, "delay mix cc scales to normalized dry wet");
    expect_near(result.synth_value, 44.0f / 127.0f, 0.0001f, "delay mix result reports normalized value");

    pickup_cc(&mapping, &s, 31, 0, 16);
    expect_true(apply_cc_value(&mapping, &s, 31, 64, &result), "delay time cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_DELAY_TIME,
        "delay time cc reports delay time parameter");
    expect_near(synth_get_delay_time(&s), expected_time, 0.0001f, "delay time cc scales to seconds");
    expect_near(result.synth_value, expected_time, 0.0001f, "delay time result reports seconds");

    pickup_cc(&mapping, &s, 32, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 32, 96, &result), "delay feedback cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_DELAY_FEEDBACK,
        "delay feedback cc reports delay feedback parameter");
    expect_near(synth_get_delay_feedback(&s), expected_feedback, 0.0001f, "delay feedback cc scales to bounded feedback");
    expect_near(result.synth_value, expected_feedback, 0.0001f, "delay feedback result reports scaled value");
}

static void test_applies_plate_reverb_cc_values(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_decay =
        SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS +
        ((64.0f / 127.0f) *
            (SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS - SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS));
    const float expected_damping = 96.0f / 127.0f;
    const float expected_predelay =
        (44.0f / 127.0f) * SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS;

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/direct_effect_mapping.conf", error, sizeof(error)),
        "direct effect mapping loads for plate reverb test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    pickup_cc(&mapping, &s, 41, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 41, 44, &result), "plate reverb mix cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_MIX,
        "plate reverb mix cc reports plate reverb mix parameter");
    expect_near(
        synth_get_plate_reverb_mix(&s),
        44.0f / 127.0f,
        0.0001f,
        "plate reverb mix cc scales to normalized dry wet");

    pickup_cc(&mapping, &s, 42, 0, 22);
    expect_true(apply_cc_value(&mapping, &s, 42, 64, &result), "plate reverb decay cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_DECAY,
        "plate reverb decay cc reports plate reverb decay parameter");
    expect_near(
        synth_get_plate_reverb_decay(&s),
        expected_decay,
        0.0001f,
        "plate reverb decay cc scales to seconds");
    expect_near(result.synth_value, expected_decay, 0.0001f, "plate reverb decay result reports seconds");

    pickup_cc(&mapping, &s, 43, 0, 44);
    expect_true(apply_cc_value(&mapping, &s, 43, 96, &result), "plate reverb damping cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_DAMPING,
        "plate reverb damping cc reports plate reverb damping parameter");
    expect_near(
        synth_get_plate_reverb_damping(&s),
        expected_damping,
        0.0001f,
        "plate reverb damping cc scales to normalized damping");

    pickup_cc(&mapping, &s, 44, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 44, 44, &result), "plate reverb predelay cc applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_PREDELAY,
        "plate reverb predelay cc reports plate reverb predelay parameter");
    expect_near(
        synth_get_plate_reverb_predelay(&s),
        expected_predelay,
        0.0001f,
        "plate reverb predelay cc scales to seconds");
}

static void test_loads_effect_macro_mapping(void)
{
    midi_mapping mapping;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    midi_mapping_parameter parameter;
    midi_mapping_effect effect;

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/effect_macro_mapping.conf", error, sizeof(error)),
        "effect macro mapping loads");
    expect_true(strcmp(mapping.name, "Effect Macro Test") == 0, "effect macro mapping keeps its name");
    expect_true(mapping.binding_count == 1, "effect macro mapping keeps direct parameter bindings");
    expect_true(mapping.effect_banks[0].selector.enabled, "bank one effect selector is bound");
    expect_true(mapping.effect_banks[0].selector.channel == 1, "bank one effect selector channel loads");
    expect_true(mapping.effect_banks[0].selector.control == 10, "bank one effect selector control loads");
    expect_true(mapping.effect_banks[0].macros[0].enabled, "bank one effect macro one is bound");
    expect_true(mapping.effect_banks[0].macros[1].enabled, "bank one effect macro two is bound");
    expect_true(mapping.effect_banks[0].macros[2].enabled, "bank one effect macro three is bound");
    expect_true(mapping.effect_banks[0].has_selected_effect, "bank one starts on an active effect");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_SATURATION,
        "bank one starts on saturation");
    expect_true(mapping.effect_banks[1].selector.enabled, "bank two effect selector is bound");
    expect_true(mapping.effect_banks[1].selector.control == 14, "bank two effect selector control loads");
    expect_true(mapping.effect_banks[1].macros[0].enabled, "bank two effect macro one is bound");
    expect_true(mapping.effect_banks[1].macros[1].enabled, "bank two effect macro two is bound");
    expect_true(mapping.effect_banks[1].macros[2].enabled, "bank two effect macro three is bound");
    expect_true(mapping.effect_banks[1].has_selected_effect, "bank two starts on an active effect");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_FLANGER,
        "bank two starts on flanger");

    expect_true(
        strcmp(midi_mapping_effect_name(MIDI_MAPPING_EFFECT_DELAY), "delay") == 0,
        "delay effect page has a name");
    expect_true(
        strcmp(midi_mapping_effect_name(MIDI_MAPPING_EFFECT_PLATE_REVERB), "plate_reverb") == 0,
        "plate reverb effect page has a name");
    expect_true(
        strcmp(midi_mapping_effect_name(MIDI_MAPPING_EFFECT_FLANGER), "flanger") == 0,
        "flanger effect page has a name");
    expect_true(
        strcmp(midi_mapping_effect_name(MIDI_MAPPING_EFFECT_RING_MOD), "ring_mod") == 0,
        "ring mod effect page has a name");
    expect_true(
        strcmp(midi_mapping_effect_name(MIDI_MAPPING_EFFECT_EQ), "eq") == 0,
        "eq effect page has a name");
    expect_true(
        strcmp(midi_mapping_effect_name(MIDI_MAPPING_EFFECT_COMPRESSOR), "compressor") == 0,
        "compressor effect page has a name");
    expect_true(
        strcmp(midi_mapping_effect_selector_name(0), "effect_selector_1") == 0,
        "bank one selector has a config name");
    expect_true(
        strcmp(midi_mapping_effect_macro_name(0, 2), "effect_1_macro_3") == 0,
        "bank one third effect macro control has a config name");
    expect_true(
        strcmp(midi_mapping_effect_selector_name(1), "effect_selector_2") == 0,
        "bank two selector has a config name");
    expect_true(
        strcmp(midi_mapping_effect_macro_name(1, 2), "effect_2_macro_3") == 0,
        "bank two third effect macro control has a config name");
    expect_true(
        midi_mapping_effect_bank_page(0, 0, &effect),
        "bank one first page has an effect");
    expect_true(
        effect == MIDI_MAPPING_EFFECT_SATURATION,
        "bank one first page is saturation");
    expect_true(
        midi_mapping_effect_bank_page(0, 4, &effect),
        "bank one fifth page has an effect");
    expect_true(
        effect == MIDI_MAPPING_EFFECT_PLATE_REVERB,
        "bank one fifth page is plate reverb");
    expect_true(
        midi_mapping_effect_bank_page(1, 0, &effect),
        "bank two first page has an effect");
    expect_true(
        effect == MIDI_MAPPING_EFFECT_FLANGER,
        "bank two first page is flanger");
    expect_true(
        midi_mapping_effect_bank_page(1, 1, &effect),
        "bank two second page has an effect");
    expect_true(
        effect == MIDI_MAPPING_EFFECT_RING_MOD,
        "bank two second page is ring mod");
    expect_true(
        midi_mapping_effect_bank_page(1, 2, &effect),
        "bank two third page has an effect");
    expect_true(
        effect == MIDI_MAPPING_EFFECT_EQ,
        "bank two third page is eq");
    expect_true(
        midi_mapping_effect_bank_page(1, 3, &effect),
        "bank two fourth page has an effect");
    expect_true(
        effect == MIDI_MAPPING_EFFECT_COMPRESSOR,
        "bank two fourth page is compressor");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_SATURATION, 0, &parameter),
        "saturation macro one has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_SATURATION_DRIVE,
        "saturation macro one routes to drive");
    expect_true(
        !midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_SATURATION, 1, &parameter),
        "saturation macro two is unused");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_SATURATION, 2, &parameter),
        "saturation macro three has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_SATURATION_MIX,
        "saturation macro three routes to dry wet");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_PLATE_REVERB, 0, &parameter),
        "plate reverb macro one has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_DECAY,
        "plate reverb macro one routes to decay");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_PLATE_REVERB, 1, &parameter),
        "plate reverb macro two has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_DAMPING,
        "plate reverb macro two routes to damping");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_PLATE_REVERB, 2, &parameter),
        "plate reverb macro three has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_MIX,
        "plate reverb macro three routes to dry wet");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_FLANGER, 0, &parameter),
        "flanger macro one has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_FLANGER_RATE,
        "flanger macro one routes to rate");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_FLANGER, 1, &parameter),
        "flanger macro two has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_FLANGER_INTENSITY,
        "flanger macro two routes to intensity");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_FLANGER, 2, &parameter),
        "flanger macro three has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_FLANGER_MIX,
        "flanger macro three routes to dry wet");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_RING_MOD, 0, &parameter),
        "ring mod macro one has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_RING_MOD_FREQUENCY,
        "ring mod macro one routes to frequency");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_RING_MOD, 1, &parameter),
        "ring mod macro two has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_RING_MOD_RECTIFY,
        "ring mod macro two routes to rectification");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_RING_MOD, 2, &parameter),
        "ring mod macro three has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_RING_MOD_MIX,
        "ring mod macro three routes to dry wet");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_EQ, 0, &parameter),
        "eq macro one has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_EQ_LOW,
        "eq macro one routes to low band");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_EQ, 1, &parameter),
        "eq macro two has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_EQ_MID,
        "eq macro two routes to mid band");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_EQ, 2, &parameter),
        "eq macro three has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_EQ_HIGH,
        "eq macro three routes to high band");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_COMPRESSOR, 0, &parameter),
        "compressor macro one has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_COMPRESSOR_THRESHOLD,
        "compressor macro one routes to threshold");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_COMPRESSOR, 1, &parameter),
        "compressor macro two has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_COMPRESSOR_RATIO,
        "compressor macro two routes to ratio");
    expect_true(
        midi_mapping_effect_macro_parameter(MIDI_MAPPING_EFFECT_COMPRESSOR, 2, &parameter),
        "compressor macro three has a route");
    expect_true(
        parameter == MIDI_MAPPING_PARAM_COMPRESSOR_MAKEUP_GAIN,
        "compressor macro three routes to makeup gain");
}

static void test_rejects_legacy_effect_macro_names(void)
{
    midi_mapping mapping;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        !midi_mapping_load(
            &mapping,
            "tests/fixtures/legacy_effect_macro_mapping.conf",
            error,
            sizeof(error)),
        "legacy effect macro mapping names are rejected");
}

static void test_effect_selectors_use_bank_pages(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/effect_macro_mapping.conf", error, sizeof(error)),
        "effect macro mapping loads for selector test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    expect_true(apply_cc_value(&mapping, &s, 10, 0, &result), "selector cc reports effect selection");
    expect_true(result.kind == MIDI_MAPPING_APPLY_EFFECT_SELECT, "selector result is effect selection");
    expect_true(result.effect_bank_index == 0, "selector result reports bank one");
    expect_true(result.has_effect, "bank one selector reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_SATURATION, "selector result reports saturation");
    expect_true(mapping.effect_banks[0].has_selected_effect, "bank one has selected effect");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_SATURATION,
        "selector value zero chooses saturation");
    expect_true(apply_cc_value(&mapping, &s, 10, 25, &result), "selector upper saturation step reports effect selection");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_SATURATION,
        "selector value twenty-five chooses saturation");
    expect_true(apply_cc_value(&mapping, &s, 10, 26, &result), "selector lower distortion step reports effect selection");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_DISTORTION, "selector result reports distortion");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_DISTORTION,
        "selector value twenty-six chooses distortion");
    expect_true(apply_cc_value(&mapping, &s, 10, 51, &result), "selector upper distortion step reports effect selection");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_DISTORTION,
        "selector value fifty-one chooses distortion");
    expect_true(apply_cc_value(&mapping, &s, 10, 52, &result), "selector lower bitcrusher step reports effect selection");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_BITCRUSHER, "selector result reports bitcrusher");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_BITCRUSHER,
        "selector value fifty-two chooses bitcrusher");
    expect_true(apply_cc_value(&mapping, &s, 10, 76, &result), "selector upper bitcrusher step reports effect selection");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_BITCRUSHER,
        "selector value seventy-six chooses bitcrusher");
    expect_true(apply_cc_value(&mapping, &s, 10, 77, &result), "selector lower delay step reports effect selection");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_DELAY, "selector result reports delay");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_DELAY,
        "selector value seventy-seven chooses delay");
    expect_true(apply_cc_value(&mapping, &s, 10, 102, &result), "selector upper delay step reports effect selection");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_DELAY,
        "selector value one hundred two chooses delay");
    expect_true(apply_cc_value(&mapping, &s, 10, 103, &result), "selector lower plate reverb step reports effect selection");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_PLATE_REVERB, "selector result reports plate reverb");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_PLATE_REVERB,
        "selector value one hundred three chooses plate reverb");
    expect_true(apply_cc_value(&mapping, &s, 10, 127, &result), "selector upper plate reverb step reports effect selection");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_PLATE_REVERB,
        "selector value one twenty-seven chooses plate reverb");

    expect_true(apply_cc_value(&mapping, &s, 14, 0, &result), "bank two selector reports selection");
    expect_true(result.kind == MIDI_MAPPING_APPLY_EFFECT_SELECT, "bank two selector result is effect selection");
    expect_true(result.effect_bank_index == 1, "bank two selector result reports bank two");
    expect_true(result.has_effect, "bank two selector reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_FLANGER, "bank two selector reports flanger");
    expect_true(mapping.effect_banks[1].has_selected_effect, "bank two has selected effect");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_FLANGER,
        "bank two lower selector chooses flanger");
    expect_true(
        mapping.effect_banks[0].selected_effect == MIDI_MAPPING_EFFECT_PLATE_REVERB,
        "bank two selector does not change bank one");
    expect_true(apply_cc_value(&mapping, &s, 14, 25, &result), "bank two upper flanger step reports selection");
    expect_true(result.has_effect, "bank two upper flanger step reports active effect");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_FLANGER,
        "bank two value twenty-five chooses flanger");
    expect_true(apply_cc_value(&mapping, &s, 14, 26, &result), "bank two lower ring mod step reports selection");
    expect_true(result.has_effect, "bank two lower ring mod step reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_RING_MOD, "bank two selector reports ring mod");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_RING_MOD,
        "bank two value twenty-six chooses ring mod");
    expect_true(apply_cc_value(&mapping, &s, 14, 51, &result), "bank two upper ring mod step reports selection");
    expect_true(result.has_effect, "bank two upper ring mod step reports active effect");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_RING_MOD,
        "bank two value fifty-one chooses ring mod");
    expect_true(apply_cc_value(&mapping, &s, 14, 52, &result), "bank two lower eq step reports selection");
    expect_true(result.has_effect, "bank two lower eq step reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_EQ, "bank two selector reports eq");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_EQ,
        "bank two value fifty-two chooses eq");
    expect_true(apply_cc_value(&mapping, &s, 14, 76, &result), "bank two upper eq step reports selection");
    expect_true(result.has_effect, "bank two upper eq step reports active effect");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_EQ,
        "bank two value seventy-six chooses eq");
    expect_true(apply_cc_value(&mapping, &s, 14, 77, &result), "bank two lower compressor step reports selection");
    expect_true(result.has_effect, "bank two lower compressor step reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_COMPRESSOR, "bank two selector reports compressor");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_COMPRESSOR,
        "bank two value seventy-seven chooses compressor");
    expect_true(apply_cc_value(&mapping, &s, 14, 102, &result), "bank two upper compressor step reports selection");
    expect_true(result.has_effect, "bank two upper compressor step reports active effect");
    expect_true(
        mapping.effect_banks[1].selected_effect == MIDI_MAPPING_EFFECT_COMPRESSOR,
        "bank two value one hundred two chooses compressor");
    expect_true(apply_cc_value(&mapping, &s, 14, 103, &result), "bank two lower blank step reports selection");
    expect_true(!result.has_effect, "bank two lower blank step reports blank page");
    expect_true(!mapping.effect_banks[1].has_selected_effect, "bank two blank page clears selected effect");
    expect_true(apply_cc_value(&mapping, &s, 14, 127, &result), "bank two upper selector reports selection");
    expect_true(!result.has_effect, "bank two upper selector reports blank page");
    expect_true(!mapping.effect_banks[1].has_selected_effect, "bank two upper selector remains blank");
}

static void test_effect_macros_apply_selected_effect_parameters(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float expected_time = 0.001f + ((64.0f / 127.0f) * (2.0f - 0.001f));
    const float expected_feedback = (96.0f / 127.0f) * 0.95f;
    const float expected_decay =
        SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS +
        ((64.0f / 127.0f) *
            (SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS - SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS));
    const float expected_flanger_rate =
        expf(
            logf(SYNTH_FLANGER_MIN_RATE_HZ) +
            ((64.0f / 127.0f) *
                (logf(SYNTH_FLANGER_MAX_RATE_HZ) - logf(SYNTH_FLANGER_MIN_RATE_HZ))));
    const float expected_flanger_intensity_feedback =
        SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK +
        (sqrtf(96.0f / 127.0f) *
            (SYNTH_FLANGER_MAX_FEEDBACK - SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK));
    const float expected_ring_mod_frequency =
        expf(
            logf(SYNTH_RING_MOD_MIN_FREQUENCY_HZ) +
            ((64.0f / 127.0f) *
                (logf(SYNTH_RING_MOD_MAX_FREQUENCY_HZ) -
                    logf(SYNTH_RING_MOD_MIN_FREQUENCY_HZ))));
    const float expected_ring_mod_rectify =
        SYNTH_RING_MOD_MIN_RECTIFY +
        ((96.0f / 127.0f) *
            (SYNTH_RING_MOD_MAX_RECTIFY - SYNTH_RING_MOD_MIN_RECTIFY));
    const float expected_eq_low =
        SYNTH_EQ_MIN_GAIN_DB +
        ((96.0f / 127.0f) * (SYNTH_EQ_MAX_GAIN_DB - SYNTH_EQ_MIN_GAIN_DB));
    const float expected_eq_mid =
        SYNTH_EQ_MIN_GAIN_DB +
        ((32.0f / 127.0f) * (SYNTH_EQ_MAX_GAIN_DB - SYNTH_EQ_MIN_GAIN_DB));
    const float expected_eq_high =
        SYNTH_EQ_MIN_GAIN_DB +
        ((80.0f / 127.0f) * (SYNTH_EQ_MAX_GAIN_DB - SYNTH_EQ_MIN_GAIN_DB));
    const float expected_compressor_threshold =
        SYNTH_COMPRESSOR_MIN_THRESHOLD_DB +
        ((64.0f / 127.0f) *
            (SYNTH_COMPRESSOR_MAX_THRESHOLD_DB - SYNTH_COMPRESSOR_MIN_THRESHOLD_DB));
    const float expected_compressor_ratio =
        SYNTH_COMPRESSOR_MIN_RATIO +
        ((96.0f / 127.0f) *
            (SYNTH_COMPRESSOR_MAX_RATIO - SYNTH_COMPRESSOR_MIN_RATIO));
    const float expected_compressor_makeup_gain =
        SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB +
        ((44.0f / 127.0f) *
            (SYNTH_COMPRESSOR_MAX_MAKEUP_GAIN_DB - SYNTH_COMPRESSOR_MIN_MAKEUP_GAIN_DB));

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/effect_macro_mapping.conf", error, sizeof(error)),
        "effect macro mapping loads for apply test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    (void)apply_cc_value(&mapping, &s, 10, 77, 0);
    pickup_cc(&mapping, &s, 11, 0, 16);
    expect_true(apply_cc_value(&mapping, &s, 11, 64, &result), "delay macro one applies");
    expect_true(result.effect_bank_index == 0, "delay macro one reports bank one");
    expect_true(result.has_effect, "delay macro one reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_DELAY, "delay macro one reports selected effect");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_DELAY_TIME, "delay macro one reports delay time");
    expect_near(synth_get_delay_time(&s), expected_time, 0.0001f, "delay macro one scales delay time");

    pickup_cc(&mapping, &s, 12, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 12, 96, &result), "delay macro two applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_DELAY_FEEDBACK, "delay macro two reports delay feedback");
    expect_near(synth_get_delay_feedback(&s), expected_feedback, 0.0001f, "delay macro two scales delay feedback");

    pickup_cc(&mapping, &s, 13, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 13, 44, &result), "delay macro three applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_DELAY_MIX, "delay macro three reports delay mix");
    expect_near(synth_get_delay_mix(&s), 44.0f / 127.0f, 0.0001f, "delay macro three scales delay dry wet");

    (void)apply_cc_value(&mapping, &s, 10, 127, 0);
    pickup_cc(&mapping, &s, 11, 0, 22);
    expect_true(apply_cc_value(&mapping, &s, 11, 64, &result), "plate reverb macro one applies");
    expect_true(result.effect_bank_index == 0, "plate reverb macro one reports bank one");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_PLATE_REVERB, "plate reverb macro one reports selected effect");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_DECAY, "plate reverb macro one reports decay");
    expect_near(synth_get_plate_reverb_decay(&s), expected_decay, 0.0001f, "plate reverb macro one scales decay");

    pickup_cc(&mapping, &s, 12, 0, 44);
    expect_true(apply_cc_value(&mapping, &s, 12, 96, &result), "plate reverb macro two applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_DAMPING, "plate reverb macro two reports damping");
    expect_near(synth_get_plate_reverb_damping(&s), 96.0f / 127.0f, 0.0001f, "plate reverb macro two scales damping");

    pickup_cc(&mapping, &s, 13, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 13, 44, &result), "plate reverb macro three applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_PLATE_REVERB_MIX, "plate reverb macro three reports mix");
    expect_near(synth_get_plate_reverb_mix(&s), 44.0f / 127.0f, 0.0001f, "plate reverb macro three scales dry wet");

    (void)apply_cc_value(&mapping, &s, 14, 0, 0);
    pickup_cc(&mapping, &s, 15, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 15, 64, &result), "flanger macro one applies");
    expect_true(result.effect_bank_index == 1, "flanger macro one reports bank two");
    expect_true(result.has_effect, "flanger macro one reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_FLANGER, "flanger macro one reports selected effect");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FLANGER_RATE, "flanger macro one reports rate");
    expect_near(synth_get_flanger_rate(&s), expected_flanger_rate, 0.0001f, "flanger macro one scales rate");

    pickup_cc(&mapping, &s, 16, 0, 96);
    expect_true(apply_cc_value(&mapping, &s, 16, 96, &result), "flanger macro two applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FLANGER_INTENSITY, "flanger macro two reports intensity");
    expect_near(synth_get_flanger_intensity(&s), 96.0f / 127.0f, 0.0001f, "flanger macro two scales intensity");
    expect_near(synth_get_flanger_depth(&s), 96.0f / 127.0f, 0.0001f, "flanger macro two controls depth");
    expect_near(
        synth_get_flanger_feedback(&s),
        expected_flanger_intensity_feedback,
        0.0001f,
        "flanger macro two controls feedback");

    pickup_cc(&mapping, &s, 17, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 17, 44, &result), "flanger macro three applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_FLANGER_MIX, "flanger macro three reports mix");
    expect_near(synth_get_flanger_mix(&s), 44.0f / 127.0f, 0.0001f, "flanger macro three scales dry wet");

    (void)apply_cc_value(&mapping, &s, 14, 26, 0);
    pickup_cc(&mapping, &s, 15, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 15, 64, &result), "ring mod macro one applies");
    expect_true(result.effect_bank_index == 1, "ring mod macro one reports bank two");
    expect_true(result.has_effect, "ring mod macro one reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_RING_MOD, "ring mod macro one reports selected effect");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_RING_MOD_FREQUENCY, "ring mod macro one reports frequency");
    expect_near(
        synth_get_ring_mod_frequency(&s),
        expected_ring_mod_frequency,
        0.0001f,
        "ring mod macro one scales frequency");

    pickup_cc(&mapping, &s, 16, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 16, 96, &result), "ring mod macro two applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_RING_MOD_RECTIFY, "ring mod macro two reports rectify");
    expect_near(
        synth_get_ring_mod_rectify(&s),
        expected_ring_mod_rectify,
        0.0001f,
        "ring mod macro two scales bipolar rectification");

    pickup_cc(&mapping, &s, 17, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 17, 44, &result), "ring mod macro three applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_RING_MOD_MIX, "ring mod macro three reports mix");
    expect_near(synth_get_ring_mod_mix(&s), 44.0f / 127.0f, 0.0001f, "ring mod macro three scales dry wet");

    (void)apply_cc_value(&mapping, &s, 14, 52, 0);
    pickup_cc(&mapping, &s, 15, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 15, 96, &result), "eq macro one applies");
    expect_true(result.effect_bank_index == 1, "eq macro one reports bank two");
    expect_true(result.has_effect, "eq macro one reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_EQ, "eq macro one reports selected effect");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_EQ_LOW, "eq macro one reports low");
    expect_near(synth_get_eq_low(&s), expected_eq_low, 0.0001f, "eq macro one scales low band");

    pickup_cc(&mapping, &s, 16, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 16, 32, &result), "eq macro two applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_EQ_MID, "eq macro two reports mid");
    expect_near(synth_get_eq_mid(&s), expected_eq_mid, 0.0001f, "eq macro two scales mid band");

    pickup_cc(&mapping, &s, 17, 0, 64);
    expect_true(apply_cc_value(&mapping, &s, 17, 80, &result), "eq macro three applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_EQ_HIGH, "eq macro three reports high");
    expect_near(synth_get_eq_high(&s), expected_eq_high, 0.0001f, "eq macro three scales high band");

    (void)apply_cc_value(&mapping, &s, 14, 77, 0);
    pickup_cc(&mapping, &s, 15, 127, 127);
    expect_true(apply_cc_value(&mapping, &s, 15, 64, &result), "compressor macro one applies");
    expect_true(result.effect_bank_index == 1, "compressor macro one reports bank two");
    expect_true(result.has_effect, "compressor macro one reports active effect");
    expect_true(result.effect == MIDI_MAPPING_EFFECT_COMPRESSOR, "compressor macro one reports selected effect");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_THRESHOLD,
        "compressor macro one reports threshold");
    expect_near(
        synth_get_compressor_threshold(&s),
        expected_compressor_threshold,
        0.0001f,
        "compressor macro one scales threshold");

    pickup_cc(&mapping, &s, 16, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 16, 96, &result), "compressor macro two applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_RATIO, "compressor macro two reports ratio");
    expect_near(
        synth_get_compressor_ratio(&s),
        expected_compressor_ratio,
        0.0001f,
        "compressor macro two scales ratio");

    pickup_cc(&mapping, &s, 17, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 17, 44, &result), "compressor macro three applies");
    expect_true(
        result.parameter == MIDI_MAPPING_PARAM_COMPRESSOR_MAKEUP_GAIN,
        "compressor macro three reports makeup gain");
    expect_near(
        synth_get_compressor_makeup_gain(&s),
        expected_compressor_makeup_gain,
        0.0001f,
        "compressor macro three scales makeup gain");

    (void)apply_cc_value(&mapping, &s, 14, 103, 0);
    expect_true(!apply_cc_value(&mapping, &s, 15, 64, &result), "blank bank two macro one does nothing");
    expect_true(!apply_cc_value(&mapping, &s, 16, 64, &result), "blank bank two macro two does nothing");
    expect_true(!apply_cc_value(&mapping, &s, 17, 64, &result), "blank bank two macro three does nothing");
}

static void test_effect_macro_pickup_is_independent_per_effect(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    const float initial_bitcrusher_rate = SYNTH_DEFAULT_SAMPLE_RATE;

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/effect_macro_mapping.conf", error, sizeof(error)),
        "effect macro mapping loads for pickup independence test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    (void)apply_cc_value(&mapping, &s, 10, 77, 0);
    pickup_cc(&mapping, &s, 11, 0, 16);
    expect_true(apply_cc_value(&mapping, &s, 11, 127, &result), "delay macro one is picked up");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_DELAY_TIME, "picked-up delay macro one reports delay time");

    (void)apply_cc_value(&mapping, &s, 10, 52, 0);
    expect_true(
        !apply_cc_value(&mapping, &s, 11, 0, &result),
        "bitcrusher macro one waits for its own pickup state");
    expect_near(
        synth_get_bitcrusher_sample_rate(&s),
        initial_bitcrusher_rate,
        0.0001f,
        "waiting bitcrusher macro leaves sample rate unchanged");
}

static void test_effect_macro_pickup_rearms_when_returning_to_effect(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/effect_macro_mapping.conf", error, sizeof(error)),
        "effect macro mapping loads for pickup rearm test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    (void)apply_cc_value(&mapping, &s, 10, 0, 0);
    pickup_cc(&mapping, &s, 13, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 13, 127, &result), "saturation mix reaches one hundred percent");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_SATURATION_MIX, "saturation mix macro reports saturation mix");
    expect_near(synth_get_saturation_mix(&s), 1.0f, 0.0001f, "saturation mix is at one hundred percent");
    expect_true(
        mapping.effect_banks[0].pickups[MIDI_MAPPING_EFFECT_SATURATION][2].picked_up,
        "saturation macro three is picked up");

    (void)apply_cc_value(&mapping, &s, 10, 25, 0);
    expect_true(
        mapping.effect_banks[0].pickups[MIDI_MAPPING_EFFECT_SATURATION][2].picked_up,
        "selector movement inside the same effect keeps macro pickup");

    (void)apply_cc_value(&mapping, &s, 10, 26, 0);
    pickup_cc(&mapping, &s, 13, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 13, 19, &result), "distortion mix reaches about fifteen percent");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_DISTORTION_MIX, "distortion mix macro reports distortion mix");
    expect_near(synth_get_distortion_mix(&s), 19.0f / 127.0f, 0.0001f, "distortion mix is about fifteen percent");

    (void)apply_cc_value(&mapping, &s, 10, 0, 0);
    expect_true(
        !mapping.effect_banks[0].pickups[MIDI_MAPPING_EFFECT_SATURATION][2].picked_up,
        "returning to saturation clears its macro pickup latch");
    expect_true(
        !apply_cc_value(&mapping, &s, 13, 20, &result),
        "returning to saturation rearms macro pickup");
    expect_near(
        synth_get_saturation_mix(&s),
        1.0f,
        0.0001f,
        "rearmed saturation mix does not jump to the distortion knob position");

    expect_true(apply_cc_value(&mapping, &s, 13, 127, &result), "saturation mix applies again at pickup point");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_SATURATION_MIX, "rearmed macro reports saturation mix");
    expect_near(synth_get_saturation_mix(&s), 1.0f, 0.0001f, "saturation mix remains at pickup value");
}

static void test_effect_macros_leave_unused_macro_empty_and_direct_bindings_working(void)
{
    midi_mapping mapping;
    synth s;
    midi_mapping_apply_result result;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    expect_true(
        midi_mapping_load(&mapping, "tests/fixtures/effect_macro_mapping.conf", error, sizeof(error)),
        "effect macro mapping loads for unused/direct test");
    synth_init(&s, SYNTH_DEFAULT_SAMPLE_RATE);

    (void)apply_cc_value(&mapping, &s, 10, 0, 0);
    expect_true(!apply_cc_value(&mapping, &s, 12, 127, &result), "unused saturation macro two does nothing");

    pickup_cc(&mapping, &s, 13, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 13, 64, &result), "saturation macro three applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_SATURATION_MIX, "saturation macro three reports dry wet");
    expect_near(synth_get_saturation_mix(&s), 64.0f / 127.0f, 0.0001f, "saturation macro three scales dry wet");

    pickup_cc(&mapping, &s, 1, 0, 0);
    expect_true(apply_cc_value(&mapping, &s, 1, 127, &result), "direct attack binding still applies");
    expect_true(result.parameter == MIDI_MAPPING_PARAM_ATTACK, "direct binding reports attack");
    expect_near(synth_get_adsr(&s).attack_seconds, 2.0f, 0.0001f, "direct binding updates attack");
}

int main(void)
{
    test_loads_akai_mapping();
    test_parameter_metadata();
    test_applies_adsr_cc_values();
    test_applies_master_gain_cc_value();
    test_applies_filter_cc_values();
    test_applies_oscillator_morph_cc_value();
    test_applies_second_oscillator_cc_values();
    test_applies_oscillator_mix_cc_values();
    test_applies_stereo_spread_cc_value();
    test_applies_lfo_cc_values();
    test_waits_for_pickup_before_first_knob_change();
    test_names_saturation_parameters();
    test_names_distortion_parameters();
    test_names_bitcrusher_parameters();
    test_names_flanger_parameters();
    test_names_ring_mod_parameters();
    test_names_eq_parameters();
    test_names_delay_parameters();
    test_names_plate_reverb_parameters();
    test_names_compressor_parameters();
    test_applies_distortion_mix_cc_value();
    test_applies_distortion_drive_cc_value();
    test_applies_saturation_cc_values();
    test_applies_bitcrusher_cc_values();
    test_applies_flanger_cc_values();
    test_applies_ring_mod_cc_values();
    test_applies_eq_cc_values();
    test_applies_compressor_cc_values();
    test_applies_delay_cc_values();
    test_applies_plate_reverb_cc_values();
    test_loads_effect_macro_mapping();
    test_rejects_legacy_effect_macro_names();
    test_effect_selectors_use_bank_pages();
    test_effect_macros_apply_selected_effect_parameters();
    test_effect_macro_pickup_is_independent_per_effect();
    test_effect_macro_pickup_rearms_when_returning_to_effect();
    test_effect_macros_leave_unused_macro_empty_and_direct_bindings_working();
    printf("All MIDI mapping tests passed.\n");
    return 0;
}
