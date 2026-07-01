#include "synth/synth.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static int buffer_has_signal(const float *buffer, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (fabsf(buffer[i]) > 0.0001f) {
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    synth s;
    synth_adsr quick_release = {0.0f, 0.0f, 1.0f, 0.001f};
    float buffer[256];
    int active_count = 0;

    synth_init(&s, 48000.0f);
    memset(buffer, 1, sizeof(buffer));
    synth_render_mono(&s, buffer, 128);
    expect_true(!buffer_has_signal(buffer, 128), "synth renders silence with no active voices");

    synth_note_on(&s, 60, 1.0f);
    synth_note_on(&s, 64, 1.0f);
    synth_note_on(&s, 67, 1.0f);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (s.voices[i].active) {
            ++active_count;
        }
    }
    expect_true(active_count == 3, "multiple note ons allocate multiple voices");

    synth_render_mono(&s, buffer, 256);
    expect_true(buffer_has_signal(buffer, 256), "active voices render signal");

    synth_set_adsr(&s, quick_release);
    synth_note_on(&s, 69, 1.0f);
    synth_render_mono(&s, buffer, 64);
    synth_note_off(&s, 69);
    synth_render_mono(&s, buffer, 256);
    expect_true(!s.voices[3].active, "note off releases voice");

    synth_set_waveform(&s, SYNTH_WAVEFORM_SQUARE);
    synth_set_filter_cutoff(&s, 900.0f);
    synth_note_on_frequency(&s, 440.0f, 1.0f);
    synth_render_mono(&s, buffer, 256);
    expect_true(buffer_has_signal(buffer, 256), "direct frequency voice renders through filter");

    if (failures != 0) {
        fprintf(stderr, "%d voice test(s) failed\n", failures);
        return 1;
    }

    printf("All voice tests passed.\n");
    return 0;
}
