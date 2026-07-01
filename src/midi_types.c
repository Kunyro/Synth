#include "synth/midi_types.h"

#include <math.h>

float synth_midi_note_to_frequency(int midi_note)
{
    return 440.0f * powf(2.0f, ((float)midi_note - 69.0f) / 12.0f);
}

int synth_midi_parse_short_message(const unsigned char *data, unsigned short length, synth_midi_message *message)
{
    unsigned char status;
    int velocity;

    if (message == 0 || data == 0 || length < 3) {
        return 0;
    }

    message->type = SYNTH_MIDI_MESSAGE_NONE;
    message->note = 0;
    message->velocity = 0.0f;

    status = data[0] & 0xF0;
    if (status != 0x80 && status != 0x90) {
        return 0;
    }

    velocity = data[2];
    message->note = data[1];

    if (status == 0x90 && velocity > 0) {
        message->type = SYNTH_MIDI_MESSAGE_NOTE_ON;
        message->velocity = (float)velocity / 127.0f;
    } else {
        message->type = SYNTH_MIDI_MESSAGE_NOTE_OFF;
        message->velocity = 0.0f;
    }

    return 1;
}
