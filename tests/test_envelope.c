#include "synth/envelope.h"

#include <stdio.h>

static int failures = 0;

static void expect_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    synth_adsr adsr = {0.0f, 0.0f, 0.5f, 0.001f};
    synth_envelope envelope;

    synth_envelope_init(&envelope, adsr);
    expect_true(!synth_envelope_is_active(&envelope), "envelope starts inactive");

    synth_envelope_note_on(&envelope);
    synth_envelope_advance(&envelope, 48000.0f);
    expect_true(synth_envelope_is_active(&envelope), "note on activates envelope");
    expect_true(envelope.level > 0.0f, "attack raises envelope level");

    synth_envelope_note_off(&envelope);
    for (int i = 0; i < 128; ++i) {
        synth_envelope_advance(&envelope, 48000.0f);
    }
    expect_true(!synth_envelope_is_active(&envelope), "release deactivates envelope");

    if (failures != 0) {
        fprintf(stderr, "%d envelope test(s) failed\n", failures);
        return 1;
    }

    printf("All envelope tests passed.\n");
    return 0;
}
