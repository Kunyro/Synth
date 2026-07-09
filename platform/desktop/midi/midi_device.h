#ifndef DESKTOP_MIDI_DEVICE_H
#define DESKTOP_MIDI_DEVICE_H

// a callback for raw short midi messages.
typedef void (*midi_short_message_fn)(void *user_data, const unsigned char *data, unsigned short length);

// the callbacks a midi input can call into.
typedef struct midi_device_callbacks {
    midi_short_message_fn short_message;
    void *user_data;
} midi_device_callbacks;

#endif
