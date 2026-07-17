#include "synth/synth.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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

static int buffer_has_signal(const float *buffer, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (fabsf(buffer[i]) > 0.0001f) {
            return 1;
        }
    }

    return 0;
}

static int buffers_match(const float *a, const float *b, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (fabsf(a[i] - b[i]) > 0.0001f) {
            return 0;
        }
    }

    return 1;
}

static float frequency_with_semitone_offset(float frequency, float semitones)
{
    return frequency * powf(2.0f, semitones / 12.0f);
}

static synth_voice *find_voice_by_frequency(synth *s, float frequency)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice->active && fabsf(voice->oscillator.frequency - frequency) < 0.0001f) {
            return voice;
        }
    }

    return 0;
}

static synth_voice *find_voice_by_note(synth *s, int midi_note)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice->active && voice->midi_note == midi_note) {
            return voice;
        }
    }

    return 0;
}

static void test_stereo_spread(void)
{
    synth centered;
    synth wide;
    const synth_adsr immediate_envelope = {0.0f, 0.0f, 1.0f, 0.0f};
    float centered_left[256];
    float centered_right[256];
    float wide_left[256];
    float wide_right[256];
    synth_audio_buffer centered_buffer = {centered_left, centered_right, 256};
    synth_audio_buffer wide_buffer = {wide_left, wide_right, 256};

    synth_init(&centered, 48000.0f);
    synth_init(&wide, 48000.0f);
    synth_set_adsr(&centered, immediate_envelope);
    synth_set_adsr(&wide, immediate_envelope);
    synth_note_on_frequency(&centered, 440.0f, 1.0f);
    synth_note_on_frequency(&wide, 440.0f, 1.0f);
    synth_set_stereo_spread(&wide, 1.0f);

    synth_render_stereo(&centered, &centered_buffer);
    synth_render_stereo(&wide, &wide_buffer);

    expect_true(buffers_match(centered_left, centered_right, 256), "zero spread stays centered");
    expect_true(!buffers_match(wide_left, wide_right, 256), "full spread separates the oscillator pair");

    for (size_t i = 0; i < 256; ++i) {
        expect_near(
            (wide_left[i] + wide_right[i]) * 0.5f,
            centered_left[i],
            0.0001f,
            "spread keeps the mono average unchanged");
    }

    synth_set_stereo_spread(&wide, -1.0f);
    expect_near(wide.stereo_spread, 0.0f, 0.0001f, "stereo spread clamps low");
    synth_set_stereo_spread(&wide, 2.0f);
    expect_near(wide.stereo_spread, 1.0f, 0.0001f, "stereo spread clamps high");
}

static void test_global_lfo(void)
{
    synth continuous;
    synth dry;
    synth modulated;
    const synth_adsr immediate_envelope = {0.0f, 0.0f, 1.0f, 0.0f};
    float silence[20];
    float dry_buffer[256];
    float modulated_buffer[256];

    synth_init(&continuous, 100.0f);
    expect_near(continuous.lfo.frequency_hz, SYNTH_DEFAULT_LFO_RATE_HZ, 0.0001f, "lfo has a default rate");
    expect_near(continuous.lfo_depth, 0.0f, 0.0001f, "lfo starts with no modulation depth");

    synth_set_lfo_rate(&continuous, 2.0f);
    synth_set_lfo_shape_morph(&continuous, 2.0f);
    synth_set_lfo_depth(&continuous, 2.0f);
    synth_set_lfo_first_oscillator_morph_amount(&continuous, -1.0f);
    synth_set_lfo_second_oscillator_morph_amount(&continuous, 2.0f);
    synth_set_lfo_first_oscillator_gain_amount(&continuous, 2.0f);
    synth_set_lfo_second_oscillator_gain_amount(&continuous, -1.0f);
    synth_set_lfo_filter_amount(&continuous, 2.0f);
    expect_near(continuous.lfo_depth, 1.0f, 0.0001f, "lfo depth clamps high");
    expect_near(continuous.lfo.morph, 1.0f, 0.0001f, "lfo shape morph clamps high");
    expect_near(
        continuous.lfo_first_oscillator_morph_amount,
        0.0f,
        0.0001f,
        "first oscillator morph lfo amount clamps low");
    expect_near(
        continuous.lfo_second_oscillator_morph_amount,
        1.0f,
        0.0001f,
        "second oscillator morph lfo amount clamps high");
    expect_near(continuous.lfo_first_oscillator_gain_amount, 1.0f, 0.0001f, "first oscillator lfo amount clamps high");
    expect_near(continuous.lfo_second_oscillator_gain_amount, 0.0f, 0.0001f, "second oscillator lfo amount clamps low");
    expect_near(continuous.lfo_filter_amount, 1.0f, 0.0001f, "filter lfo amount clamps high");

    synth_render_mono(&continuous, silence, 20);
    expect_near(continuous.lfo.phase, 0.4f, 0.0001f, "global lfo runs continuously without active voices");

    synth_init(&dry, 1000.0f);
    synth_init(&modulated, 1000.0f);
    synth_set_adsr(&dry, immediate_envelope);
    synth_set_adsr(&modulated, immediate_envelope);
    synth_set_filter_cutoff(&dry, 200.0f);
    synth_set_filter_cutoff(&modulated, 200.0f);
    synth_set_oscillator_morph(&dry, 0.5f);
    synth_set_oscillator_morph(&modulated, 0.5f);
    synth_note_on_frequency(&dry, 100.0f, 1.0f);
    synth_note_on_frequency(&modulated, 100.0f, 1.0f);
    synth_set_lfo_rate(&modulated, 5.0f);
    synth_set_lfo_shape_morph(&modulated, 0.5f);
    synth_set_lfo_depth(&modulated, 1.0f);
    synth_set_lfo_first_oscillator_morph_amount(&modulated, 0.25f);
    synth_set_lfo_second_oscillator_morph_amount(&modulated, 0.75f);
    synth_set_lfo_first_oscillator_gain_amount(&modulated, 0.75f);
    synth_set_lfo_second_oscillator_gain_amount(&modulated, 0.5f);
    synth_set_lfo_filter_amount(&modulated, 0.25f);

    synth_render_mono(&dry, dry_buffer, 256);
    synth_render_mono(&modulated, modulated_buffer, 256);

    expect_true(!buffers_match(dry_buffer, modulated_buffer, 256), "global lfo changes rendered audio");
    expect_near(modulated.oscillator_morph, 0.5f, 0.0001f, "lfo preserves the base morph setting");
    expect_near(modulated.filter.cutoff_hz, 200.0f, 0.0001f, "lfo preserves the base filter cutoff");
    expect_near(
        modulated.voices[0].oscillator.morph,
        0.5f,
        0.0001f,
        "lfo preserves the first voice oscillator base morph");
    expect_near(
        modulated.voices[0].second_oscillator.morph,
        1.0f,
        0.0001f,
        "lfo preserves the second voice oscillator base morph");
}

static void test_master_gain_scales_after_effects(void)
{
    synth full_gain;
    synth half_gain;
    const synth_adsr immediate_envelope = {0.0f, 0.0f, 1.0f, 0.0f};
    float full_left[256];
    float full_right[256];
    float half_left[256];
    float half_right[256];
    synth_audio_buffer full_buffer = {full_left, full_right, 256};
    synth_audio_buffer half_buffer = {half_left, half_right, 256};

    synth_init(&full_gain, 48000.0f);
    synth_init(&half_gain, 48000.0f);
    synth_set_adsr(&full_gain, immediate_envelope);
    synth_set_adsr(&half_gain, immediate_envelope);
    synth_set_filter_cutoff(&full_gain, 24000.0f);
    synth_set_filter_cutoff(&half_gain, 24000.0f);
    synth_set_distortion_drive(&full_gain, 16.0f);
    synth_set_distortion_drive(&half_gain, 16.0f);
    synth_set_distortion_mix(&full_gain, 1.0f);
    synth_set_distortion_mix(&half_gain, 1.0f);
    synth_set_master_gain(&full_gain, 1.0f);
    synth_set_master_gain(&half_gain, 0.5f);
    synth_note_on_frequency(&full_gain, 110.0f, 1.0f);
    synth_note_on_frequency(&half_gain, 110.0f, 1.0f);

    synth_render_stereo(&full_gain, &full_buffer);
    synth_render_stereo(&half_gain, &half_buffer);

    for (size_t i = 0; i < 256; ++i) {
        expect_near(half_left[i], full_left[i] * 0.5f, 0.0001f, "master gain scales left after effects");
        expect_near(half_right[i], full_right[i] * 0.5f, 0.0001f, "master gain scales right after effects");
    }
}

static void test_delay_controls(void)
{
    synth delay_synth;

    synth_init(&delay_synth, 48000.0f);
    expect_near(
        synth_get_delay_time(&delay_synth),
        SYNTH_DELAY_DEFAULT_TIME_SECONDS,
        0.0001f,
        "synth delay starts at default time");
    expect_near(synth_get_delay_feedback(&delay_synth), 0.0f, 0.0001f, "synth delay starts with no feedback");
    expect_near(synth_get_delay_mix(&delay_synth), 0.0f, 0.0001f, "synth delay starts dry");

    synth_set_delay_time(&delay_synth, 10.0f);
    synth_set_delay_feedback(&delay_synth, 2.0f);
    synth_set_delay_mix(&delay_synth, -1.0f);
    expect_near(synth_get_delay_time(&delay_synth), SYNTH_DELAY_MAX_TIME_SECONDS, 0.0001f, "synth delay time clamps high");
    expect_near(synth_get_delay_feedback(&delay_synth), SYNTH_DELAY_MAX_FEEDBACK, 0.0001f, "synth delay feedback clamps high");
    expect_near(synth_get_delay_mix(&delay_synth), 0.0f, 0.0001f, "synth delay mix clamps low");
}

int main(void)
{
    synth s;
    synth bend_synth;
    synth second_tune_synth;
    synth_voice *morphed_voice;
    synth_voice *bent_voice;
    synth_voice *held_bend_voice;
    synth_voice *second_tune_voice;
    synth_adsr quick_release = {0.0f, 0.0f, 1.0f, 0.001f};
    float buffer[256];
    float left[256];
    float right[256];
    synth_audio_buffer stereo_buffer = {left, right, 256};
    int active_count = 0;

    synth_init(&s, 48000.0f);
    memset(buffer, 1, sizeof(buffer));
    synth_render_mono(&s, buffer, 128);
    expect_true(!buffer_has_signal(buffer, 128), "synth renders silence with no active voices");

    synth_note_on(&s, 60, 1.0f);
    synth_note_on(&s, 64, 1.0f);
    synth_note_on(&s, 67, 1.0f);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (s.voices[i].active) {
            ++active_count;
        }
    }
    expect_true(active_count == 3, "multiple note ons allocate multiple voices");

    synth_render_mono(&s, buffer, 256);
    expect_true(buffer_has_signal(buffer, 256), "active voices render signal");

    synth_set_adsr(&s, quick_release);
    synth_note_on(&s, 69, 1.0f);
    synth_render_mono(&s, buffer, 64);
    synth_note_off(&s, 69);
    synth_render_mono(&s, buffer, 256);
    expect_true(!s.voices[3].active, "note off releases voice");

    synth_set_waveform(&s, SYNTH_WAVEFORM_SQUARE);
    synth_set_filter_cutoff(&s, 900.0f);
    synth_note_on_frequency(&s, 440.0f, 1.0f);
    synth_render_mono(&s, buffer, 256);
    expect_true(buffer_has_signal(buffer, 256), "direct frequency voice renders through filter");

    synth_set_oscillator_morph(&s, 0.75f);
    expect_near(s.oscillator_morph, 0.75f, 0.0001f, "synth stores oscillator morph");
    synth_note_on_frequency(&s, 220.0f, 0.8f);
    morphed_voice = find_voice_by_frequency(&s, 220.0f);
    expect_true(morphed_voice != 0, "morphed frequency voice starts");
    if (morphed_voice != 0) {
        expect_near(morphed_voice->oscillator.morph, 0.75f, 0.0001f, "new voice receives default morph");
        expect_true(
            morphed_voice->second_oscillator.waveform == SYNTH_WAVEFORM_SQUARE,
            "second oscillator starts as square");
        expect_near(
            morphed_voice->second_oscillator.frequency,
            220.0f,
            0.0001f,
            "second oscillator starts at voice frequency");
        expect_near(
            morphed_voice->second_oscillator.morph,
            1.0f,
            0.0001f,
            "second oscillator uses square morph");
    }
    synth_set_oscillator_morph(&s, 2.0f);
    expect_near(s.oscillator_morph, 1.0f, 0.0001f, "synth oscillator morph clamps high");

    synth_init(&bend_synth, 48000.0f);
    synth_note_on(&bend_synth, 69, 1.0f);
    bent_voice = find_voice_by_note(&bend_synth, 69);
    expect_true(bent_voice != 0, "pitch bend test voice starts");
    if (bent_voice != 0) {
        expect_near(bent_voice->base_frequency, 440.0f, 0.001f, "pitch bend keeps base frequency");
        synth_set_pitch_bend(&bend_synth, 1.0f);
        expect_near(
            bent_voice->oscillator.frequency,
            synth_midi_note_to_frequency(70),
            0.001f,
            "full pitch bend up retunes by one semitone");
        expect_near(
            bent_voice->second_oscillator.frequency,
            synth_midi_note_to_frequency(70),
            0.001f,
            "full pitch bend up retunes second oscillator");
        synth_set_pitch_bend(&bend_synth, -1.0f);
        expect_near(
            bent_voice->oscillator.frequency,
            synth_midi_note_to_frequency(68),
            0.001f,
            "full pitch bend down retunes by one semitone");
        expect_near(
            bent_voice->second_oscillator.frequency,
            synth_midi_note_to_frequency(68),
            0.001f,
            "full pitch bend down retunes second oscillator");
        synth_set_pitch_bend(&bend_synth, 0.0f);
        expect_near(bent_voice->oscillator.frequency, 440.0f, 0.001f, "center pitch bend restores base pitch");
        expect_near(
            bent_voice->second_oscillator.frequency,
            440.0f,
            0.001f,
            "center pitch bend restores second oscillator base pitch");
    }

    synth_set_pitch_bend(&bend_synth, 1.0f);
    synth_note_on(&bend_synth, 72, 1.0f);
    held_bend_voice = find_voice_by_note(&bend_synth, 72);
    expect_true(held_bend_voice != 0, "new voice starts while pitch bend is held");
    if (held_bend_voice != 0) {
        expect_near(
            held_bend_voice->oscillator.frequency,
            synth_midi_note_to_frequency(73),
            0.001f,
            "new voice receives held pitch bend");
        expect_near(
            held_bend_voice->second_oscillator.frequency,
            synth_midi_note_to_frequency(73),
            0.001f,
            "new voice second oscillator receives held pitch bend");
    }

    synth_init(&second_tune_synth, 48000.0f);
    expect_true(second_tune_synth.second_oscillator_octave == 0, "second oscillator octave defaults to zero");
    expect_true(
        second_tune_synth.second_oscillator_pitch_semitones == 0,
        "second oscillator pitch defaults to zero");
    expect_near(
        second_tune_synth.second_oscillator_fine_tune_cents,
        0.0f,
        0.0001f,
        "second oscillator fine tune defaults to zero");
    expect_near(second_tune_synth.first_oscillator_gain, 1.0f, 0.0001f, "first oscillator gain defaults to full");
    expect_near(second_tune_synth.second_oscillator_gain, 1.0f, 0.0001f, "second oscillator gain defaults to full");
    expect_near(second_tune_synth.second_oscillator_morph, 1.0f, 0.0001f, "second oscillator morph defaults to square");

    synth_note_on_frequency(&second_tune_synth, 440.0f, 1.0f);
    second_tune_voice = find_voice_by_frequency(&second_tune_synth, 440.0f);
    expect_true(second_tune_voice != 0, "second oscillator tuning test voice starts");
    if (second_tune_voice != 0) {
        synth_set_second_oscillator_octave(&second_tune_synth, 1);
        expect_true(second_tune_synth.second_oscillator_octave == 1, "second oscillator octave stores upper octave");
        expect_near(second_tune_voice->oscillator.frequency, 440.0f, 0.001f, "second tuning keeps primary pitch");
        expect_near(second_tune_voice->second_oscillator.frequency, 880.0f, 0.001f, "second oscillator octave retunes active voice");

        synth_set_second_oscillator_pitch(&second_tune_synth, -6);
        expect_near(
            second_tune_voice->second_oscillator.frequency,
            frequency_with_semitone_offset(440.0f, 6.0f),
            0.001f,
            "second oscillator semitone pitch retunes active voice");

        synth_set_second_oscillator_fine_tune(&second_tune_synth, 50.0f);
        expect_near(
            second_tune_voice->second_oscillator.frequency,
            frequency_with_semitone_offset(440.0f, 6.5f),
            0.001f,
            "second oscillator fine tune retunes active voice");

        synth_set_second_oscillator_octave(&second_tune_synth, -4);
        synth_set_second_oscillator_pitch(&second_tune_synth, 99);
        synth_set_second_oscillator_fine_tune(&second_tune_synth, -99.0f);
        expect_true(second_tune_synth.second_oscillator_octave == -1, "second oscillator octave clamps low");
        expect_true(
            second_tune_synth.second_oscillator_pitch_semitones == 6,
            "second oscillator pitch clamps high");
        expect_near(
            second_tune_synth.second_oscillator_fine_tune_cents,
            -50.0f,
            0.0001f,
            "second oscillator fine tune clamps low");

        synth_set_first_oscillator_gain(&second_tune_synth, 2.0f);
        synth_set_second_oscillator_gain(&second_tune_synth, -1.0f);
        synth_set_second_oscillator_morph(&second_tune_synth, 0.25f);
        expect_near(second_tune_synth.first_oscillator_gain, 1.0f, 0.0001f, "first oscillator gain clamps high");
        expect_near(second_tune_synth.second_oscillator_gain, 0.0f, 0.0001f, "second oscillator gain clamps low");
        expect_near(second_tune_synth.second_oscillator_morph, 0.25f, 0.0001f, "second oscillator morph stores value");
        expect_near(
            second_tune_voice->second_oscillator.morph,
            0.25f,
            0.0001f,
            "second oscillator morph updates active voice");
    }

    synth_render_stereo(&s, &stereo_buffer);
    expect_true(buffer_has_signal(left, 256), "stereo render writes left channel");
    expect_true(buffer_has_signal(right, 256), "stereo render writes right channel");
    expect_true(buffers_match(left, right, 256), "stereo render starts centered");

    test_stereo_spread();
    test_global_lfo();
    test_master_gain_scales_after_effects();
    test_delay_controls();

    if (failures != 0) {
        fprintf(stderr, "%d voice test(s) failed\n", failures);
        return 1;
    }

    printf("All voice tests passed.\n");
    return 0;
}
