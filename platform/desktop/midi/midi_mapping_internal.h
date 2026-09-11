#ifndef DESKTOP_MIDI_MAPPING_INTERNAL_H
#define DESKTOP_MIDI_MAPPING_INTERNAL_H

#include "midi/midi_mapping.h"

typedef struct midi_mapping_chord_entry {
    midi_chord_mode_pad pad;
    const char *name;
} midi_mapping_chord_entry;

const midi_mapping_chord_entry *midi_mapping_find_chord_by_name(const char *name);

#endif
