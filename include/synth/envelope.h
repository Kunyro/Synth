#ifndef SYNTH_ENVELOPE_H
#define SYNTH_ENVELOPE_H

typedef enum synth_envelope_stage {
    SYNTH_ENV_OFF = 0,
    SYNTH_ENV_ATTACK,
    SYNTH_ENV_DECAY,
    SYNTH_ENV_SUSTAIN,
    SYNTH_ENV_RELEASE
} synth_envelope_stage;

typedef struct synth_adsr {
    float attack_seconds;
    float decay_seconds;
    float sustain_level;
    float release_seconds;
} synth_adsr;

typedef struct synth_envelope {
    synth_adsr adsr;
    synth_envelope_stage stage;
    float level;
} synth_envelope;

void synth_envelope_init(synth_envelope *envelope, synth_adsr adsr);
void synth_envelope_note_on(synth_envelope *envelope);
void synth_envelope_note_off(synth_envelope *envelope);
float synth_envelope_advance(synth_envelope *envelope, float sample_rate);
int synth_envelope_is_active(const synth_envelope *envelope);

#endif
