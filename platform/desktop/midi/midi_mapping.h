#ifndef DESKTOP_MIDI_MAPPING_H
#define DESKTOP_MIDI_MAPPING_H

#include <stddef.h>

#include "synth/synth.h"

// the most midi controls one mapping file can bind.
#define MIDI_MAPPING_MAX_BINDINGS 64
// the longest controller name stored from a mapping file.
#define MIDI_MAPPING_NAME_LENGTH 64
// the longest mapping load error message.
#define MIDI_MAPPING_ERROR_LENGTH 160
// how close a knob must get to the current parameter before it takes over.
#define MIDI_MAPPING_PICKUP_THRESHOLD 1.0f

// the synth parameters a midi control can update.
typedef enum midi_mapping_parameter {
    MIDI_MAPPING_PARAM_ATTACK = 0,
    MIDI_MAPPING_PARAM_DECAY,
    MIDI_MAPPING_PARAM_SUSTAIN,
    MIDI_MAPPING_PARAM_RELEASE,
    MIDI_MAPPING_PARAM_MASTER_GAIN,
    MIDI_MAPPING_PARAM_FILTER_CUTOFF,
    MIDI_MAPPING_PARAM_FILTER_POLES,
    MIDI_MAPPING_PARAM_OSCILLATOR_MORPH,
    MIDI_MAPPING_PARAM_FIRST_OSCILLATOR_GAIN,
    MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_GAIN,
    MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_MORPH,
    MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_OCTAVE,
    MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_PITCH,
    MIDI_MAPPING_PARAM_SECOND_OSCILLATOR_FINE_TUNE,
    MIDI_MAPPING_PARAM_STEREO_SPREAD,
    MIDI_MAPPING_PARAM_LFO_RATE,
    MIDI_MAPPING_PARAM_LFO_SHAPE_MORPH,
    MIDI_MAPPING_PARAM_LFO_DEPTH,
    MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_MORPH_AMOUNT,
    MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_MORPH_AMOUNT,
    MIDI_MAPPING_PARAM_LFO_FIRST_OSCILLATOR_GAIN_AMOUNT,
    MIDI_MAPPING_PARAM_LFO_SECOND_OSCILLATOR_GAIN_AMOUNT,
    MIDI_MAPPING_PARAM_LFO_FILTER_AMOUNT,
    MIDI_MAPPING_PARAM_DISTORTION_DRIVE,
    MIDI_MAPPING_PARAM_DISTORTION_MIX
} midi_mapping_parameter;

// the kind of midi source a binding listens for.
typedef enum midi_mapping_source_type {
    MIDI_MAPPING_SOURCE_CC = 0
} midi_mapping_source_type;

// how a midi value should be scaled into a synth value.
typedef enum midi_mapping_scale {
    MIDI_MAPPING_SCALE_LINEAR = 0,
    MIDI_MAPPING_SCALE_LOG,
    MIDI_MAPPING_SCALE_STEP
} midi_mapping_scale;

// runtime state for soft takeover on one midi control.
typedef struct midi_mapping_pickup {
    int picked_up;
    int has_last_midi_value;
    int last_midi_value;
} midi_mapping_pickup;

// one binding from a midi control to a synth parameter.
typedef struct midi_mapping_binding {
    midi_mapping_parameter parameter;
    midi_mapping_source_type source_type;
    int channel;
    int control;
    midi_mapping_scale scale;
    float min_value;
    float max_value;
    midi_mapping_pickup pickup;
} midi_mapping_binding;

// a loaded controller mapping with all of its bindings.
typedef struct midi_mapping {
    char name[MIDI_MAPPING_NAME_LENGTH];
    midi_mapping_binding bindings[MIDI_MAPPING_MAX_BINDINGS];
    size_t binding_count;
} midi_mapping;

// details about a midi message that changed a synth value.
typedef struct midi_mapping_apply_result {
    midi_mapping_parameter parameter;
    int channel;
    int control;
    int midi_value;
    float synth_value;
} midi_mapping_apply_result;

// clears a midi mapping and gives it a fallback name.
void midi_mapping_init(midi_mapping *mapping);
// returns the readable name for a mapped synth parameter.
const char *midi_mapping_parameter_name(midi_mapping_parameter parameter);
// loads a midi mapping from a config file.
int midi_mapping_load(midi_mapping *mapping, const char *path, char *error, size_t error_size);
// applies a raw midi message to the synth when it matches a binding.
int midi_mapping_apply_short_message(
    midi_mapping *mapping,
    const unsigned char *data,
    unsigned short length,
    synth *s,
    midi_mapping_apply_result *result);

#endif
