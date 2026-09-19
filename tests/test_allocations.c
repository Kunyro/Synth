// this target compiles the core with allocator symbols redirected to these hooks
#undef calloc
#undef free
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "synth/synth.h"

static size_t requests;
static size_t live_blocks;
static size_t fail_at = SIZE_MAX;
static int failures;

void *synth_test_calloc(size_t count, size_t size)
{
    if (requests++ == fail_at || (size != 0 && count > SIZE_MAX / size)) return NULL;
    void *p = malloc(count * size);
    if (p) { memset(p, 0, count * size); ++live_blocks; }
    return p;
}
void synth_test_free(void *p)
{
    if (p) { --live_blocks; free(p); }
}
static void check(int condition, const char *message)
{
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

int main(void)
{
    synth s;
    synth_init(&s, 8000);
    check(synth_is_ready(&s), "complete initialization succeeds");
    const size_t allocation_count = requests;
    synth_set_adsr(&s, (synth_adsr){0, 0, 1, 0});
    for (int i = 0; i < 12; ++i) synth_note_on(&s, 48 + i, 1);
    float output[64];
    synth_render_mono(&s, output, 64);
    synth_note_on(&s, 90, 1);
    synth_render_mono(&s, output, 64);
    synth_set_delay_time(&s, 0.4f);
    synth_set_mod_envelope_depth(&s, 1);
    synth_set_envelope_amount(&s, SYNTH_PARAM_DELAY_MIX, 1);
    synth_all_notes_off(&s);
    synth_render_mono(&s, output, 64);
    check(requests == allocation_count, "notes, steals, manual edits, and rendering allocate no buffers");
    synth_uninit(&s);
    check(live_blocks == 0, "all voice resources are released");

    // each allocation site is failed in turn, including partially initialized late voices
    for (size_t site = 0; site < allocation_count; ++site) {
        requests = 0;
        fail_at = site;
        synth_init(&s, 1000);
        check(!synth_is_ready(&s), "partial initialization reports failure");
        check(live_blocks == 0, "partial initialization immediately releases every buffer");
        synth_note_on(&s, 60, 1);
        synth_render_mono(&s, output, 64);
        for (int i = 0; i < 64; ++i) check(output[i] == 0, "failed instance renders silence");
        synth_uninit(&s);
        check(live_blocks == 0, "failed instance is safely destructible");
    }
    if (failures) return 1;
    printf("Allocation checks passed across %zu failure sites.\n", allocation_count);
    return 0;
}
