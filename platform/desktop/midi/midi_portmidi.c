#include "midi/midi_portmidi.h"

#include "synth/midi_types.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// a portmidi stream handle from the loaded library.
typedef void PortMidiStream;
// a portmidi error code from the loaded library.
typedef int PmError;
// a portmidi device id from the loaded library.
typedef int PmDeviceID;
// a packed short midi message from the loaded library.
typedef int32_t PmMessage;
// a portmidi timestamp from the loaded library.
typedef int32_t PmTimestamp;

// one raw event read from a portmidi stream.
typedef struct PmEvent {
    PmMessage message;
    PmTimestamp timestamp;
} PmEvent;

// basic info about a portmidi device.
typedef struct PmDeviceInfo {
    int structVersion;
    const char *interf;
    const char *name;
    int input;
    int output;
    int opened;
} PmDeviceInfo;

// the portmidi functions loaded at runtime.
typedef struct portmidi_api {
    PmError (*Initialize)(void);
    PmError (*Terminate)(void);
    int (*CountDevices)(void);
    const PmDeviceInfo *(*GetDeviceInfo)(PmDeviceID id);
    PmError (*OpenInput)(
        PortMidiStream **stream,
        PmDeviceID input_device,
        void *input_driver_info,
        int32_t buffer_size,
        void *time_proc,
        void *time_info);
    PmError (*Close)(PortMidiStream *stream);
    PmError (*Poll)(PortMidiStream *stream);
    int (*Read)(PortMidiStream *stream, PmEvent *buffer, int32_t length);
} portmidi_api;

static portmidi_api g_portmidi;

// parses raw midi bytes and calls note callbacks.
static void dispatch_midi_bytes(const midi_device_callbacks *callbacks, const unsigned char *data)
{
    synth_midi_message message;

    if (callbacks->short_message != 0) {
        callbacks->short_message(callbacks->user_data, data, 3);
    }

    if (!synth_midi_parse_short_message(data, 3, &message)) {
        return;
    }

    if (message.type == SYNTH_MIDI_MESSAGE_NOTE_ON) {
        if (callbacks->note_on != 0) {
            callbacks->note_on(callbacks->user_data, message.note, message.velocity);
        }
    } else if (message.type == SYNTH_MIDI_MESSAGE_NOTE_OFF && callbacks->note_off != 0) {
        callbacks->note_off(callbacks->user_data, message.note);
    }
}

// unpacks a portmidi message and dispatches its bytes.
static void dispatch_portmidi_message(const midi_device_callbacks *callbacks, PmMessage message)
{
    unsigned char data[3];

    data[0] = (unsigned char)(message & 0xFF);
    data[1] = (unsigned char)((message >> 8) & 0xFF);
    data[2] = (unsigned char)((message >> 16) & 0xFF);
    dispatch_midi_bytes(callbacks, data);
}

#if defined(_WIN32)
// loads a symbol from a windows library handle.
static void *load_symbol(void *library, const char *name)
{
    return (void *)GetProcAddress((HMODULE)library, name);
}

// opens the portmidi library on windows.
static void *open_portmidi_library(void)
{
    return (void *)LoadLibraryA("portmidi.dll");
}

// closes a windows library handle.
static void close_portmidi_library(void *library)
{
    if (library != 0) {
        FreeLibrary((HMODULE)library);
    }
}
#else
// loads a symbol from a posix library handle.
static void *load_symbol(void *library, const char *name)
{
    return dlsym(library, name);
}

// tries to open one shared library path.
static void *try_dlopen(const char *path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

// opens portmidi from a few common library paths.
static void *open_portmidi_library(void)
{
#if defined(__APPLE__)
    static const char *paths[] = {
        "libportmidi.dylib",
        "/opt/homebrew/lib/libportmidi.dylib",
        "/usr/local/lib/libportmidi.dylib"
    };
#else
    static const char *paths[] = {
        "libportmidi.so",
        "libportmidi.so.0",
        "/usr/lib/libportmidi.so",
        "/usr/local/lib/libportmidi.so"
    };
#endif

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        void *library = try_dlopen(paths[i]);
        if (library != 0) {
            return library;
        }
    }

    return 0;
}

// closes a posix library handle.
static void close_portmidi_library(void *library)
{
    if (library != 0) {
        dlclose(library);
    }
}
#endif

// fills the portmidi api table from the loaded library.
static int load_portmidi_api(void *library)
{
    memset(&g_portmidi, 0, sizeof(g_portmidi));

    g_portmidi.Initialize = (PmError (*)(void))load_symbol(library, "Pm_Initialize");
    g_portmidi.Terminate = (PmError (*)(void))load_symbol(library, "Pm_Terminate");
    g_portmidi.CountDevices = (int (*)(void))load_symbol(library, "Pm_CountDevices");
    g_portmidi.GetDeviceInfo = (const PmDeviceInfo *(*)(PmDeviceID))load_symbol(library, "Pm_GetDeviceInfo");
    g_portmidi.OpenInput = (PmError (*)(
        PortMidiStream **,
        PmDeviceID,
        void *,
        int32_t,
        void *,
        void *))load_symbol(library, "Pm_OpenInput");
    g_portmidi.Close = (PmError (*)(PortMidiStream *))load_symbol(library, "Pm_Close");
    g_portmidi.Poll = (PmError (*)(PortMidiStream *))load_symbol(library, "Pm_Poll");
    g_portmidi.Read = (int (*)(PortMidiStream *, PmEvent *, int32_t))load_symbol(library, "Pm_Read");

    return g_portmidi.Initialize != 0 &&
           g_portmidi.Terminate != 0 &&
           g_portmidi.CountDevices != 0 &&
           g_portmidi.GetDeviceInfo != 0 &&
           g_portmidi.OpenInput != 0 &&
           g_portmidi.Close != 0 &&
           g_portmidi.Poll != 0 &&
           g_portmidi.Read != 0;
}

// loads portmidi and opens available midi input streams.
int midi_portmidi_init(midi_portmidi_input *input, midi_device_callbacks callbacks)
{
    int device_count;

    memset(input, 0, sizeof(*input));
    input->callbacks = callbacks;
    input->library = open_portmidi_library();

    if (input->library == 0 || !load_portmidi_api(input->library)) {
        midi_portmidi_uninit(input);
        return MIDI_PORTMIDI_INIT_UNAVAILABLE;
    }

    if (g_portmidi.Initialize() != 0) {
        midi_portmidi_uninit(input);
        return MIDI_PORTMIDI_INIT_FAILED;
    }

    input->portmidi_initialized = 1;
    device_count = g_portmidi.CountDevices();

    for (PmDeviceID id = 0; id < device_count && input->stream_count < MIDI_PORTMIDI_MAX_STREAMS; ++id) {
        const PmDeviceInfo *info = g_portmidi.GetDeviceInfo(id);
        PortMidiStream *stream = 0;

        if (info == 0 || !info->input) {
            continue;
        }

        input->source_count += 1;
        if (g_portmidi.OpenInput(&stream, id, 0, 256, 0, 0) == 0 && stream != 0) {
            input->streams[input->stream_count] = stream;
            input->stream_count += 1;
            fprintf(stdout, "Opened MIDI input: %s\n", info->name != 0 ? info->name : "(unnamed)");
        }
    }

    return input->stream_count;
}

// reads pending midi events and sends callbacks.
void midi_portmidi_poll(midi_portmidi_input *input)
{
    PmEvent events[32];

    if (input == 0 || input->stream_count == 0) {
        return;
    }

    for (int i = 0; i < input->stream_count; ++i) {
        PortMidiStream *stream = (PortMidiStream *)input->streams[i];
        PmError poll_result = g_portmidi.Poll(stream);

        while (poll_result > 0) {
            int read_count = g_portmidi.Read(stream, events, 32);

            if (read_count <= 0) {
                break;
            }

            for (int event = 0; event < read_count; ++event) {
                dispatch_portmidi_message(&input->callbacks, events[event].message);
            }

            poll_result = g_portmidi.Poll(stream);
        }
    }
}

// closes midi streams and unloads portmidi.
void midi_portmidi_uninit(midi_portmidi_input *input)
{
    if (input == 0) {
        return;
    }

    for (int i = 0; i < input->stream_count; ++i) {
        if (input->streams[i] != 0 && g_portmidi.Close != 0) {
            g_portmidi.Close((PortMidiStream *)input->streams[i]);
        }
    }

    if (input->portmidi_initialized && g_portmidi.Terminate != 0) {
        g_portmidi.Terminate();
    }

    close_portmidi_library(input->library);
    memset(input, 0, sizeof(*input));
}
