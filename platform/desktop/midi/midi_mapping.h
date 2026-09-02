#ifndef DESKTOP_MIDI_MAPPING_H
#define DESKTOP_MIDI_MAPPING_H

#include <stddef.h>

#include "midi/chord_mode.h"
#include "synth/synth.h"

// the most midi controls one mapping file can bind.
#define MIDI_MAPPING_MAX_BINDINGS 64
// the longest controller name stored from a mapping file.
#define MIDI_MAPPING_NAME_LENGTH 64
// each effect macro page exposes three parameter knobs.
#define MIDI_MAPPING_EFFECT_MACRO_COUNT 3
// each mapping can expose two independent selector/macro rows.
#define MIDI_MAPPING_EFFECT_BANK_COUNT 2
// each selector row exposes five effect pages.
#define MIDI_MAPPING_EFFECTS_PER_BANK 5
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
    MIDI_MAPPING_PARAM_DISTORTION_MIX,
    MIDI_MAPPING_PARAM_BITCRUSHER_SAMPLE_RATE,
    MIDI_MAPPING_PARAM_BITCRUSHER_BITS,
    MIDI_MAPPING_PARAM_BITCRUSHER_MIX,
    MIDI_MAPPING_PARAM_FLANGER_RATE,
    MIDI_MAPPING_PARAM_FLANGER_INTENSITY,
    MIDI_MAPPING_PARAM_FLANGER_DEPTH,
    MIDI_MAPPING_PARAM_FLANGER_FEEDBACK,
    MIDI_MAPPING_PARAM_FLANGER_MIX,
    MIDI_MAPPING_PARAM_FLANGER_MANUAL,
    MIDI_MAPPING_PARAM_RING_MOD_FREQUENCY,
    MIDI_MAPPING_PARAM_RING_MOD_RECTIFY,
    MIDI_MAPPING_PARAM_RING_MOD_MIX,
    MIDI_MAPPING_PARAM_CHORUS_RATE,
    MIDI_MAPPING_PARAM_CHORUS_DEPTH,
    MIDI_MAPPING_PARAM_CHORUS_MIX,
    MIDI_MAPPING_PARAM_CHORUS_WIDTH,
    MIDI_MAPPING_PARAM_CHORUS_DELAY,
    MIDI_MAPPING_PARAM_CHORUS_FEEDBACK,
    MIDI_MAPPING_PARAM_EQ_LOW,
    MIDI_MAPPING_PARAM_EQ_MID,
    MIDI_MAPPING_PARAM_EQ_HIGH,
    MIDI_MAPPING_PARAM_DELAY_TIME,
    MIDI_MAPPING_PARAM_DELAY_FEEDBACK,
    MIDI_MAPPING_PARAM_DELAY_MIX,
    MIDI_MAPPING_PARAM_SATURATION_DRIVE,
    MIDI_MAPPING_PARAM_SATURATION_MIX,
    MIDI_MAPPING_PARAM_PLATE_REVERB_DECAY,
    MIDI_MAPPING_PARAM_PLATE_REVERB_DAMPING,
    MIDI_MAPPING_PARAM_PLATE_REVERB_MIX,
    MIDI_MAPPING_PARAM_PLATE_REVERB_PREDELAY,
    MIDI_MAPPING_PARAM_COMPRESSOR_THRESHOLD,
    MIDI_MAPPING_PARAM_COMPRESSOR_RATIO,
    MIDI_MAPPING_PARAM_COMPRESSOR_MAKEUP_GAIN,
    MIDI_MAPPING_PARAM_COMPRESSOR_ATTACK_SECONDS,
    MIDI_MAPPING_PARAM_COMPRESSOR_RELEASE_SECONDS
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

// the effect pages selected by the effect-selector macro knob.
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

// editable defaults for one synth parameter in the midi learn utility.
typedef struct midi_mapping_parameter_info {
    midi_mapping_parameter parameter;
    const char *name;
    midi_mapping_scale default_scale;
    float default_min_value;
    float default_max_value;
} midi_mapping_parameter_info;

// runtime state for soft takeover on one midi control.
typedef struct midi_mapping_pickup {
    int picked_up;
    int has_last_midi_value;
    int last_midi_value;
} midi_mapping_pickup;

// one simple midi control binding without synth-parameter scaling.
typedef struct midi_mapping_control_binding {
    int enabled;
    midi_mapping_source_type source_type;
    int channel;
    int control;
} midi_mapping_control_binding;

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

// one binding from a midi control to a chord-mode pad.
typedef struct midi_mapping_chord_binding {
    int enabled;
    midi_chord_mode_pad pad;
    midi_mapping_source_type source_type;
    int channel;
    int control;
} midi_mapping_chord_binding;

// one selector/macro row for controlling a fixed bank of effect pages.
typedef struct midi_mapping_effect_bank {
    midi_mapping_control_binding selector;
    midi_mapping_control_binding macros[MIDI_MAPPING_EFFECT_MACRO_COUNT];
    int has_selected_effect;
    midi_mapping_effect selected_effect;
    midi_mapping_pickup pickups[MIDI_MAPPING_EFFECT_COUNT][MIDI_MAPPING_EFFECT_MACRO_COUNT];
} midi_mapping_effect_bank;

// a loaded controller mapping with all of its bindings.
typedef struct midi_mapping {
    char name[MIDI_MAPPING_NAME_LENGTH];
    midi_mapping_binding bindings[MIDI_MAPPING_MAX_BINDINGS];
    size_t binding_count;
    midi_mapping_chord_binding chord_bindings[MIDI_CHORD_MODE_PAD_COUNT];
    midi_mapping_effect_bank effect_banks[MIDI_MAPPING_EFFECT_BANK_COUNT];
} midi_mapping;

// the kind of mapping action produced by a midi message.
typedef enum midi_mapping_apply_kind {
    MIDI_MAPPING_APPLY_PARAMETER = 0,
    MIDI_MAPPING_APPLY_EFFECT_SELECT
} midi_mapping_apply_kind;

// details about a midi message that matched a mapping.
typedef struct midi_mapping_apply_result {
    midi_mapping_apply_kind kind;
    midi_mapping_parameter parameter;
    size_t effect_bank_index;
    int has_effect;
    midi_mapping_effect effect;
    int channel;
    int control;
    int midi_value;
    float synth_value;
} midi_mapping_apply_result;

// clears a midi mapping and gives it a fallback name.
void midi_mapping_init(midi_mapping *mapping);
// returns how many synth parameters can be mapped.
size_t midi_mapping_parameter_count(void);
// returns metadata for a mappable synth parameter by index.
const midi_mapping_parameter_info *midi_mapping_parameter_info_at(size_t index);
// returns metadata for a mappable synth parameter by name.
const midi_mapping_parameter_info *midi_mapping_parameter_info_by_name(const char *name);
// returns the readable name for a mapped synth parameter.
const char *midi_mapping_parameter_name(midi_mapping_parameter parameter);
// returns the config spelling for a chord-mode pad.
const char *midi_mapping_chord_pad_name(midi_chord_mode_pad pad);
// returns the config spelling for an effect macro page.
const char *midi_mapping_effect_name(midi_mapping_effect effect);
// returns the config spelling for an effect selector control by zero-based bank index.
const char *midi_mapping_effect_selector_name(size_t bank_index);
// returns the config spelling for an effect macro control by zero-based bank and macro indexes.
const char *midi_mapping_effect_macro_name(size_t bank_index, size_t macro_index);
// returns the effect assigned to a selector bank page, or 0 when the page is blank.
int midi_mapping_effect_bank_page(
    size_t bank_index,
    size_t page_index,
    midi_mapping_effect *effect);
// returns the synth parameter routed by an effect page macro, or 0 when unused.
int midi_mapping_effect_macro_parameter(
    midi_mapping_effect effect,
    size_t macro_index,
    midi_mapping_parameter *parameter);
// returns the config spelling for a scale.
const char *midi_mapping_scale_name(midi_mapping_scale scale);
// parses the config spelling for a scale.
int midi_mapping_parse_scale_name(const char *name, midi_mapping_scale *scale);
// loads a midi mapping from a config file.
int midi_mapping_load(midi_mapping *mapping, const char *path, char *error, size_t error_size);
// copies loaded chord pad bindings into a chord-mode processor.
void midi_mapping_configure_chord_mode(const midi_mapping *mapping, midi_chord_mode *mode);
// applies a raw midi message to the synth when it matches a binding.
int midi_mapping_apply_short_message(
    midi_mapping *mapping,
    const unsigned char *data,
    unsigned short length,
    synth *s,
    midi_mapping_apply_result *result);

#endif
