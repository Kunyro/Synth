#include "midi/midi_portmidi.h"

#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

// checks whether stdin has a line ready.
static int stdin_has_line(void)
{
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return select(STDIN_FILENO + 1, &read_fds, 0, 0, &timeout) > 0;
}

// returns a friendly name for a midi note number.
static const char *note_name(int note)
{
    static const char *names[] = {
        "c",
        "c#",
        "d",
        "d#",
        "e",
        "f",
        "f#",
        "g",
        "g#",
        "a",
        "a#",
        "b"
    };

    return names[note % 12];
}

// prints a midi note as name and octave.
static void print_note(int note)
{
    printf("%s%d", note_name(note), (note / 12) - 1);
}

// prints one raw short midi message in a useful form.
static void print_short_message(void *user_data, const unsigned char *data, unsigned short length)
{
    const unsigned char status = data[0] & 0xF0;
    const int channel = (data[0] & 0x0F) + 1;

    (void)user_data;

    if (data[0] >= 0xF0) {
        switch (data[0]) {
            case 0xF1:
                printf("midi time code quarter frame: value=%u raw=%02x %02x\n", data[1], data[0], data[1]);
                break;

            case 0xF2:
                printf(
                    "song position pointer: value=%u raw=%02x %02x %02x\n",
                    ((unsigned int)data[2] << 7) | data[1],
                    data[0],
                    data[1],
                    data[2]);
                break;

            case 0xF3:
                printf("song select: song=%u raw=%02x %02x\n", data[1], data[0], data[1]);
                break;

            case 0xF6:
                printf("tune request: raw=%02x\n", data[0]);
                break;

            case 0xF8:
                printf("timing clock: raw=%02x\n", data[0]);
                break;

            case 0xFA:
                printf("start: raw=%02x\n", data[0]);
                break;

            case 0xFB:
                printf("continue: raw=%02x\n", data[0]);
                break;

            case 0xFC:
                printf("stop: raw=%02x\n", data[0]);
                break;

            case 0xFE:
                printf("active sensing: raw=%02x\n", data[0]);
                break;

            case 0xFF:
                printf("system reset: raw=%02x\n", data[0]);
                break;

            default:
                printf("system message: length=%u raw=%02x %02x %02x\n", length, data[0], data[1], data[2]);
                break;
        }

        fflush(stdout);
        return;
    }

    if (length < 2) {
        printf("short message: length=%u raw=%02x\n", length, data[0]);
        fflush(stdout);
        return;
    }

    switch (status) {
        case 0x80:
            printf("note off: channel=%d note=%u name=", channel, data[1]);
            print_note(data[1]);
            printf(" velocity=%u raw=%02x %02x %02x\n", data[2], data[0], data[1], data[2]);
            break;

        case 0x90:
            if (data[2] == 0) {
                printf("note off: channel=%d note=%u name=", channel, data[1]);
                print_note(data[1]);
                printf(" velocity=0 raw=%02x %02x %02x\n", data[0], data[1], data[2]);
            } else {
                printf("note on: channel=%d note=%u name=", channel, data[1]);
                print_note(data[1]);
                printf(" velocity=%u raw=%02x %02x %02x\n", data[2], data[0], data[1], data[2]);
            }
            break;

        case 0xA0:
            printf(
                "poly pressure: channel=%d note=%u pressure=%u raw=%02x %02x %02x\n",
                channel,
                data[1],
                data[2],
                data[0],
                data[1],
                data[2]);
            break;

        case 0xB0:
            printf(
                "control change: channel=%d cc=%u value=%u raw=%02x %02x %02x\n",
                channel,
                data[1],
                data[2],
                data[0],
                data[1],
                data[2]);
            break;

        case 0xC0:
            printf(
                "program change: channel=%d program=%u raw=%02x %02x\n",
                channel,
                data[1],
                data[0],
                data[1]);
            break;

        case 0xD0:
            printf(
                "channel pressure: channel=%d pressure=%u raw=%02x %02x\n",
                channel,
                data[1],
                data[0],
                data[1]);
            break;

        case 0xE0:
            printf(
                "pitch bend: channel=%d value=%d raw=%02x %02x %02x\n",
                channel,
                (((int)data[2] << 7) | (int)data[1]) - 8192,
                data[0],
                data[1],
                data[2]);
            break;

        default:
            printf("unknown message: status=%02x raw=%02x %02x %02x\n", status, data[0], data[1], data[2]);
            break;
    }

    fflush(stdout);
}

// prints why midi monitoring could not start.
static void print_midi_status(int stream_count, const midi_portmidi_input *midi)
{
    if (stream_count > 0) {
        printf("listening to %d midi input stream(s). press enter to stop.\n", stream_count);
    } else if (stream_count == MIDI_PORTMIDI_INIT_UNAVAILABLE) {
        printf("portmidi library not found, so midi monitoring cannot start.\n");
    } else if (stream_count == MIDI_PORTMIDI_INIT_FAILED) {
        printf("portmidi loaded but failed to initialize, so midi monitoring cannot start.\n");
    } else if (midi->device_count == 0) {
        printf("portmidi loaded, but no midi devices were found.\n");
    } else if (midi->source_count == 0) {
        printf("portmidi found %d midi device(s), but none were input devices.\n", midi->device_count);
    } else {
        printf("portmidi found %d midi input source(s), but none opened.\n", midi->source_count);
    }
}

// starts the midi monitor utility.
int main(void)
{
    midi_portmidi_input midi;
    midi_device_callbacks callbacks;
    int stream_count;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.short_message = print_short_message;

    stream_count = midi_portmidi_init(&midi, callbacks);
    print_midi_status(stream_count, &midi);

    if (stream_count <= 0) {
        midi_portmidi_uninit(&midi);
        return stream_count < 0 ? 1 : 0;
    }

    while (!stdin_has_line()) {
        midi_portmidi_poll(&midi);
        usleep(5000);
    }

    (void)getchar();
    midi_portmidi_uninit(&midi);
    return 0;
}
