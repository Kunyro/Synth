#include "midi/chord_mode.h"

#include <string.h>

// returns true when a midi note is inside the supported 0-127 range.
static int valid_midi_note(int midi_note)
{
    return midi_note >= 0 && midi_note < MIDI_CHORD_MODE_NOTE_COUNT;
}

// returns true when a chord pad value means held.
static int pad_value_is_held(int value)
{
    return value > 0;
}

// emits one note event when a callback was supplied.
static void emit_note(
    midi_chord_mode_emit_callback emit,
    void *user_data,
    midi_chord_mode_event_type type,
    int note,
    float velocity)
{
    midi_chord_mode_note_event event;

    if (emit == 0) {
        return;
    }

    event.type = type;
    event.note = note;
    event.velocity = velocity;
    emit(user_data, &event);
}

// adds a note to a set while preserving uniqueness and midi range.
static void add_unique_note(midi_chord_mode_note_set *notes, int midi_note)
{
    size_t index;

    if (!valid_midi_note(midi_note) || notes->count >= MIDI_CHORD_MODE_MAX_NOTES) {
        return;
    }

    for (index = 0; index < notes->count; ++index) {
        if (notes->notes[index] == midi_note) {
            return;
        }
    }

    notes->notes[notes->count] = midi_note;
    notes->count += 1;
}

// sorts a note set from low to high for stable output.
static void sort_notes(midi_chord_mode_note_set *notes)
{
    size_t i;

    for (i = 1; i < notes->count; ++i) {
        const int value = notes->notes[i];
        size_t j = i;

        while (j > 0 && notes->notes[j - 1] > value) {
            notes->notes[j] = notes->notes[j - 1];
            --j;
        }

        notes->notes[j] = value;
    }
}

// returns true when a note set contains one midi note.
static int note_set_contains(const midi_chord_mode_note_set *notes, int midi_note)
{
    size_t index;

    for (index = 0; index < notes->count; ++index) {
        if (notes->notes[index] == midi_note) {
            return 1;
        }
    }

    return 0;
}

// finds the velocity from the held root that created a rebuilt note.
static float velocity_for_rebuilt_note(
    const midi_chord_mode *mode,
    const midi_chord_mode_note_set *new_notes,
    int midi_note)
{
    int root;

    for (root = 0; root < MIDI_CHORD_MODE_NOTE_COUNT; ++root) {
        if (mode->held_notes[root].held && note_set_contains(&new_notes[root], midi_note)) {
            return mode->held_notes[root].velocity;
        }
    }

    // new_counts says this note exists, so this fallback only protects the invariant.
    return 1.0f;
}

// remembers that a chord type pad was pressed after older held type pads.
static void mark_chord_type_pressed(midi_chord_mode *mode, midi_chord_mode_pad pad)
{
    const size_t index = (size_t)pad;

    mode->chord_type_order += 1;
    if (mode->chord_type_order == 0) {
        mode->chord_type_order = 1;
    }

    mode->chord_type_press_order[index] = mode->chord_type_order;
}

// returns the held chord type with the newest press order.
static int current_chord_type(const midi_chord_mode *mode)
{
    int selected = -1;
    unsigned int selected_order = 0;
    int pad;

    for (pad = MIDI_CHORD_MODE_PAD_DIMINISHED; pad <= MIDI_CHORD_MODE_PAD_SUSPENDED; ++pad) {
        if (mode->pads[pad] && mode->chord_type_press_order[pad] >= selected_order) {
            selected = pad;
            selected_order = mode->chord_type_press_order[pad];
        }
    }

    return selected;
}

// returns the configured pad for a cc message, or -1 when none match.
static int pad_for_cc_message(const midi_chord_mode *mode, int channel, int control)
{
    int pad;

    for (pad = 0; pad < MIDI_CHORD_MODE_PAD_COUNT; ++pad) {
        const midi_chord_mode_pad_binding *binding = &mode->bindings[pad];

        if (binding->enabled && binding->channel == channel && binding->control == control) {
            return binding->pad;
        }
    }

    return -1;
}

// adds the triad intervals for the active chord type.
static void add_chord_type_notes(
    const midi_chord_mode *mode,
    int root_note,
    midi_chord_mode_note_set *notes)
{
    switch (current_chord_type(mode)) {
        case MIDI_CHORD_MODE_PAD_DIMINISHED:
            add_unique_note(notes, root_note + 3);
            add_unique_note(notes, root_note + 6);
            break;

        case MIDI_CHORD_MODE_PAD_MINOR:
            add_unique_note(notes, root_note + 3);
            add_unique_note(notes, root_note + 7);
            break;

        case MIDI_CHORD_MODE_PAD_MAJOR:
            add_unique_note(notes, root_note + 4);
            add_unique_note(notes, root_note + 7);
            break;

        case MIDI_CHORD_MODE_PAD_SUSPENDED:
            add_unique_note(notes, root_note + 5);
            add_unique_note(notes, root_note + 7);
            break;

        default:
            break;
    }
}

// adds every held extension pad as an interval above the root.
static void add_extension_notes(
    const midi_chord_mode *mode,
    int root_note,
    midi_chord_mode_note_set *notes)
{
    if (mode->pads[MIDI_CHORD_MODE_PAD_SIXTH]) {
        add_unique_note(notes, root_note + 9);
    }

    if (mode->pads[MIDI_CHORD_MODE_PAD_MINOR_SEVENTH]) {
        add_unique_note(notes, root_note + 10);
    }

    if (mode->pads[MIDI_CHORD_MODE_PAD_MAJOR_SEVENTH]) {
        add_unique_note(notes, root_note + 11);
    }

    if (mode->pads[MIDI_CHORD_MODE_PAD_NINTH]) {
        add_unique_note(notes, root_note + 14);
    }
}

// rebuilds the sounding notes after a pad-state change.
static void refresh_held_notes(
    midi_chord_mode *mode,
    midi_chord_mode_emit_callback emit,
    void *user_data)
{
    int new_counts[MIDI_CHORD_MODE_NOTE_COUNT];
    midi_chord_mode_note_set new_notes[MIDI_CHORD_MODE_NOTE_COUNT];
    int root;
    int note;

    memset(new_counts, 0, sizeof(new_counts));
    memset(new_notes, 0, sizeof(new_notes));

    for (root = 0; root < MIDI_CHORD_MODE_NOTE_COUNT; ++root) {
        if (mode->held_notes[root].held) {
            size_t index;

            midi_chord_mode_build_notes(mode, root, &new_notes[root]);
            for (index = 0; index < new_notes[root].count; ++index) {
                new_counts[new_notes[root].notes[index]] += 1;
            }
        }
    }

    for (note = 0; note < MIDI_CHORD_MODE_NOTE_COUNT; ++note) {
        if (mode->sounding_counts[note] > 0 && new_counts[note] == 0) {
            emit_note(emit, user_data, MIDI_CHORD_MODE_EVENT_NOTE_OFF, note, 0.0f);
        }
    }

    for (note = 0; note < MIDI_CHORD_MODE_NOTE_COUNT; ++note) {
        if (mode->sounding_counts[note] == 0 && new_counts[note] > 0) {
            const float velocity = velocity_for_rebuilt_note(mode, new_notes, note);

            emit_note(emit, user_data, MIDI_CHORD_MODE_EVENT_NOTE_ON, note, velocity);
        }
    }

    for (root = 0; root < MIDI_CHORD_MODE_NOTE_COUNT; ++root) {
        if (mode->held_notes[root].held) {
            mode->held_notes[root].notes = new_notes[root];
        }
    }

    memcpy(mode->sounding_counts, new_counts, sizeof(mode->sounding_counts));
}

// clears chord-mode state.
void midi_chord_mode_init(midi_chord_mode *mode)
{
    memset(mode, 0, sizeof(*mode));
}

// binds one midi cc to a chord-mode pad.
void midi_chord_mode_bind_pad(
    midi_chord_mode *mode,
    midi_chord_mode_pad pad,
    int channel,
    int control)
{
    if (mode == 0 || pad < 0 || pad >= MIDI_CHORD_MODE_PAD_COUNT) {
        return;
    }

    mode->bindings[pad].enabled = 1;
    mode->bindings[pad].channel = channel;
    mode->bindings[pad].control = control;
    mode->bindings[pad].pad = pad;
}

// removes all configured chord-mode pad bindings.
void midi_chord_mode_clear_pad_bindings(midi_chord_mode *mode)
{
    if (mode == 0) {
        return;
    }

    memset(mode->bindings, 0, sizeof(mode->bindings));
}

// returns the generated notes for the current pad state and one root.
void midi_chord_mode_build_notes(
    const midi_chord_mode *mode,
    int root_note,
    midi_chord_mode_note_set *notes)
{
    memset(notes, 0, sizeof(*notes));

    add_unique_note(notes, root_note);
    add_chord_type_notes(mode, root_note, notes);
    add_extension_notes(mode, root_note, notes);
    sort_notes(notes);
}

// handles a raw midi short message when it is one of the chord pad ccs.
int midi_chord_mode_handle_short_message(
    midi_chord_mode *mode,
    const unsigned char *data,
    unsigned short length,
    midi_chord_mode_emit_callback emit,
    void *user_data)
{
    unsigned char status;
    int channel;
    int control;
    int value;
    int pad;
    int held;

    if (mode == 0 || data == 0 || length < 3) {
        return 0;
    }

    status = data[0] & 0xF0;
    if (status != 0xB0) {
        return 0;
    }

    channel = (data[0] & 0x0F) + 1;
    control = data[1];
    pad = pad_for_cc_message(mode, channel, control);
    if (pad < 0) {
        return 0;
    }

    value = data[2];
    held = pad_value_is_held(value);

    if (mode->pads[pad] == held) {
        return 1;
    }

    mode->pads[pad] = held;
    if (held && pad <= MIDI_CHORD_MODE_PAD_SUSPENDED) {
        mark_chord_type_pressed(mode, pad);
    }

    refresh_held_notes(mode, emit, user_data);
    return 1;
}

// starts a keyboard note through chord mode.
void midi_chord_mode_note_on(
    midi_chord_mode *mode,
    int midi_note,
    float velocity,
    midi_chord_mode_emit_callback emit,
    void *user_data)
{
    midi_chord_mode_note_set notes;
    size_t index;

    if (mode == 0 || !valid_midi_note(midi_note)) {
        return;
    }

    if (mode->held_notes[midi_note].held) {
        midi_chord_mode_note_off(mode, midi_note, emit, user_data);
    }

    midi_chord_mode_build_notes(mode, midi_note, &notes);
    mode->held_notes[midi_note].held = 1;
    mode->held_notes[midi_note].velocity = velocity;
    mode->held_notes[midi_note].notes = notes;

    for (index = 0; index < notes.count; ++index) {
        const int note = notes.notes[index];

        if (mode->sounding_counts[note] == 0) {
            emit_note(emit, user_data, MIDI_CHORD_MODE_EVENT_NOTE_ON, note, velocity);
        }

        mode->sounding_counts[note] += 1;
    }
}

// releases a keyboard note through chord mode.
void midi_chord_mode_note_off(
    midi_chord_mode *mode,
    int midi_note,
    midi_chord_mode_emit_callback emit,
    void *user_data)
{
    midi_chord_mode_held_note *held_note;
    size_t index;

    if (mode == 0 || !valid_midi_note(midi_note)) {
        return;
    }

    held_note = &mode->held_notes[midi_note];
    if (!held_note->held) {
        return;
    }

    for (index = 0; index < held_note->notes.count; ++index) {
        const int note = held_note->notes.notes[index];

        if (mode->sounding_counts[note] > 0) {
            mode->sounding_counts[note] -= 1;
            if (mode->sounding_counts[note] == 0) {
                emit_note(emit, user_data, MIDI_CHORD_MODE_EVENT_NOTE_OFF, note, 0.0f);
            }
        }
    }

    memset(held_note, 0, sizeof(*held_note));
}

// releases every note currently owned by chord mode.
void midi_chord_mode_all_notes_off(
    midi_chord_mode *mode,
    midi_chord_mode_emit_callback emit,
    void *user_data)
{
    int note;

    if (mode == 0) {
        return;
    }

    for (note = 0; note < MIDI_CHORD_MODE_NOTE_COUNT; ++note) {
        if (mode->sounding_counts[note] > 0) {
            emit_note(emit, user_data, MIDI_CHORD_MODE_EVENT_NOTE_OFF, note, 0.0f);
        }
    }

    memset(mode->held_notes, 0, sizeof(mode->held_notes));
    memset(mode->sounding_counts, 0, sizeof(mode->sounding_counts));
}
