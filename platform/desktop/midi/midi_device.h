#ifndef DESKTOP_MIDI_DEVICE_H
#define DESKTOP_MIDI_DEVICE_H

// a callback for incoming note on messages.
typedef void (*midi_note_on_fn)(void *user_data, int midi_note, float velocity);
// a callback for incoming note off messages.
typedef void (*midi_note_off_fn)(void *user_data, int midi_note);

// the callbacks a midi input can call into.
typedef struct midi_device_callbacks {
    midi_note_on_fn note_on;
    midi_note_off_fn note_off;
    void *user_data;
} midi_device_callbacks;

#endif
