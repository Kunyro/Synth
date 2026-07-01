#ifndef DESKTOP_MIDI_PORTMIDI_H
#define DESKTOP_MIDI_PORTMIDI_H

#include "midi_device.h"

#define MIDI_PORTMIDI_MAX_STREAMS 16

typedef struct midi_portmidi_input {
    midi_device_callbacks callbacks;
    void *library;
    void *streams[MIDI_PORTMIDI_MAX_STREAMS];
    int stream_count;
    int source_count;
    int portmidi_initialized;
} midi_portmidi_input;

int midi_portmidi_init(midi_portmidi_input *input, midi_device_callbacks callbacks);
void midi_portmidi_poll(midi_portmidi_input *input);
void midi_portmidi_uninit(midi_portmidi_input *input);

#endif
