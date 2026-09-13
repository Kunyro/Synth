#include "midi/midi_text.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// trims whitespace from both ends of a mutable string.
char *midi_text_trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        ++text;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return text;
}

// copies text into a fixed size string buffer.
void midi_text_copy(char *target, size_t target_size, const char *source)
{
    if (target_size == 0) {
        return;
    }

    snprintf(target, target_size, "%s", source);
}
