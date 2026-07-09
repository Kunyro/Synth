#include "synth/midi_types.h"

#include <math.h>

// converts a midi note number into hz.
float synth_midi_note_to_frequency(int midi_note)
{
    return 440.0f * powf(2.0f, ((float)midi_note - 69.0f) / 12.0f);
}

// parses a short midi packet into a synth midi message.
int synth_midi_parse_short_message(const unsigned char *data, unsigned short length, synth_midi_message *message)
{
    unsigned char status;
    int value_14_bit;
    int velocity;

    if (message == 0 || data == 0 || length < 3) {
        return 0;
    }

    message->type = SYNTH_MIDI_MESSAGE_NONE;
    message->channel = 0;
    message->note = 0;
    message->velocity = 0.0f;
    message->pitch_bend_value = 0;
    message->pitch_bend = 0.0f;

    status = data[0] & 0xF0;
    if (status != 0x80 && status != 0x90 && status != 0xE0) {
        return 0;
    }

    message->channel = (data[0] & 0x0F) + 1;

    if (status == 0xE0) {
        value_14_bit = ((int)data[2] << 7) | (int)data[1];
        message->type = SYNTH_MIDI_MESSAGE_PITCH_BEND;
        message->pitch_bend_value = value_14_bit - 8192;
        message->pitch_bend = message->pitch_bend_value < 0
            ? (float)message->pitch_bend_value / 8192.0f
            : (float)message->pitch_bend_value / 8191.0f;
        return 1;
    }

    velocity = data[2];
    message->note = data[1];

    if (status == 0x90 && velocity > 0) {
        message->type = SYNTH_MIDI_MESSAGE_NOTE_ON;
        message->velocity = (float)velocity / 127.0f;
    } else {
        // midi treats note on with zero velocity as note off.
        message->type = SYNTH_MIDI_MESSAGE_NOTE_OFF;
        message->velocity = 0.0f;
    }

    return 1;
}
