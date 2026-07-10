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

int main(void)
{
    synth s;
    synth bend_synth;
    synth_voice *morphed_voice;
    synth_voice *bent_voice;
    synth_voice *held_bend_voice;
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

    synth_render_stereo(&s, &stereo_buffer);
    expect_true(buffer_has_signal(left, 256), "stereo render writes left channel");
    expect_true(buffer_has_signal(right, 256), "stereo render writes right channel");
    expect_true(buffers_match(left, right, 256), "stereo render starts centered");

    if (failures != 0) {
        fprintf(stderr, "%d voice test(s) failed\n", failures);
        return 1;
    }

    printf("All voice tests passed.\n");
    return 0;
}
