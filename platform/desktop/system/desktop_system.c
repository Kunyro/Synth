#include "system/desktop_system.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <conio.h>
#include <ctype.h>
#include <io.h>
#include <windows.h>

#define DESKTOP_CONSOLE_LINE_LENGTH 1024

static char g_pending_line[DESKTOP_CONSOLE_LINE_LENGTH];
static size_t g_pending_line_length;
static int g_pending_line_ready;

static int stdin_is_console(void)
{
    return _isatty(_fileno(stdin));
}

static void clear_pending_line(void)
{
    g_pending_line[0] = '\0';
    g_pending_line_length = 0;
    g_pending_line_ready = 0;
}

static void copy_pending_line(char *line, size_t line_size)
{
    size_t copy_length;

    if (line_size == 0) {
        clear_pending_line();
        return;
    }

    copy_length = g_pending_line_length;
    if (copy_length >= line_size) {
        copy_length = line_size - 1;
    }

    memcpy(line, g_pending_line, copy_length);
    line[copy_length] = '\0';
    clear_pending_line();
}

static void collect_console_input(void)
{
    while (!g_pending_line_ready && _kbhit()) {
        int ch = _getch();

        if (ch == 0 || ch == 0xE0) {
            if (_kbhit()) {
                (void)_getch();
            }
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            g_pending_line[g_pending_line_length] = '\0';
            g_pending_line_ready = 1;
            fputc('\n', stdout);
            fflush(stdout);
        } else if (ch == '\b') {
            if (g_pending_line_length > 0) {
                g_pending_line_length -= 1;
                fputs("\b \b", stdout);
                fflush(stdout);
            }
        } else if (isprint((unsigned char)ch) || ch == '\t') {
            if (g_pending_line_length + 1 < sizeof(g_pending_line)) {
                g_pending_line[g_pending_line_length] = (char)ch;
                g_pending_line_length += 1;
                fputc(ch, stdout);
                fflush(stdout);
            }
        }
    }
}

int desktop_stdin_line_ready(void)
{
    HANDLE input;
    DWORD available = 0;
    DWORD file_type;

    if (stdin_is_console()) {
        collect_console_input();
        return g_pending_line_ready;
    }

    input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE) {
        return 0;
    }

    file_type = GetFileType(input);
    if (file_type == FILE_TYPE_DISK) {
        return !_eof(_fileno(stdin));
    }

    if (input != INVALID_HANDLE_VALUE &&
        file_type == FILE_TYPE_PIPE &&
        PeekNamedPipe(input, 0, 0, 0, &available, 0)) {
        return available > 0;
    }

    return 0;
}

int desktop_read_stdin_line(char *line, size_t line_size)
{
    if (line_size == 0) {
        return 0;
    }

    if (!stdin_is_console()) {
        return fgets(line, line_size, stdin) != 0;
    }

    while (!g_pending_line_ready) {
        collect_console_input();
        if (!g_pending_line_ready) {
            desktop_sleep_ms(5);
        }
    }

    copy_pending_line(line, line_size);
    return 1;
}

void desktop_sleep_ms(unsigned int milliseconds)
{
    Sleep(milliseconds);
}

void desktop_sleep_us(unsigned int microseconds)
{
    if (microseconds == 0) {
        return;
    }

    Sleep((microseconds + 999U) / 1000U);
}
#else
#include <sys/select.h>
#include <unistd.h>

int desktop_stdin_line_ready(void)
{
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return select(STDIN_FILENO + 1, &read_fds, 0, 0, &timeout) > 0;
}

int desktop_read_stdin_line(char *line, size_t line_size)
{
    if (line_size == 0) {
        return 0;
    }

    return fgets(line, line_size, stdin) != 0;
}

void desktop_sleep_ms(unsigned int milliseconds)
{
    usleep(milliseconds * 1000U);
}

void desktop_sleep_us(unsigned int microseconds)
{
    usleep(microseconds);
}
#endif
