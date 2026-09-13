#include "midi/midi_types.h"

// parses a short midi packet into a synth midi message
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

    // the upper four bits select the message kind; the lower four select channel
    // we handle note-off (0x80), note-on (0x90), and pitch bend (0xe0)
    status = data[0] & 0xF0;
    if (status != 0x80 && status != 0x90 && status != 0xE0) {
        return 0;
    }

    // midi encodes channels as 0..15, while config files show them as 1..16
    message->channel = (data[0] & 0x0F) + 1;

    if (status == 0xE0) {
        // two seven-bit bytes form a 14-bit bend value subtract its center,
        // 8192, then scale each side separately because the endpoints are asymmetric:
        // -8192 below center and +8191 above it both become exact full-scale bends
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
        // turn the 0..127 velocity byte into the engine's normalized 0..1 level
        message->velocity = (float)velocity / 127.0f;
    } else {
        // midi treats note on with zero velocity as note off
        message->type = SYNTH_MIDI_MESSAGE_NOTE_OFF;
        message->velocity = 0.0f;
    }

    return 1;
}
