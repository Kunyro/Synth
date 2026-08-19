#ifndef DESKTOP_MIDI_MAPPING_INTERNAL_H
#define DESKTOP_MIDI_MAPPING_INTERNAL_H

#include "midi/midi_mapping.h"

typedef float (*midi_mapping_parameter_getter)(const synth *s);
typedef void (*midi_mapping_parameter_setter)(synth *s, float value);

typedef struct midi_mapping_parameter_entry {
    midi_mapping_parameter parameter;
    const char *name;
    midi_mapping_parameter_getter get;
    midi_mapping_parameter_setter set;
    midi_mapping_scale default_scale;
    float default_min_value;
    float default_max_value;
} midi_mapping_parameter_entry;

typedef struct midi_mapping_chord_entry {
    midi_chord_mode_pad pad;
    const char *name;
} midi_mapping_chord_entry;

const midi_mapping_parameter_entry *midi_mapping_find_parameter(midi_mapping_parameter parameter);
const midi_mapping_parameter_entry *midi_mapping_find_parameter_by_name(const char *name);
const midi_mapping_chord_entry *midi_mapping_find_chord_by_name(const char *name);
void midi_mapping_fill_parameter_info(
    const midi_mapping_parameter_entry *entry,
    midi_mapping_parameter_info *info);

#endif
