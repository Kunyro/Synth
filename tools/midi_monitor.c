#include "midi/midi_mapping.h"
#include "midi/midi_portmidi.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define MIDI_MONITOR_LINE_LENGTH 256
#define MIDI_MONITOR_PATH_LENGTH 512
#define MIDI_LEARN_POLL_SLEEP_US 5000
#define MIDI_LEARN_DRAIN_QUIET_MS 80
#define MIDI_LEARN_DRAIN_MAX_MS 600

typedef struct learn_capture {
    int has_cc;
    int channel;
    int control;
    int value;
} learn_capture;

typedef struct learn_context {
    learn_capture capture;
    int capture_enabled;
    unsigned long ignored_message_count;
} learn_context;

typedef enum learn_capture_result {
    LEARN_CAPTURE_GOT_CC = 0,
    LEARN_CAPTURE_SKIPPED,
    LEARN_CAPTURE_CANCELLED,
    LEARN_CAPTURE_ENDED
} learn_capture_result;

typedef enum learn_bind_result {
    LEARN_BIND_UNCHANGED = 0,
    LEARN_BIND_CHANGED,
    LEARN_BIND_EXITED
} learn_bind_result;

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

// trims whitespace from both ends of a mutable string.
static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        ++text;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return text;
}

// reads one command line and trims it in place.
static int read_line(char *line, size_t line_size)
{
    char *trimmed;

    if (fgets(line, line_size, stdin) == 0) {
        return 0;
    }

    trimmed = trim(line);
    if (trimmed != line) {
        memmove(line, trimmed, strlen(trimmed) + 1);
    }

    return 1;
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

// captures the first control-change message while a bind prompt is waiting.
static void capture_cc_message(void *user_data, const unsigned char *data, unsigned short length)
{
    learn_context *context = (learn_context *)user_data;
    unsigned char status;

    if (length == 0) {
        return;
    }

    if (!context->capture_enabled) {
        if (data[0] != 0xFE) {
            context->ignored_message_count += 1;
        }
        return;
    }

    status = data[0] & 0xF0;

    if (length < 3 || context->capture.has_cc) {
        return;
    }

    if (status == 0xB0) {
        context->capture.has_cc = 1;
        context->capture.channel = (data[0] & 0x0F) + 1;
        context->capture.control = data[1];
        context->capture.value = data[2];
        printf(
            "heard: control change channel=%d cc=%d value=%d\n",
            context->capture.channel,
            context->capture.control,
            context->capture.value);
        fflush(stdout);
    } else if (data[0] != 0xFE) {
        printf("heard a non-cc message; move a knob or fader that sends control change.\n");
        fflush(stdout);
    }
}

// prints why midi monitoring could not start.
static void print_midi_status(int stream_count, const midi_portmidi_input *midi, const char *running_text)
{
    if (stream_count > 0) {
        printf("listening to %d midi input stream(s). %s\n", stream_count, running_text);
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

static void print_usage(void)
{
    printf("usage:\n");
    printf("  midi_monitor\n");
    printf("  midi_monitor monitor\n");
    printf("  midi_monitor learn --output <path> [--name <controller name>]\n");
    printf("  midi_monitor learn --edit <path> [--name <controller name>]\n");
    printf("  midi_monitor show --config <path>\n");
    printf("  midi_monitor validate --config <path>\n");
}

static int run_monitor(void)
{
    midi_portmidi_input midi;
    midi_device_callbacks callbacks;
    int stream_count;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.short_message = print_short_message;

    stream_count = midi_portmidi_init(&midi, callbacks);
    print_midi_status(stream_count, &midi, "press enter to stop.");

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

static void print_binding(const midi_mapping_binding *binding)
{
    printf(
        "cc:%d:%d:%s:%.6g:%.6g",
        binding->channel,
        binding->control,
        midi_mapping_scale_name(binding->scale),
        binding->min_value,
        binding->max_value);
}

static const midi_mapping_binding *find_first_binding(
    const midi_mapping *mapping,
    midi_mapping_parameter parameter)
{
    for (size_t i = 0; i < mapping->binding_count; ++i) {
        if (mapping->bindings[i].parameter == parameter) {
            return &mapping->bindings[i];
        }
    }

    return 0;
}

static int parse_parameter_selection(
    const char *text,
    const midi_mapping_parameter_info **info)
{
    char *end;
    long index;

    if (text == 0 || *text == '\0') {
        return 0;
    }

    index = strtol(text, &end, 10);
    if (*text != '\0' && *end == '\0') {
        if (index < 1 || (size_t)index > midi_mapping_parameter_count()) {
            return 0;
        }

        *info = midi_mapping_parameter_info_at((size_t)index - 1);
        return *info != 0;
    }

    *info = midi_mapping_parameter_info_by_name(text);
    return *info != 0;
}

static void print_parameter_list(const midi_mapping *mapping)
{
    printf("\n%s\n", mapping->name);
    printf("parameter bindings:\n");

    for (size_t i = 0; i < midi_mapping_parameter_count(); ++i) {
        const midi_mapping_parameter_info *info = midi_mapping_parameter_info_at(i);
        int printed_binding = 0;

        printf("%2zu  %-42s ", i + 1, info->name);

        for (size_t binding_index = 0; binding_index < mapping->binding_count; ++binding_index) {
            const midi_mapping_binding *binding = &mapping->bindings[binding_index];

            if (binding->parameter == info->parameter) {
                if (printed_binding) {
                    printf("\n    %-42s ", "");
                }

                print_binding(binding);
                printed_binding = 1;
            }
        }

        if (!printed_binding) {
            printf("unbound");
        }

        printf("\n");
    }
}

static void remove_bindings_for_parameter(midi_mapping *mapping, midi_mapping_parameter parameter)
{
    size_t write_index = 0;

    for (size_t read_index = 0; read_index < mapping->binding_count; ++read_index) {
        if (mapping->bindings[read_index].parameter != parameter) {
            mapping->bindings[write_index] = mapping->bindings[read_index];
            write_index += 1;
        }
    }

    mapping->binding_count = write_index;
}

static int control_is_already_bound(
    const midi_mapping *mapping,
    int channel,
    int control,
    midi_mapping_parameter ignored_parameter)
{
    for (size_t i = 0; i < mapping->binding_count; ++i) {
        const midi_mapping_binding *binding = &mapping->bindings[i];

        if (binding->source_type == MIDI_MAPPING_SOURCE_CC &&
            binding->channel == channel &&
            binding->control == control &&
            binding->parameter != ignored_parameter) {
            printf(
                "warning: cc:%d:%d is already bound to %s\n",
                channel,
                control,
                midi_mapping_parameter_name(binding->parameter));
            return 1;
        }
    }

    return 0;
}

static int parse_float_input(const char *text, float *value)
{
    char *end;
    const float parsed = (float)strtod(text, &end);

    if (*text == '\0' || *end != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

static learn_capture_result prompt_scale_and_range(midi_mapping_binding *binding)
{
    char line[MIDI_MONITOR_LINE_LENGTH];

    for (;;) {
        printf(
            "scale/range: %s %.6g..%.6g [enter=accept, edit, cancel] > ",
            midi_mapping_scale_name(binding->scale),
            binding->min_value,
            binding->max_value);
        fflush(stdout);

        if (!read_line(line, sizeof(line))) {
            return LEARN_CAPTURE_ENDED;
        }

        if (line[0] == '\0' || strcmp(line, "y") == 0 || strcmp(line, "yes") == 0) {
            return LEARN_CAPTURE_GOT_CC;
        }

        if (strcmp(line, "cancel") == 0 ||
            strcmp(line, "done") == 0 ||
            strcmp(line, "quit") == 0 ||
            strcmp(line, "exit") == 0 ||
            strcmp(line, "n") == 0 ||
            strcmp(line, "no") == 0) {
            return LEARN_CAPTURE_CANCELLED;
        }

        if (strcmp(line, "edit") != 0) {
            printf("enter, edit, or cancel.\n");
            continue;
        }

        printf("scale [linear/log/step] > ");
        fflush(stdout);
        if (!read_line(line, sizeof(line)) ||
            !midi_mapping_parse_scale_name(line, &binding->scale)) {
            printf("unknown scale.\n");
            continue;
        }

        printf("min > ");
        fflush(stdout);
        if (!read_line(line, sizeof(line)) || !parse_float_input(line, &binding->min_value)) {
            printf("min must be a number.\n");
            continue;
        }

        printf("max > ");
        fflush(stdout);
        if (!read_line(line, sizeof(line)) || !parse_float_input(line, &binding->max_value)) {
            printf("max must be a number.\n");
            continue;
        }

        if (binding->max_value <= binding->min_value) {
            printf("max must be greater than min.\n");
            continue;
        }

        if (binding->scale == MIDI_MAPPING_SCALE_LOG &&
            (binding->min_value <= 0.0f || binding->max_value <= 0.0f)) {
            printf("log scale needs min and max above zero.\n");
            continue;
        }
    }
}

// consumes queued midi after a bind so one knob twist cannot bind the next row too.
static void drain_midi_learn_input(midi_portmidi_input *midi, learn_context *context)
{
    unsigned int elapsed_ms = 0;
    unsigned int quiet_ms = 0;
    unsigned long seen_message_count;

    context->capture_enabled = 0;
    context->capture.has_cc = 0;
    seen_message_count = context->ignored_message_count;

    while (elapsed_ms < MIDI_LEARN_DRAIN_MAX_MS && quiet_ms < MIDI_LEARN_DRAIN_QUIET_MS) {
        midi_portmidi_poll(midi);

        if (context->ignored_message_count != seen_message_count) {
            seen_message_count = context->ignored_message_count;
            quiet_ms = 0;
        } else {
            quiet_ms += MIDI_LEARN_POLL_SLEEP_US / 1000;
        }

        usleep(MIDI_LEARN_POLL_SLEEP_US);
        elapsed_ms += MIDI_LEARN_POLL_SLEEP_US / 1000;
    }

    context->capture.has_cc = 0;
}

static learn_capture_result wait_for_control_change(
    midi_portmidi_input *midi,
    learn_context *context,
    int allow_skip)
{
    char line[MIDI_MONITOR_LINE_LENGTH];

    context->capture.has_cc = 0;
    context->capture_enabled = 1;
    if (allow_skip) {
        printf("move the knob or fader to bind. press enter to skip, or type done to leave map-all.\n");
    } else {
        printf("move the knob or fader to bind. type cancel and press enter to abort.\n");
    }
    fflush(stdout);

    while (!context->capture.has_cc) {
        midi_portmidi_poll(midi);

        if (context->capture.has_cc) {
            break;
        }

        if (stdin_has_line()) {
            if (!read_line(line, sizeof(line))) {
                context->capture_enabled = 0;
                return LEARN_CAPTURE_ENDED;
            }

            if (line[0] == '\0') {
                context->capture_enabled = 0;
                return allow_skip ? LEARN_CAPTURE_SKIPPED : LEARN_CAPTURE_CANCELLED;
            }

            if (strcmp(line, "cancel") == 0 ||
                strcmp(line, "done") == 0 ||
                strcmp(line, "quit") == 0 ||
                strcmp(line, "exit") == 0) {
                context->capture_enabled = 0;
                return LEARN_CAPTURE_CANCELLED;
            }

            if (allow_skip) {
                printf("waiting for midi control change; enter skips, done exits map-all.\n");
            } else {
                printf("waiting for midi control change; type cancel to abort.\n");
            }
        }

        usleep(MIDI_LEARN_POLL_SLEEP_US);
    }

    context->capture_enabled = 0;
    return LEARN_CAPTURE_GOT_CC;
}

static learn_bind_result store_captured_binding(
    midi_mapping *mapping,
    learn_context *context,
    const midi_mapping_parameter_info *info)
{
    midi_mapping_binding binding;
    const midi_mapping_binding *existing = find_first_binding(mapping, info->parameter);
    learn_capture_result prompt_result;

    memset(&binding, 0, sizeof(binding));
    binding.parameter = info->parameter;
    binding.source_type = MIDI_MAPPING_SOURCE_CC;
    binding.channel = context->capture.channel;
    binding.control = context->capture.control;

    if (existing != 0) {
        binding.scale = existing->scale;
        binding.min_value = existing->min_value;
        binding.max_value = existing->max_value;
    } else {
        binding.scale = info->default_scale;
        binding.min_value = info->default_min_value;
        binding.max_value = info->default_max_value;
    }

    (void)control_is_already_bound(mapping, binding.channel, binding.control, binding.parameter);

    prompt_result = prompt_scale_and_range(&binding);
    if (prompt_result == LEARN_CAPTURE_CANCELLED || prompt_result == LEARN_CAPTURE_ENDED) {
        printf("bind cancelled.\n");
        return prompt_result == LEARN_CAPTURE_CANCELLED ? LEARN_BIND_EXITED : LEARN_BIND_UNCHANGED;
    }

    remove_bindings_for_parameter(mapping, info->parameter);
    if (mapping->binding_count >= MIDI_MAPPING_MAX_BINDINGS) {
        printf("could not bind: too many midi bindings.\n");
        return LEARN_BIND_UNCHANGED;
    }

    mapping->bindings[mapping->binding_count] = binding;
    mapping->binding_count += 1;

    printf("%s=", info->name);
    print_binding(&binding);
    printf("\n");
    return LEARN_BIND_CHANGED;
}

static int bind_parameter(
    midi_mapping *mapping,
    midi_portmidi_input *midi,
    learn_context *context,
    const midi_mapping_parameter_info *info)
{
    learn_capture_result capture_result = wait_for_control_change(midi, context, 0);
    learn_bind_result bind_result;

    if (capture_result != LEARN_CAPTURE_GOT_CC) {
        printf("bind cancelled.\n");
        return 0;
    }

    bind_result = store_captured_binding(mapping, context, info);
    drain_midi_learn_input(midi, context);
    return bind_result == LEARN_BIND_CHANGED;
}

static int map_all_parameters(
    midi_mapping *mapping,
    midi_portmidi_input *midi,
    learn_context *context)
{
    int changed = 0;

    printf("map-all started. parameters are visited in the same order as list.\n");

    for (size_t i = 0; i < midi_mapping_parameter_count(); ++i) {
        const midi_mapping_parameter_info *info = midi_mapping_parameter_info_at(i);
        learn_capture_result capture_result;

        printf("\n%zu/%zu  %s\n", i + 1, midi_mapping_parameter_count(), info->name);
        capture_result = wait_for_control_change(midi, context, 1);

        switch (capture_result) {
            case LEARN_CAPTURE_GOT_CC:
            {
                const learn_bind_result bind_result = store_captured_binding(mapping, context, info);

                if (bind_result == LEARN_BIND_CHANGED) {
                    changed = 1;
                } else if (bind_result == LEARN_BIND_EXITED) {
                    printf("map-all stopped.\n");
                    return changed;
                }

                drain_midi_learn_input(midi, context);
                break;
            }

            case LEARN_CAPTURE_SKIPPED:
                printf("skipped %s\n", info->name);
                drain_midi_learn_input(midi, context);
                break;

            case LEARN_CAPTURE_CANCELLED:
                printf("map-all stopped.\n");
                return changed;

            case LEARN_CAPTURE_ENDED:
            default:
                printf("map-all ended.\n");
                return changed;
        }
    }

    printf("map-all complete.\n");
    return changed;
}

static int save_mapping_file(const char *path, const midi_mapping *mapping)
{
    FILE *file = fopen(path, "w");

    if (file == 0) {
        fprintf(stderr, "could not write '%s': %s\n", path, strerror(errno));
        return 0;
    }

    fprintf(file, "# midi controller mapping generated by midi_monitor learn\n");
    fprintf(file, "name=%s\n\n", mapping->name);
    fprintf(file, "# format: parameter=cc:channel:control:scale:min:max\n");

    for (size_t i = 0; i < mapping->binding_count; ++i) {
        const midi_mapping_binding *binding = &mapping->bindings[i];

        if (binding->source_type == MIDI_MAPPING_SOURCE_CC) {
            fprintf(
                file,
                "%s=cc:%d:%d:%s:%.6g:%.6g\n",
                midi_mapping_parameter_name(binding->parameter),
                binding->channel,
                binding->control,
                midi_mapping_scale_name(binding->scale),
                binding->min_value,
                binding->max_value);
        }
    }

    fclose(file);
    return 1;
}

static int copy_file(const char *source_path, const char *target_path)
{
    FILE *source = fopen(source_path, "rb");
    FILE *target;
    unsigned char buffer[4096];
    size_t bytes_read;

    if (source == 0) {
        return 0;
    }

    target = fopen(target_path, "wb");
    if (target == 0) {
        fclose(source);
        return 0;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target) != bytes_read) {
            fclose(source);
            fclose(target);
            return 0;
        }
    }

    fclose(source);
    fclose(target);
    return 1;
}

static int backup_mapping_file(const char *path)
{
    char backup_path[MIDI_MONITOR_PATH_LENGTH];

    snprintf(backup_path, sizeof(backup_path), "%s.bak", path);
    if (!copy_file(path, backup_path)) {
        fprintf(stderr, "could not write backup '%s'\n", backup_path);
        return 0;
    }

    printf("backup written: %s\n", backup_path);
    return 1;
}

static int parse_config_path(int argc, char **argv, const char **path)
{
    if (argc == 3) {
        *path = argv[2];
        return 1;
    }

    if (argc == 4 && strcmp(argv[2], "--config") == 0) {
        *path = argv[3];
        return 1;
    }

    return 0;
}

static int print_loaded_mapping(const char *path)
{
    midi_mapping mapping;
    char error[MIDI_MAPPING_ERROR_LENGTH];

    if (!midi_mapping_load(&mapping, path, error, sizeof(error))) {
        fprintf(stderr, "invalid midi config '%s': %s\n", path, error);
        return 1;
    }

    print_parameter_list(&mapping);
    return 0;
}

static int validate_mapping_file(const char *path)
{
    midi_mapping mapping;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    int warnings = 0;

    if (!midi_mapping_load(&mapping, path, error, sizeof(error))) {
        fprintf(stderr, "invalid midi config '%s': %s\n", path, error);
        return 1;
    }

    for (size_t i = 0; i < mapping.binding_count; ++i) {
        for (size_t j = i + 1; j < mapping.binding_count; ++j) {
            const midi_mapping_binding *a = &mapping.bindings[i];
            const midi_mapping_binding *b = &mapping.bindings[j];

            if (a->source_type == MIDI_MAPPING_SOURCE_CC &&
                b->source_type == MIDI_MAPPING_SOURCE_CC &&
                a->channel == b->channel &&
                a->control == b->control) {
                printf(
                    "warning: cc:%d:%d is bound to both %s and %s\n",
                    a->channel,
                    a->control,
                    midi_mapping_parameter_name(a->parameter),
                    midi_mapping_parameter_name(b->parameter));
                warnings += 1;
            }
        }
    }

    printf(
        "valid midi config: %s (%zu binding%s",
        path,
        mapping.binding_count,
        mapping.binding_count == 1 ? "" : "s");
    if (warnings > 0) {
        printf(", %d warning%s", warnings, warnings == 1 ? "" : "s");
    }
    printf(")\n");

    return 0;
}

static void print_learn_help(void)
{
    printf("commands:\n");
    printf("  list                 show parameters and current bindings\n");
    printf("  bind <name|number>   learn a cc binding for one parameter\n");
    printf("  map-all              walk every parameter and bind them in order\n");
    printf("  unbind <name|number> remove a parameter binding\n");
    printf("  name <text>          rename the controller in this config\n");
    printf("  save                 write the config file\n");
    printf("  help                 show this help\n");
    printf("  quit                 exit\n");
}

static int run_learn_command(int argc, char **argv)
{
    midi_mapping mapping;
    midi_portmidi_input midi;
    midi_device_callbacks callbacks;
    learn_context context;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    char line[MIDI_MONITOR_LINE_LENGTH];
    const char *path = 0;
    const char *name = 0;
    int edit_existing = 0;
    int has_midi_input;
    int backed_up = 0;
    int dirty = 0;
    int stream_count;

    midi_mapping_init(&mapping);

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            path = argv[i + 1];
            edit_existing = 0;
            i += 1;
        } else if (strcmp(argv[i], "--edit") == 0 && i + 1 < argc) {
            path = argv[i + 1];
            edit_existing = 1;
            i += 1;
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            name = argv[i + 1];
            i += 1;
        } else {
            print_usage();
            return 1;
        }
    }

    if (path == 0) {
        print_usage();
        return 1;
    }

    if (edit_existing &&
        !midi_mapping_load(&mapping, path, error, sizeof(error))) {
        fprintf(stderr, "could not load '%s': %s\n", path, error);
        return 1;
    }

    if (name != 0) {
        snprintf(mapping.name, sizeof(mapping.name), "%s", name);
        dirty = 1;
    } else if (!edit_existing) {
        snprintf(mapping.name, sizeof(mapping.name), "%s", "learned midi controller");
        dirty = 1;
    }

    memset(&context, 0, sizeof(context));
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.short_message = capture_cc_message;
    callbacks.user_data = &context;

    stream_count = midi_portmidi_init(&midi, callbacks);
    print_midi_status(stream_count, &midi, "use learn commands; type quit to stop.");
    has_midi_input = stream_count > 0;

    printf("editing: %s\n", path);
    print_learn_help();

    for (;;) {
        char *command;
        char *argument;

        printf("\nlearn> ");
        fflush(stdout);

        if (!read_line(line, sizeof(line))) {
            break;
        }

        command = trim(line);
        argument = command;
        while (*argument != '\0' && !isspace((unsigned char)*argument)) {
            ++argument;
        }
        if (*argument != '\0') {
            *argument = '\0';
            argument = trim(argument + 1);
        }

        if (*command == '\0') {
            continue;
        }

        if (strcmp(command, "help") == 0) {
            print_learn_help();
        } else if (strcmp(command, "list") == 0) {
            print_parameter_list(&mapping);
        } else if (strcmp(command, "bind") == 0) {
            const midi_mapping_parameter_info *info;

            if (!has_midi_input) {
                printf("no midi input stream is open, so bind cannot learn a control.\n");
                continue;
            }

            if (!parse_parameter_selection(argument, &info)) {
                printf("unknown parameter. use list to see names and numbers.\n");
                continue;
            }

            if (bind_parameter(&mapping, &midi, &context, info)) {
                dirty = 1;
            }
        } else if (strcmp(command, "map-all") == 0 || strcmp(command, "mapall") == 0) {
            if (!has_midi_input) {
                printf("no midi input stream is open, so map-all cannot learn controls.\n");
                continue;
            }

            if (map_all_parameters(&mapping, &midi, &context)) {
                dirty = 1;
            }
        } else if (strcmp(command, "unbind") == 0) {
            const midi_mapping_parameter_info *info;

            if (!parse_parameter_selection(argument, &info)) {
                printf("unknown parameter. use list to see names and numbers.\n");
                continue;
            }

            remove_bindings_for_parameter(&mapping, info->parameter);
            dirty = 1;
            printf("unbound %s\n", info->name);
        } else if (strcmp(command, "name") == 0) {
            if (*argument == '\0') {
                printf("usage: name <controller name>\n");
                continue;
            }

            snprintf(mapping.name, sizeof(mapping.name), "%s", argument);
            dirty = 1;
        } else if (strcmp(command, "save") == 0) {
            if (edit_existing && !backed_up) {
                if (!backup_mapping_file(path)) {
                    continue;
                }
                backed_up = 1;
            }

            if (save_mapping_file(path, &mapping)) {
                dirty = 0;
                printf("saved: %s\n", path);
            }
        } else if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
            if (dirty) {
                printf("unsaved changes. quit anyway? [y/N] > ");
                fflush(stdout);
                if (!read_line(line, sizeof(line)) ||
                    !(strcmp(line, "y") == 0 || strcmp(line, "yes") == 0)) {
                    continue;
                }
            }

            break;
        } else {
            printf("unknown command. type help.\n");
        }
    }

    midi_portmidi_uninit(&midi);
    return 0;
}

int main(int argc, char **argv)
{
    const char *path;

    if (argc == 1 || strcmp(argv[1], "monitor") == 0) {
        return run_monitor();
    }

    if (strcmp(argv[1], "learn") == 0) {
        return run_learn_command(argc, argv);
    }

    if (strcmp(argv[1], "show") == 0) {
        if (!parse_config_path(argc, argv, &path)) {
            print_usage();
            return 1;
        }

        return print_loaded_mapping(path);
    }

    if (strcmp(argv[1], "validate") == 0) {
        if (!parse_config_path(argc, argv, &path)) {
            print_usage();
            return 1;
        }

        return validate_mapping_file(path);
    }

    print_usage();
    return 1;
}
