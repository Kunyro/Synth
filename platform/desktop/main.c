#include "midi/midi_types.h"
#include "synth/pitch.h"
#include "audio/audio_miniaudio.h"
#include "midi/chord_mode.h"
#include "midi/midi_mapping.h"
#include "midi/midi_portmidi.h"
#include "system/desktop_system.h"
#include "synth/synth.h"
#include "synth/synth_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the desktop app asks for stereo output
#define DESKTOP_CHANNEL_COUNT 2
// the most synth frames rendered at once in the audio callback
#define DESKTOP_RENDER_CHUNK_FRAMES 1024
// the controller mapping loaded unless another config is requested
#define DESKTOP_DEFAULT_MIDI_CONFIG "config/midi/akai_mpk_mini_mk2.conf"

// the desktop app state shared by audio and midi callbacks
typedef struct desktop_synth_app {
    synth synth;
    audio_miniaudio_device audio;
    midi_chord_mode chord_mode;
    midi_mapping midi_mapping;
    int midi_mapping_enabled;
    float left_buffer[DESKTOP_RENDER_CHUNK_FRAMES];
    float right_buffer[DESKTOP_RENDER_CHUNK_FRAMES];
} desktop_synth_app;

// turns a text waveform name into a synth waveform
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

// renders audio chunks from the synth into the device buffer
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

        // the audio thread and midi callbacks both touch synth state
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

// sends one chord-mode event into the synth while the audio lock is held
static void emit_chord_note_event(void *user_data, const midi_chord_mode_note_event *event)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;

    if (event->type == MIDI_CHORD_MODE_EVENT_NOTE_ON) {
        synth_note_on(&app->synth, event->note, event->velocity);
    } else {
        synth_note_off(&app->synth, event->note);
    }
}

// handles incoming midi note on messages
static void on_midi_note_on(void *user_data, int midi_note, float velocity)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;

    audio_miniaudio_lock(&app->audio);
    midi_chord_mode_note_on(&app->chord_mode, midi_note, velocity, emit_chord_note_event, app);
    audio_miniaudio_unlock(&app->audio);
}

// handles incoming midi note off messages
static void on_midi_note_off(void *user_data, int midi_note)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;

    audio_miniaudio_lock(&app->audio);
    midi_chord_mode_note_off(&app->chord_mode, midi_note, emit_chord_note_event, app);
    audio_miniaudio_unlock(&app->audio);
}

// handles incoming midi pitch bend messages
static void on_midi_pitch_bend(void *user_data, float pitch_bend)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;

    audio_miniaudio_lock(&app->audio);
    synth_set_pitch_bend(&app->synth, pitch_bend);
    audio_miniaudio_unlock(&app->audio);
}

// applies parsed midi messages to the synth
static void apply_midi_message(void *user_data, const synth_midi_message *message)
{
    switch (message->type) {
        case SYNTH_MIDI_MESSAGE_NOTE_ON:
            on_midi_note_on(user_data, message->note, message->velocity);
            break;

        case SYNTH_MIDI_MESSAGE_NOTE_OFF:
            on_midi_note_off(user_data, message->note);
            break;

        case SYNTH_MIDI_MESSAGE_PITCH_BEND:
            on_midi_pitch_bend(user_data, message->pitch_bend);
            break;

        case SYNTH_MIDI_MESSAGE_NONE:
        default:
            break;
    }
}

// handles raw midi messages by parsing notes/bends and applying mapped controls
static void on_midi_short_message(void *user_data, const unsigned char *data, unsigned short length)
{
    desktop_synth_app *app = (desktop_synth_app *)user_data;
    synth_midi_message message;
    midi_mapping_apply_result result;
    int applied;

    audio_miniaudio_lock(&app->audio);
    if (midi_chord_mode_handle_short_message(&app->chord_mode, data, length, emit_chord_note_event, app)) {
        audio_miniaudio_unlock(&app->audio);
        return;
    }
    audio_miniaudio_unlock(&app->audio);

    if (synth_midi_parse_short_message(data, length, &message)) {
        apply_midi_message(user_data, &message);
    }

    if (!app->midi_mapping_enabled) {
        return;
    }

    audio_miniaudio_lock(&app->audio);
    applied = midi_mapping_apply_short_message(&app->midi_mapping, data, length, &app->synth, &result);
    audio_miniaudio_unlock(&app->audio);

    if (applied) {
        if (result.kind == MIDI_MAPPING_APPLY_EFFECT_SELECT) {
            if (result.has_effect) {
                printf(
                    "Mapped MIDI: effect_selector_%zu=%s from channel=%d cc=%d value=%d\n",
                    result.effect_bank_index + 1,
                    midi_mapping_effect_name(result.effect),
                    result.channel,
                    result.control,
                    result.midi_value);
            } else {
                printf(
                    "Mapped MIDI: effect_selector_%zu=blank from channel=%d cc=%d value=%d\n",
                    result.effect_bank_index + 1,
                    result.channel,
                    result.control,
                    result.midi_value);
            }
        } else {
            printf(
                "Mapped MIDI: %s%s=%.3f from channel=%d cc=%d value=%d\n",
                result.target_kind == MIDI_MAPPING_TARGET_LFO_AMOUNT ? "lfo_amount." : "",
                midi_mapping_parameter_name(result.parameter),
                result.synth_value,
                result.channel,
                result.control,
                result.midi_value);
        }
        fflush(stdout);
    }
}

// polls midi while waiting for a fixed number of seconds
static void run_for_seconds(midi_portmidi_input *midi, double seconds)
{
    unsigned int remaining_ms = (unsigned int)(seconds * 1000.0);

    while (remaining_ms > 0) {
        const unsigned int sleep_ms = remaining_ms > 5 ? 5 : remaining_ms;

        midi_portmidi_poll(midi);
        desktop_sleep_ms(sleep_ms);
        remaining_ms -= sleep_ms;
    }
}

// polls midi until the user presses enter
static void run_until_enter(midi_portmidi_input *midi)
{
    while (!desktop_stdin_line_ready()) {
        midi_portmidi_poll(midi);
        desktop_sleep_ms(5);
    }

    {
        char line[8];
        (void)desktop_read_stdin_line(line, sizeof(line));
    }
}

// starts the desktop synth app
int main(int argc, char **argv)
{
    desktop_synth_app app;
    midi_portmidi_input midi;
    midi_device_callbacks midi_callbacks;
    int midi_stream_count;
    double seconds = 0.0;
    int midi_note = 57;
    int should_play_note = 0;
    int use_frequency = 0;
    float frequency = 220.0f;
    const char *midi_config_path = DESKTOP_DEFAULT_MIDI_CONFIG;
    const char *portmidi_path = 0;
    int should_load_midi_config = 1;
    int arg_index = 1;
    char midi_mapping_error[MIDI_MAPPING_ERROR_LENGTH];

    while (arg_index < argc) {
        if (strcmp(argv[arg_index], "--portmidi-path") == 0) {
            if (arg_index + 1 >= argc) {
                fprintf(stderr, "--portmidi-path requires a library path.\n");
                return 1;
            }
            portmidi_path = argv[arg_index + 1];
            arg_index += 2;
        } else if (strncmp(argv[arg_index], "--portmidi-path=", 16) == 0) {
            portmidi_path = argv[arg_index] + 16;
            if (portmidi_path[0] == '\0') {
                fprintf(stderr, "--portmidi-path requires a library path.\n");
                return 1;
            }
            arg_index += 1;
        } else if (strcmp(argv[arg_index], "--midi-config") == 0 && arg_index + 1 < argc) {
            midi_config_path = argv[arg_index + 1];
            should_load_midi_config = 1;
            arg_index += 2;
        } else if (strncmp(argv[arg_index], "--midi-config=", 14) == 0) {
            midi_config_path = argv[arg_index] + 14;
            should_load_midi_config = 1;
            arg_index += 1;
        } else if (strcmp(argv[arg_index], "--no-midi-config") == 0) {
            should_load_midi_config = 0;
            arg_index += 1;
        } else {
            break;
        }
    }

    synth_init(&app.synth, SYNTH_DEFAULT_SAMPLE_RATE);
    midi_chord_mode_init(&app.chord_mode);
    midi_mapping_init(&app.midi_mapping);
    app.midi_mapping_enabled = 0;

    if (should_load_midi_config) {
        if (midi_mapping_load(&app.midi_mapping, midi_config_path, midi_mapping_error, sizeof(midi_mapping_error))) {
            app.midi_mapping_enabled = 1;
            midi_mapping_configure_chord_mode(&app.midi_mapping, &app.chord_mode);
        } else {
            fprintf(stderr, "Could not load MIDI config '%s': %s\n", midi_config_path, midi_mapping_error);
        }
    }

    midi_callbacks.short_message = on_midi_short_message;
    midi_callbacks.user_data = &app;
    midi_stream_count = midi_portmidi_init_with_path(&midi, midi_callbacks, portmidi_path);

    if (argc > arg_index) {
        if (strcmp(argv[arg_index], "silence") == 0) {
            should_play_note = 0;
        } else if (strncmp(argv[arg_index], "freq:", 5) == 0) {
            should_play_note = 1;
            use_frequency = 1;
            frequency = (float)strtod(argv[arg_index] + 5, 0);
        } else {
            should_play_note = 1;
            midi_note = atoi(argv[arg_index]);
        }
    } else if (midi_stream_count <= 0 && midi.source_count == 0) {
        // no controller is present, so a bare run plays a test note
        should_play_note = 1;
    }

    if (argc > arg_index + 1) {
        seconds = strtod(argv[arg_index + 1], 0);
    }

    if (argc > arg_index + 2) {
        synth_set_waveform(&app.synth, parse_waveform(argv[arg_index + 2]));
    }

    if (argc > arg_index + 3) {
        synth_set_filter_cutoff(&app.synth, (float)strtod(argv[arg_index + 3], 0));
    }

    if (argc > arg_index + 4) {
        synth_set_filter_poles(&app.synth, atoi(argv[arg_index + 4]));
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
        synth_uninit(&app.synth);
        midi_portmidi_uninit(&midi);
        return 1;
    }

    if (!audio_miniaudio_start(&app.audio)) {
        audio_miniaudio_uninit(&app.audio);
        synth_uninit(&app.synth);
        midi_portmidi_uninit(&midi);
        return 1;
    }

    if (should_play_note && use_frequency) {
        printf("Playing %.2f Hz as a wave. ", frequency);
    } else if (should_play_note) {
        printf("Playing MIDI note %d (%.2f Hz) as a wave. ", midi_note, synth_note_to_frequency(midi_note));
    } else {
        printf("Playing silence. ");
    }

    if (midi_stream_count > 0) {
        printf("PortMidi connected to %d input stream(s). ", midi_stream_count);
        if (app.midi_mapping_enabled) {
            printf("Loaded MIDI config '%s'. ", app.midi_mapping.name);
        }
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
            // release before the end so the envelope tail can be heard
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
    midi_chord_mode_all_notes_off(&app.chord_mode, emit_chord_note_event, &app);
    synth_all_notes_off(&app.synth);
    audio_miniaudio_unlock(&app.audio);
    audio_miniaudio_uninit(&app.audio);
    synth_uninit(&app.synth);
    midi_portmidi_uninit(&midi);
    return 0;
}
