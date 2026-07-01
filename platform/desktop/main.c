#include "audio/audio_miniaudio.h"
#include "midi/midi_portmidi.h"
#include "synth/synth.h"
#include "synth/synth_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

// the desktop app asks for stereo output.
#define DESKTOP_CHANNEL_COUNT 2
// the most synth frames rendered at once in the audio callback.
#define DESKTOP_RENDER_CHUNK_FRAMES 1024

// the desktop app state shared by audio and midi callbacks.
typedef struct desktop_synth_app {
    synth synth;
    audio_miniaudio_device audio;
    float left_buffer[DESKTOP_RENDER_CHUNK_FRAMES];
    float right_buffer[DESKTOP_RENDER_CHUNK_FRAMES];
} desktop_synth_app;

// turns a text waveform name into a synth waveform.
static synth_waveform parse_waveform(const char *value)
{
    if (strcmp(value, "saw") == 0) {
        return SYNTH_WAVEFORM_SAW;
    }

    if (strcmp(value, "square") == 0) {
        return SYNTH_WAVEFORM_SQUARE;
    }

    return SYNTH_WAVEFORM_SINE;
}

// renders audio chunks from the synth into the device buffer.
static void render_audio(void *user_data, float *output, unsigned int frame_count, unsigned int channel_count)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;
    unsigned int frames_done = 0;

    while (frames_done < frame_count) {
        const unsigned int remaining = frame_count - frames_done;
        const unsigned int chunk_size = remaining > DESKTOP_RENDER_CHUNK_FRAMES ? DESKTOP_RENDER_CHUNK_FRAMES : remaining;
        synth_audio_buffer synth_buffer;

        synth_buffer.left = app->left_buffer;
        synth_buffer.right = app->right_buffer;
        synth_buffer.frame_count = chunk_size;

        audio_miniaudio_lock(&app->audio);
        synth_render_stereo(&app->synth, &synth_buffer);
        audio_miniaudio_unlock(&app->audio);

        for (unsigned int frame = 0; frame < chunk_size; ++frame) {
            const float left = app->left_buffer[frame];
            const float right = app->right_buffer[frame];
            float *frame_output = &output[(frames_done + frame) * channel_count];

            if (channel_count == 1) {
                frame_output[0] = (left + right) * 0.5f;
            } else {
                frame_output[0] = left;
                frame_output[1] = right;

                for (unsigned int channel = 2; channel < channel_count; ++channel) {
                    frame_output[channel] = 0.0f;
                }
            }
        }

        frames_done += chunk_size;
    }
}

// handles incoming midi note on messages.
static void on_midi_note_on(void *user_data, int midi_note, float velocity)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;

    audio_miniaudio_lock(&app->audio);
    synth_note_on(&app->synth, midi_note, velocity);
    audio_miniaudio_unlock(&app->audio);
}

// handles incoming midi note off messages.
static void on_midi_note_off(void *user_data, int midi_note)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;

    audio_miniaudio_lock(&app->audio);
    synth_note_off(&app->synth, midi_note);
    audio_miniaudio_unlock(&app->audio);
}

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

// polls midi while waiting for a fixed number of seconds.
static void run_for_seconds(midi_portmidi_input *midi, double seconds)
{
    unsigned int remaining_ms = (unsigned int)(seconds * 1000.0);

    while (remaining_ms > 0) {
        const unsigned int sleep_ms = remaining_ms > 5 ? 5 : remaining_ms;

        midi_portmidi_poll(midi);
        audio_miniaudio_sleep(sleep_ms);
        remaining_ms -= sleep_ms;
    }
}

// polls midi until the user presses enter.
static void run_until_enter(midi_portmidi_input *midi)
{
    while (!stdin_has_line()) {
        midi_portmidi_poll(midi);
        audio_miniaudio_sleep(5);
    }

    (void)getchar();
}

// starts the desktop synth app.
int main(int argc, char **argv)
{
    desktop_synth_app app;
    midi_portmidi_input midi;
    midi_device_callbacks midi_callbacks;
    int midi_stream_count;
    double seconds = 0.0;
    int midi_note = 57;
    int should_play_note = 1;
    int use_frequency = 0;
    float frequency = 220.0f;

    synth_init(&app.synth, SYNTH_DEFAULT_SAMPLE_RATE);

    midi_callbacks.note_on = on_midi_note_on;
    midi_callbacks.note_off = on_midi_note_off;
    midi_callbacks.user_data = &app;
    midi_stream_count = midi_portmidi_init(&midi, midi_callbacks);

    if (argc > 1) {
        if (strcmp(argv[1], "silence") == 0) {
            should_play_note = 0;
        } else if (strncmp(argv[1], "freq:", 5) == 0) {
            use_frequency = 1;
            frequency = (float)strtod(argv[1] + 5, 0);
        } else {
            midi_note = atoi(argv[1]);
        }
    }

    if (argc > 2) {
        seconds = strtod(argv[2], 0);
    }

    if (argc > 3) {
        synth_set_waveform(&app.synth, parse_waveform(argv[3]));
    }

    if (argc > 4) {
        synth_set_filter_cutoff(&app.synth, (float)strtod(argv[4], 0));
    }

    if (argc > 5) {
        synth_set_filter_poles(&app.synth, atoi(argv[5]));
    }

    if (should_play_note) {
        if (use_frequency) {
            synth_note_on_frequency(&app.synth, frequency, 0.9f);
        } else {
            synth_note_on(&app.synth, midi_note, 0.9f);
        }
    }

    if (!audio_miniaudio_init(
            &app.audio,
            (unsigned int)SYNTH_DEFAULT_SAMPLE_RATE,
            DESKTOP_CHANNEL_COUNT,
            render_audio,
            &app)) {
        midi_portmidi_uninit(&midi);
        return 1;
    }

    if (!audio_miniaudio_start(&app.audio)) {
        audio_miniaudio_uninit(&app.audio);
        midi_portmidi_uninit(&midi);
        return 1;
    }

    if (should_play_note && use_frequency) {
        printf("Playing %.2f Hz as a wave. ", frequency);
    } else if (should_play_note) {
        printf("Playing MIDI note %d (%.2f Hz) as a wave. ", midi_note, synth_midi_note_to_frequency(midi_note));
    } else {
        printf("Playing silence. ");
    }

    if (midi_stream_count > 0) {
        printf("PortMidi connected to %d input stream(s). ", midi_stream_count);
    } else if (midi_stream_count == MIDI_PORTMIDI_INIT_UNAVAILABLE) {
        printf("PortMidi library not found, so MIDI input is disabled. ");
    } else if (midi_stream_count == MIDI_PORTMIDI_INIT_FAILED) {
        printf("PortMidi loaded but failed to initialize, so MIDI input is disabled. ");
    } else if (midi.source_count == 0) {
        printf("PortMidi loaded, but no MIDI input devices were found. ");
    } else {
        printf("PortMidi found %d MIDI input source(s), but none opened. ", midi.source_count);
    }

    if (seconds > 0.0) {
        printf("Stopping after %.2f seconds.\n", seconds);
        if (should_play_note && seconds > 0.25) {
            run_for_seconds(&midi, seconds * 0.8);
            audio_miniaudio_lock(&app.audio);
            if (use_frequency) {
                synth_all_notes_off(&app.synth);
            } else {
                synth_note_off(&app.synth, midi_note);
            }
            audio_miniaudio_unlock(&app.audio);
            run_for_seconds(&midi, seconds * 0.2);
        } else {
            run_for_seconds(&midi, seconds);
        }
    } else {
        printf("Press Enter to stop.\n");
        run_until_enter(&midi);
    }

    audio_miniaudio_lock(&app.audio);
    synth_all_notes_off(&app.synth);
    audio_miniaudio_unlock(&app.audio);
    audio_miniaudio_uninit(&app.audio);
    midi_portmidi_uninit(&midi);
    return 0;
}
