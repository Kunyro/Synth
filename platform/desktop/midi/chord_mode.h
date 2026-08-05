#ifndef DESKTOP_MIDI_CHORD_MODE_H
#define DESKTOP_MIDI_CHORD_MODE_H

#include <stddef.h>

// the first cc number reserved for chord-mode pads.
#define MIDI_CHORD_MODE_FIRST_PAD_CC 33
// the last cc number reserved for chord-mode pads.
#define MIDI_CHORD_MODE_LAST_PAD_CC 40
// the most notes one root can generate.
#define MIDI_CHORD_MODE_MAX_NOTES 8
// the number of midi notes the note tracker supports.
#define MIDI_CHORD_MODE_NOTE_COUNT 128

// the chord-mode pad roles in controller order.
typedef enum midi_chord_mode_pad {
    MIDI_CHORD_MODE_PAD_DIMINISHED = 0,
    MIDI_CHORD_MODE_PAD_MINOR,
    MIDI_CHORD_MODE_PAD_MAJOR,
    MIDI_CHORD_MODE_PAD_SUSPENDED,
    MIDI_CHORD_MODE_PAD_SIXTH,
    MIDI_CHORD_MODE_PAD_MINOR_SEVENTH,
    MIDI_CHORD_MODE_PAD_MAJOR_SEVENTH,
    MIDI_CHORD_MODE_PAD_NINTH,
    MIDI_CHORD_MODE_PAD_COUNT
} midi_chord_mode_pad;

// the note event kind emitted by chord mode.
typedef enum midi_chord_mode_event_type {
    MIDI_CHORD_MODE_EVENT_NOTE_ON = 0,
    MIDI_CHORD_MODE_EVENT_NOTE_OFF
} midi_chord_mode_event_type;

// a compact list of generated midi notes.
typedef struct midi_chord_mode_note_set {
    int notes[MIDI_CHORD_MODE_MAX_NOTES];
    size_t count;
} midi_chord_mode_note_set;

// one note event emitted after chord expansion.
typedef struct midi_chord_mode_note_event {
    midi_chord_mode_event_type type;
    int note;
    float velocity;
} midi_chord_mode_note_event;

// a callback used by chord mode to emit expanded note events.
typedef void (*midi_chord_mode_emit_callback)(
    void *user_data,
    const midi_chord_mode_note_event *event);

// one held keyboard note and the chord notes it currently requests.
typedef struct midi_chord_mode_held_note {
    int held;
    float velocity;
    midi_chord_mode_note_set notes;
} midi_chord_mode_held_note;

// the chord-mode state held by the desktop midi layer.
typedef struct midi_chord_mode {
    int pads[MIDI_CHORD_MODE_PAD_COUNT];
    unsigned int chord_type_order;
    unsigned int chord_type_press_order[4];
    midi_chord_mode_held_note held_notes[MIDI_CHORD_MODE_NOTE_COUNT];
    int sounding_counts[MIDI_CHORD_MODE_NOTE_COUNT];
} midi_chord_mode;

// clears chord-mode state.
void midi_chord_mode_init(midi_chord_mode *mode);
// returns whether a cc number belongs to the chord-mode pad range.
int midi_chord_mode_is_pad_cc(int control);
// returns the generated notes for the current pad state and one root.
void midi_chord_mode_build_notes(
    const midi_chord_mode *mode,
    int root_note,
    midi_chord_mode_note_set *notes);
// handles a raw midi short message when it is one of the chord pad ccs.
int midi_chord_mode_handle_short_message(
    midi_chord_mode *mode,
    const unsigned char *data,
    unsigned short length,
    midi_chord_mode_emit_callback emit,
    void *user_data);
// starts a keyboard note through chord mode.
void midi_chord_mode_note_on(
    midi_chord_mode *mode,
    int midi_note,
    float velocity,
    midi_chord_mode_emit_callback emit,
    void *user_data);
// releases a keyboard note through chord mode.
void midi_chord_mode_note_off(
    midi_chord_mode *mode,
    int midi_note,
    midi_chord_mode_emit_callback emit,
    void *user_data);
// releases every note currently owned by chord mode.
void midi_chord_mode_all_notes_off(
    midi_chord_mode *mode,
    midi_chord_mode_emit_callback emit,
    void *user_data);

#endif
