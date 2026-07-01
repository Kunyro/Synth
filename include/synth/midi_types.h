#ifndef SYNTH_MIDI_TYPES_H
#define SYNTH_MIDI_TYPES_H

typedef enum synth_midi_message_type {
    SYNTH_MIDI_MESSAGE_NONE = 0,
    SYNTH_MIDI_MESSAGE_NOTE_ON,
    SYNTH_MIDI_MESSAGE_NOTE_OFF
} synth_midi_message_type;

typedef struct synth_midi_message {
    synth_midi_message_type type;
    int note;
    float velocity;
} synth_midi_message;

float synth_midi_note_to_frequency(int midi_note);
int synth_midi_parse_short_message(const unsigned char *data, unsigned short length, synth_midi_message *message);

#endif
