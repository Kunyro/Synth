#ifndef DESKTOP_MIDI_DEVICE_H
#define DESKTOP_MIDI_DEVICE_H

typedef void (*midi_note_on_fn)(void *user_data, int midi_note, float velocity);
typedef void (*midi_note_off_fn)(void *user_data, int midi_note);

typedef struct midi_device_callbacks {
    midi_note_on_fn note_on;
    midi_note_off_fn note_off;
    void *user_data;
} midi_device_callbacks;

#endif
