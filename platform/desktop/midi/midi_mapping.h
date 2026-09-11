#ifndef DESKTOP_MIDI_MAPPING_H
#define DESKTOP_MIDI_MAPPING_H

#include <stddef.h>

#include "midi/chord_mode.h"
#include "synth/synth.h"

// capacity for the 107 base/amount controls plus repeated assignments from other
// knobs or channels selectors and chord pads have separate storage below
#define MIDI_MAPPING_MAX_BINDINGS 256
// the longest controller name stored from a mapping file
#define MIDI_MAPPING_NAME_LENGTH 64
// each effect macro page exposes three parameter knobs
#define MIDI_MAPPING_EFFECT_MACRO_COUNT 3
// each mapping can expose two independent selector/macro rows
#define MIDI_MAPPING_EFFECT_BANK_COUNT 2
// each selector row exposes five effect pages
#define MIDI_MAPPING_EFFECTS_PER_BANK 5
// the longest mapping load error message
#define MIDI_MAPPING_ERROR_LENGTH 160
// how close a knob must get to the current parameter before it takes over
#define MIDI_MAPPING_PICKUP_THRESHOLD 1.0f

// controller bindings refer to core identities and choose which property to edit
typedef synth_parameter_id midi_mapping_parameter;

typedef enum midi_mapping_target_kind {
    // a base edit changes the stored knob value; an amount edit changes its lfo route
    MIDI_MAPPING_TARGET_BASE = 0,
    MIDI_MAPPING_TARGET_LFO_AMOUNT
} midi_mapping_target_kind;

// the kind of midi source a binding listens for
typedef enum midi_mapping_source_type {
    MIDI_MAPPING_SOURCE_CC = 0
} midi_mapping_source_type;

// how a midi value should be scaled into a synth value
typedef enum midi_mapping_scale {
    MIDI_MAPPING_SCALE_LINEAR = 0,
    MIDI_MAPPING_SCALE_LOG,
    MIDI_MAPPING_SCALE_STEP
} midi_mapping_scale;

// the effect pages selected by the effect-selector macro knob
typedef enum midi_mapping_effect {
    MIDI_MAPPING_EFFECT_SATURATION = 0,
    MIDI_MAPPING_EFFECT_DISTORTION,
    MIDI_MAPPING_EFFECT_BITCRUSHER,
    MIDI_MAPPING_EFFECT_DELAY,
    MIDI_MAPPING_EFFECT_PLATE_REVERB,
    MIDI_MAPPING_EFFECT_FLANGER,
    MIDI_MAPPING_EFFECT_RING_MOD,
    MIDI_MAPPING_EFFECT_CHORUS,
    MIDI_MAPPING_EFFECT_EQ,
    MIDI_MAPPING_EFFECT_COMPRESSOR,
    MIDI_MAPPING_EFFECT_COUNT
} midi_mapping_effect;

// editable defaults for one synth parameter in the midi learn utility
typedef struct midi_mapping_parameter_info {
    midi_mapping_parameter parameter;
    midi_mapping_target_kind target_kind;
    // caller-owned name storage includes room for the optional "lfo_amount." prefix
    char name[80];
    midi_mapping_scale default_scale;
    float default_min_value;
    float default_max_value;
} midi_mapping_parameter_info;

// runtime state for soft takeover on one midi control
typedef struct midi_mapping_pickup {
    int picked_up;
    int has_last_midi_value;
    int last_midi_value;
} midi_mapping_pickup;

// one simple midi control binding without synth-parameter scaling
typedef struct midi_mapping_control_binding {
    int enabled;
    midi_mapping_source_type source_type;
    int channel;
    int control;
} midi_mapping_control_binding;

// one binding from a midi control to a synth parameter
typedef struct midi_mapping_binding {
    midi_mapping_parameter parameter;
    midi_mapping_target_kind target_kind;
    midi_mapping_source_type source_type;
    int channel;
    int control;
    midi_mapping_scale scale;
    float min_value;
    float max_value;
    midi_mapping_pickup pickup;
} midi_mapping_binding;

// one binding from a midi control to a chord-mode pad
typedef struct midi_mapping_chord_binding {
    int enabled;
    midi_chord_mode_pad pad;
    midi_mapping_source_type source_type;
    int channel;
    int control;
} midi_mapping_chord_binding;

// one selector/macro row for controlling a fixed bank of effect pages
typedef struct midi_mapping_effect_bank {
    midi_mapping_control_binding selector;
    midi_mapping_control_binding macros[MIDI_MAPPING_EFFECT_MACRO_COUNT];
    int has_selected_effect;
    midi_mapping_effect selected_effect;
    midi_mapping_pickup pickups[MIDI_MAPPING_EFFECT_COUNT][MIDI_MAPPING_EFFECT_MACRO_COUNT];
} midi_mapping_effect_bank;

// a loaded controller mapping with all of its bindings
typedef struct midi_mapping {
    char name[MIDI_MAPPING_NAME_LENGTH];
    midi_mapping_binding bindings[MIDI_MAPPING_MAX_BINDINGS];
    size_t binding_count;
    midi_mapping_chord_binding chord_bindings[MIDI_CHORD_MODE_PAD_COUNT];
    midi_mapping_effect_bank effect_banks[MIDI_MAPPING_EFFECT_BANK_COUNT];
} midi_mapping;

// the kind of mapping action produced by a midi message
typedef enum midi_mapping_apply_kind {
    MIDI_MAPPING_APPLY_PARAMETER = 0,
    MIDI_MAPPING_APPLY_EFFECT_SELECT
} midi_mapping_apply_kind;

// details about a midi message that matched a mapping
typedef struct midi_mapping_apply_result {
    midi_mapping_apply_kind kind;
    midi_mapping_parameter parameter;
    midi_mapping_target_kind target_kind;
    size_t effect_bank_index;
    int has_effect;
    midi_mapping_effect effect;
    int channel;
    int control;
    int midi_value;
    float synth_value;
} midi_mapping_apply_result;

// clears a midi mapping and gives it a fallback name
void midi_mapping_init(midi_mapping *mapping);
// counts base controls plus all eligible route-amount controls
size_t midi_mapping_parameter_count(void);
// fills metadata by zero-based list index (bases first, then amounts); returns success
int midi_mapping_parameter_info_at(size_t index, midi_mapping_parameter_info *info);
// fills metadata for a canonical base name or lfo_amount.<base name>; returns success
int midi_mapping_parameter_info_by_name(const char *name, midi_mapping_parameter_info *info);
// fills caller-owned metadata; no shared mutable result or platform state in the core
int midi_mapping_parameter_info_for(midi_mapping_parameter parameter,
    midi_mapping_target_kind kind, midi_mapping_parameter_info *info);
// returns the readable name for a mapped synth parameter
const char *midi_mapping_parameter_name(midi_mapping_parameter parameter);
// returns the config spelling for a chord-mode pad
const char *midi_mapping_chord_pad_name(midi_chord_mode_pad pad);
// returns the config spelling for an effect macro page
const char *midi_mapping_effect_name(midi_mapping_effect effect);
// returns the config spelling for an effect selector control by zero-based bank index
const char *midi_mapping_effect_selector_name(size_t bank_index);
// returns the config spelling for an effect macro control by zero-based bank and macro indexes
const char *midi_mapping_effect_macro_name(size_t bank_index, size_t macro_index);
// returns the effect assigned to a selector bank page, or 0 when the page is blank
int midi_mapping_effect_bank_page(
    size_t bank_index,
    size_t page_index,
    midi_mapping_effect *effect);
// returns the synth parameter routed by an effect page macro, or 0 when unused
int midi_mapping_effect_macro_parameter(
    midi_mapping_effect effect,
    size_t macro_index,
    midi_mapping_parameter *parameter);
// returns the config spelling for a scale
const char *midi_mapping_scale_name(midi_mapping_scale scale);
// parses the config spelling for a scale
int midi_mapping_parse_scale_name(const char *name, midi_mapping_scale *scale);
// loads a midi mapping from a config file
int midi_mapping_load(midi_mapping *mapping, const char *path, char *error, size_t error_size);
// shared validation for config loading, midi learn, and saving
int midi_mapping_validate_binding(const midi_mapping_binding *binding, char *error, size_t error_size);
// saves bindings only; runtime values and pickup state are never persisted
int midi_mapping_save(const midi_mapping *mapping, const char *path, char *error, size_t error_size);
// copies loaded chord pad bindings into a chord-mode processor
void midi_mapping_configure_chord_mode(const midi_mapping *mapping, midi_chord_mode *mode);
// applies a raw midi message to the synth when it matches a binding
int midi_mapping_apply_short_message(
    midi_mapping *mapping,
    const unsigned char *data,
    unsigned short length,
    synth *s,
    midi_mapping_apply_result *result);

#endif
