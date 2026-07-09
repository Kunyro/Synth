#ifndef SYNTH_MIDI_TYPES_H
#define SYNTH_MIDI_TYPES_H

// the midi message kinds this synth understands.
typedef enum synth_midi_message_type {
    SYNTH_MIDI_MESSAGE_NONE = 0,
    SYNTH_MIDI_MESSAGE_NOTE_ON,
    SYNTH_MIDI_MESSAGE_NOTE_OFF,
    SYNTH_MIDI_MESSAGE_PITCH_BEND
} synth_midi_message_type;

// a small parsed midi channel message.
typedef struct synth_midi_message {
    synth_midi_message_type type;
    int channel;
    int note;
    float velocity;
    int pitch_bend_value;
    float pitch_bend;
} synth_midi_message;

// converts a midi note number into hz.
float synth_midi_note_to_frequency(int midi_note);
// parses a short midi packet into a synth midi message.
int synth_midi_parse_short_message(const unsigned char *data, unsigned short length, synth_midi_message *message);

#endif
