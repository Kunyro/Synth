#ifndef SYNTH_ENVELOPE_H
#define SYNTH_ENVELOPE_H

// the stages an adsr envelope moves through.
typedef enum synth_envelope_stage {
    SYNTH_ENV_OFF = 0,
    SYNTH_ENV_ATTACK,
    SYNTH_ENV_DECAY,
    SYNTH_ENV_SUSTAIN,
    SYNTH_ENV_RELEASE
} synth_envelope_stage;

// the timing and sustain settings for an adsr envelope.
typedef struct synth_adsr {
    float attack_seconds;
    float decay_seconds;
    float sustain_level;
    float release_seconds;
} synth_adsr;

// an envelope with its settings, current stage, and current level.
typedef struct synth_envelope {
    synth_adsr adsr;
    synth_envelope_stage stage;
    float level;
} synth_envelope;

// sets up an envelope with adsr settings.
void synth_envelope_init(synth_envelope *envelope, synth_adsr adsr);
// starts the envelope attack stage.
void synth_envelope_note_on(synth_envelope *envelope);
// starts the envelope release stage.
void synth_envelope_note_off(synth_envelope *envelope);
// advances the envelope by one sample and returns its level.
float synth_envelope_advance(synth_envelope *envelope, float sample_rate);
// checks whether the envelope is still making sound.
int synth_envelope_is_active(const synth_envelope *envelope);

#endif
