#include "midi/chord_mode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct event_log {
    midi_chord_mode_note_event events[64];
    size_t count;
} event_log;

#define TEST_PAD_CHANNEL 3
#define TEST_FIRST_PAD_CC 53

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

static void emit_to_log(void *user_data, const midi_chord_mode_note_event *event)
{
    event_log *log = (event_log *)user_data;

    expect_true(log->count < sizeof(log->events) / sizeof(log->events[0]), "event log has room");
    log->events[log->count] = *event;
    log->count += 1;
}

static void clear_log(event_log *log)
{
    memset(log, 0, sizeof(*log));
}

static void send_cc(midi_chord_mode *mode, int control, int value, event_log *log)
{
    const unsigned char data[] = {0xB0 + (TEST_PAD_CHANNEL - 1), (unsigned char)control, (unsigned char)value};

    expect_true(
        midi_chord_mode_handle_short_message(mode, data, sizeof(data), emit_to_log, log),
        "chord pad cc is consumed");
}

static void bind_test_pads(midi_chord_mode *mode)
{
    int pad;

    for (pad = 0; pad < MIDI_CHORD_MODE_PAD_COUNT; ++pad) {
        midi_chord_mode_bind_pad(mode, (midi_chord_mode_pad)pad, TEST_PAD_CHANNEL, TEST_FIRST_PAD_CC + pad);
    }
}

static void press_pad(midi_chord_mode *mode, midi_chord_mode_pad pad, event_log *log)
{
    send_cc(mode, TEST_FIRST_PAD_CC + pad, 127, log);
}

static void release_pad(midi_chord_mode *mode, midi_chord_mode_pad pad, event_log *log)
{
    send_cc(mode, TEST_FIRST_PAD_CC + pad, 0, log);
}

static void expect_note_set(
    const midi_chord_mode_note_set *notes,
    const int *expected_notes,
    size_t expected_count,
    const char *message)
{
    size_t index;

    expect_true(notes->count == expected_count, message);
    for (index = 0; index < expected_count; ++index) {
        expect_true(notes->notes[index] == expected_notes[index], message);
    }
}

static void expect_event(
    const event_log *log,
    size_t index,
    midi_chord_mode_event_type type,
    int note,
    const char *message)
{
    expect_true(index < log->count, message);
    expect_true(log->events[index].type == type, message);
    expect_true(log->events[index].note == note, message);
}

static void test_major_minor_seven_nine_builds_dominant_nine(void)
{
    midi_chord_mode mode;
    midi_chord_mode_note_set notes;
    event_log log;
    const int expected[] = {60, 64, 67, 70, 74};

    midi_chord_mode_init(&mode);
    bind_test_pads(&mode);
    clear_log(&log);

    press_pad(&mode, MIDI_CHORD_MODE_PAD_MAJOR, &log);
    press_pad(&mode, MIDI_CHORD_MODE_PAD_MINOR_SEVENTH, &log);
    press_pad(&mode, MIDI_CHORD_MODE_PAD_NINTH, &log);
    midi_chord_mode_build_notes(&mode, 60, &notes);

    expect_note_set(&notes, expected, sizeof(expected) / sizeof(expected[0]), "major flat-seven nine builds dominant nine");
}

static void test_extensions_are_additive_without_chord_type(void)
{
    midi_chord_mode mode;
    midi_chord_mode_note_set notes;
    event_log log;
    const int expected[] = {60, 69, 74};

    midi_chord_mode_init(&mode);
    bind_test_pads(&mode);
    clear_log(&log);

    press_pad(&mode, MIDI_CHORD_MODE_PAD_SIXTH, &log);
    press_pad(&mode, MIDI_CHORD_MODE_PAD_NINTH, &log);
    midi_chord_mode_build_notes(&mode, 60, &notes);

    expect_note_set(&notes, expected, sizeof(expected) / sizeof(expected[0]), "extensions add directly to the root");
}

static void test_last_pressed_chord_type_falls_back_to_held_type(void)
{
    midi_chord_mode mode;
    midi_chord_mode_note_set notes;
    event_log log;
    const int minor_expected[] = {60, 63, 67};
    const int major_expected[] = {60, 64, 67};

    midi_chord_mode_init(&mode);
    bind_test_pads(&mode);
    clear_log(&log);

    press_pad(&mode, MIDI_CHORD_MODE_PAD_MAJOR, &log);
    press_pad(&mode, MIDI_CHORD_MODE_PAD_MINOR, &log);
    midi_chord_mode_build_notes(&mode, 60, &notes);
    expect_note_set(&notes, minor_expected, sizeof(minor_expected) / sizeof(minor_expected[0]), "last chord type wins");

    release_pad(&mode, MIDI_CHORD_MODE_PAD_MINOR, &log);
    midi_chord_mode_build_notes(&mode, 60, &notes);
    expect_note_set(&notes, major_expected, sizeof(major_expected) / sizeof(major_expected[0]), "released chord type falls back");
}

static void test_pad_changes_update_held_notes_immediately(void)
{
    midi_chord_mode mode;
    event_log log;

    midi_chord_mode_init(&mode);
    bind_test_pads(&mode);
    clear_log(&log);

    press_pad(&mode, MIDI_CHORD_MODE_PAD_MAJOR, &log);
    midi_chord_mode_note_on(&mode, 60, 0.5f, emit_to_log, &log);
    expect_true(log.count == 3, "major note on emits three notes");
    expect_event(&log, 0, MIDI_CHORD_MODE_EVENT_NOTE_ON, 60, "major root starts");
    expect_event(&log, 1, MIDI_CHORD_MODE_EVENT_NOTE_ON, 64, "major third starts");
    expect_event(&log, 2, MIDI_CHORD_MODE_EVENT_NOTE_ON, 67, "major fifth starts");
    expect_near(log.events[0].velocity, 0.5f, 0.0001f, "root velocity is preserved");

    clear_log(&log);
    press_pad(&mode, MIDI_CHORD_MODE_PAD_MINOR, &log);
    expect_true(log.count == 2, "minor pad swaps the third");
    expect_event(&log, 0, MIDI_CHORD_MODE_EVENT_NOTE_OFF, 64, "major third stops");
    expect_event(&log, 1, MIDI_CHORD_MODE_EVENT_NOTE_ON, 63, "minor third starts");

    clear_log(&log);
    release_pad(&mode, MIDI_CHORD_MODE_PAD_MINOR, &log);
    expect_true(log.count == 2, "minor release falls back to major");
    expect_event(&log, 0, MIDI_CHORD_MODE_EVENT_NOTE_OFF, 63, "minor third stops");
    expect_event(&log, 1, MIDI_CHORD_MODE_EVENT_NOTE_ON, 64, "major third restarts");
}

static void test_shared_generated_notes_release_only_after_last_root(void)
{
    midi_chord_mode mode;
    event_log log;

    midi_chord_mode_init(&mode);
    bind_test_pads(&mode);
    clear_log(&log);

    press_pad(&mode, MIDI_CHORD_MODE_PAD_MAJOR, &log);
    midi_chord_mode_note_on(&mode, 60, 0.8f, emit_to_log, &log);
    midi_chord_mode_note_on(&mode, 64, 0.7f, emit_to_log, &log);
    expect_true(log.count == 5, "shared root note is not duplicated");
    expect_event(&log, 0, MIDI_CHORD_MODE_EVENT_NOTE_ON, 60, "c chord root starts");
    expect_event(&log, 1, MIDI_CHORD_MODE_EVENT_NOTE_ON, 64, "c chord third starts");
    expect_event(&log, 2, MIDI_CHORD_MODE_EVENT_NOTE_ON, 67, "c chord fifth starts");
    expect_event(&log, 3, MIDI_CHORD_MODE_EVENT_NOTE_ON, 68, "e chord third starts");
    expect_event(&log, 4, MIDI_CHORD_MODE_EVENT_NOTE_ON, 71, "e chord fifth starts");

    clear_log(&log);
    midi_chord_mode_note_off(&mode, 60, emit_to_log, &log);
    expect_true(log.count == 2, "shared e stays held after c releases");
    expect_event(&log, 0, MIDI_CHORD_MODE_EVENT_NOTE_OFF, 60, "c root stops");
    expect_event(&log, 1, MIDI_CHORD_MODE_EVENT_NOTE_OFF, 67, "c fifth stops");

    clear_log(&log);
    midi_chord_mode_note_off(&mode, 64, emit_to_log, &log);
    expect_true(log.count == 3, "remaining e chord releases");
    expect_event(&log, 0, MIDI_CHORD_MODE_EVENT_NOTE_OFF, 64, "shared e stops last");
    expect_event(&log, 1, MIDI_CHORD_MODE_EVENT_NOTE_OFF, 68, "e chord third stops");
    expect_event(&log, 2, MIDI_CHORD_MODE_EVENT_NOTE_OFF, 71, "e chord fifth stops");
}

static void test_chord_mode_only_consumes_pad_ccs(void)
{
    midi_chord_mode mode;
    event_log log;
    const unsigned char note_on[] = {0x90, 60, 100};
    const unsigned char other_cc[] = {0xB0 + (TEST_PAD_CHANNEL - 1), 32, 127};
    const unsigned char wrong_channel_pad_cc[] = {0xB0, TEST_FIRST_PAD_CC, 1};
    const unsigned char pad_cc[] = {0xB0 + (TEST_PAD_CHANNEL - 1), TEST_FIRST_PAD_CC, 1};

    midi_chord_mode_init(&mode);
    bind_test_pads(&mode);
    clear_log(&log);

    expect_true(
        !midi_chord_mode_handle_short_message(&mode, note_on, sizeof(note_on), emit_to_log, &log),
        "note messages are not consumed");
    expect_true(
        !midi_chord_mode_handle_short_message(&mode, other_cc, sizeof(other_cc), emit_to_log, &log),
        "non-pad ccs are not consumed");
    expect_true(
        !midi_chord_mode_handle_short_message(&mode, wrong_channel_pad_cc, sizeof(wrong_channel_pad_cc), emit_to_log, &log),
        "pad ccs on other channels are not consumed");
    expect_true(
        midi_chord_mode_handle_short_message(&mode, pad_cc, sizeof(pad_cc), emit_to_log, &log),
        "pad ccs are consumed");
}

int main(void)
{
    test_major_minor_seven_nine_builds_dominant_nine();
    test_extensions_are_additive_without_chord_type();
    test_last_pressed_chord_type_falls_back_to_held_type();
    test_pad_changes_update_held_notes_immediately();
    test_shared_generated_notes_release_only_after_last_root();
    test_chord_mode_only_consumes_pad_ccs();

    printf("All chord mode tests passed.\n");
    return 0;
}
