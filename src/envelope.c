#include "synth/envelope.h"

#include "synth_internal.h"

// turns a time in seconds into a per sample step.
static float seconds_to_step(float sample_rate, float seconds)
{
    if (seconds <= 0.0f) {
        return 1.0f;
    }

    return 1.0f / (seconds * sample_rate);
}

// sets up an envelope with adsr settings.
void synth_envelope_init(synth_envelope *envelope, synth_adsr adsr)
{
    envelope->adsr = synth_sanitize_adsr(adsr);
    envelope->stage = SYNTH_ENV_OFF;
    envelope->level = 0.0f;
}

// starts the envelope attack stage.
void synth_envelope_note_on(synth_envelope *envelope)
{
    envelope->stage = SYNTH_ENV_ATTACK;
    envelope->level = 0.0f;
}

// starts the envelope release stage.
void synth_envelope_note_off(synth_envelope *envelope)
{
    if (envelope->stage != SYNTH_ENV_OFF) {
        envelope->stage = SYNTH_ENV_RELEASE;
    }
}

// advances the envelope by one sample and returns its level.
float synth_envelope_advance(synth_envelope *envelope, float sample_rate)
{
    const float attack_step = seconds_to_step(sample_rate, envelope->adsr.attack_seconds);
    const float decay_step = seconds_to_step(sample_rate, envelope->adsr.decay_seconds);
    const float release_step = seconds_to_step(sample_rate, envelope->adsr.release_seconds);

    switch (envelope->stage) {
        case SYNTH_ENV_ATTACK:
            envelope->level += attack_step;
            if (envelope->level >= 1.0f) {
                envelope->level = 1.0f;
                envelope->stage = SYNTH_ENV_DECAY;
            }
            break;

        case SYNTH_ENV_DECAY:
            envelope->level -= decay_step * (1.0f - envelope->adsr.sustain_level);
            if (envelope->level <= envelope->adsr.sustain_level) {
                envelope->level = envelope->adsr.sustain_level;
                envelope->stage = SYNTH_ENV_SUSTAIN;
            }
            break;

        case SYNTH_ENV_SUSTAIN:
            envelope->level = envelope->adsr.sustain_level;
            break;

        case SYNTH_ENV_RELEASE:
            envelope->level -= release_step;
            if (envelope->level <= 0.0f) {
                envelope->level = 0.0f;
                envelope->stage = SYNTH_ENV_OFF;
            }
            break;

        case SYNTH_ENV_OFF:
        default:
            envelope->level = 0.0f;
            break;
    }

    return envelope->level;
}

// checks whether the envelope is still making sound.
int synth_envelope_is_active(const synth_envelope *envelope)
{
    return envelope->stage != SYNTH_ENV_OFF;
}
