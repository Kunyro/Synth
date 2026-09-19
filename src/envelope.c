#include "synth/envelope.h"

#include "internal/synth_internal.h"

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
    envelope->full_duration_release = 0;
    envelope->release_start_level = 0.0f;
    envelope->release_elapsed = 0.0;
}

// a changed release time starts a new full-duration ramp from the current level
void synth_envelope_set_adsr(synth_envelope *envelope, synth_adsr adsr)
{
    adsr = synth_sanitize_adsr(adsr);
    if (envelope->full_duration_release && envelope->stage == SYNTH_ENV_RELEASE &&
        adsr.release_seconds != envelope->adsr.release_seconds) {
        envelope->release_start_level = envelope->level;
        envelope->release_elapsed = 0.0;
    }
    envelope->adsr = adsr;
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
    if (envelope->stage != SYNTH_ENV_OFF && envelope->stage != SYNTH_ENV_RELEASE) {
        envelope->stage = SYNTH_ENV_RELEASE;
        envelope->release_start_level = envelope->level;
        envelope->release_elapsed = 0.0;
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
            // decay moves from full level down to sustain over the decay time.
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
            if (envelope->full_duration_release) {
                envelope->release_elapsed += 1.0;
                const double duration = round((double)envelope->adsr.release_seconds * sample_rate);
                const double remaining = duration > 0.0
                    ? 1.0 - envelope->release_elapsed / duration : 0.0;
                envelope->level = remaining > 0.0
                    ? envelope->release_start_level * (float)remaining : 0.0f;
            } else {
                envelope->level -= release_step;
            }
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
