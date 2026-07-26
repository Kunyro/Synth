#ifndef DESKTOP_SYSTEM_H
#define DESKTOP_SYSTEM_H

#include <stddef.h>

// returns non-zero when stdin has a full line ready to read.
int desktop_stdin_line_ready(void);
// reads one line from stdin, using the same console handling as line_ready.
int desktop_read_stdin_line(char *line, size_t line_size);
// sleeps for a whole number of milliseconds.
void desktop_sleep_ms(unsigned int milliseconds);
// sleeps for a whole number of microseconds.
void desktop_sleep_us(unsigned int microseconds);

#endif
