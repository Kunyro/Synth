#ifndef DESKTOP_MIDI_TEXT_H
#define DESKTOP_MIDI_TEXT_H

#include <stddef.h>

char *midi_text_trim(char *text);
void midi_text_copy(char *target, size_t target_size, const char *source);

#endif
