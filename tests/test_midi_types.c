#include "synth/midi_types.h"

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
    synth_midi_message message;
    const unsigned char note_on[] = {0x90, 60, 100};
    const unsigned char note_off[] = {0x80, 60, 64};
    const unsigned char note_on_zero_velocity[] = {0x90, 61, 0};
    const unsigned char pitch_bend_center[] = {0xE0, 0x00, 0x40};
    const unsigned char pitch_bend_full_up[] = {0xE0, 0x7F, 0x7F};
    const unsigned char pitch_bend_full_down_channel_2[] = {0xE1, 0x00, 0x00};

    expect_near(synth_midi_note_to_frequency(69), 440.0f, 0.001f, "MIDI note 69 is A440");
    expect_near(synth_midi_note_to_frequency(57), 220.0f, 0.001f, "MIDI note 57 is A220");
    expect_near(synth_midi_note_to_frequency(60), 261.6256f, 0.01f, "MIDI note 60 is middle C");

    expect_true(synth_midi_parse_short_message(note_on, sizeof(note_on), &message), "note on parses");
    expect_true(message.type == SYNTH_MIDI_MESSAGE_NOTE_ON, "note on type");
    expect_true(message.channel == 1, "note on channel");
    expect_true(message.note == 60, "note on note");
    expect_near(message.velocity, 100.0f / 127.0f, 0.0001f, "note on velocity");

    expect_true(synth_midi_parse_short_message(note_off, sizeof(note_off), &message), "note off parses");
    expect_true(message.type == SYNTH_MIDI_MESSAGE_NOTE_OFF, "note off type");

    expect_true(synth_midi_parse_short_message(note_on_zero_velocity, sizeof(note_on_zero_velocity), &message), "zero velocity note on parses");
    expect_true(message.type == SYNTH_MIDI_MESSAGE_NOTE_OFF, "zero velocity note on is note off");
    expect_true(message.note == 61, "zero velocity note on note");

    expect_true(synth_midi_parse_short_message(pitch_bend_center, sizeof(pitch_bend_center), &message), "pitch bend center parses");
    expect_true(message.type == SYNTH_MIDI_MESSAGE_PITCH_BEND, "pitch bend center type");
    expect_true(message.channel == 1, "pitch bend center channel");
    expect_true(message.pitch_bend_value == 0, "pitch bend center value");
    expect_near(message.pitch_bend, 0.0f, 0.0001f, "pitch bend center normalizes to zero");

    expect_true(synth_midi_parse_short_message(pitch_bend_full_up, sizeof(pitch_bend_full_up), &message), "pitch bend full up parses");
    expect_true(message.pitch_bend_value == 8191, "pitch bend full up value");
    expect_near(message.pitch_bend, 1.0f, 0.0001f, "pitch bend full up normalizes to one");

    expect_true(
        synth_midi_parse_short_message(pitch_bend_full_down_channel_2, sizeof(pitch_bend_full_down_channel_2), &message),
        "pitch bend full down parses");
    expect_true(message.channel == 2, "pitch bend channel 2");
    expect_true(message.pitch_bend_value == -8192, "pitch bend full down value");
    expect_near(message.pitch_bend, -1.0f, 0.0001f, "pitch bend full down normalizes to negative one");

    if (failures != 0) {
        fprintf(stderr, "%d MIDI type test(s) failed\n", failures);
        return 1;
    }

    printf("All MIDI type tests passed.\n");
    return 0;
}
