#ifndef DESKTOP_MIDI_PORTMIDI_H
#define DESKTOP_MIDI_PORTMIDI_H

#include "midi_device.h"

// the most midi input streams to open at once.
#define MIDI_PORTMIDI_MAX_STREAMS 16
// portmidi could not be loaded or did not have the needed symbols.
#define MIDI_PORTMIDI_INIT_UNAVAILABLE -1
// portmidi loaded but failed to initialize.
#define MIDI_PORTMIDI_INIT_FAILED -2

// a portmidi input wrapper with opened streams and callbacks.
typedef struct midi_portmidi_input {
    midi_device_callbacks callbacks;
    void *library;
    void *streams[MIDI_PORTMIDI_MAX_STREAMS];
    int device_count;
    int stream_count;
    int source_count;
    int portmidi_initialized;
} midi_portmidi_input;

// loads portmidi and opens available midi input streams.
int midi_portmidi_init(midi_portmidi_input *input, midi_device_callbacks callbacks);
// reads pending midi events and sends callbacks.
void midi_portmidi_poll(midi_portmidi_input *input);
// closes midi streams and unloads portmidi.
void midi_portmidi_uninit(midi_portmidi_input *input);

#endif
