CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude -Iplatform/desktop -Ithird_party
LDLIBS ?= -lm -lpthread

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
LDLIBS += -framework CoreFoundation -framework CoreAudio -framework AudioToolbox -framework AudioUnit
else ifeq ($(UNAME_S),Linux)
LDLIBS += -ldl
endif

TARGET := build/synth
OSCILLATOR_TEST_TARGET := build/test_oscillator
ENVELOPE_TEST_TARGET := build/test_envelope
FILTER_TEST_TARGET := build/test_filter
VOICE_TEST_TARGET := build/test_voice
MIDI_TYPES_TEST_TARGET := build/test_midi_types

CORE_SOURCES := \
	src/audio_types.c \
	src/midi_types.c \
	src/oscillator.c \
	src/envelope.c \
	src/voice.c \
	src/filter.c \
	src/synth.c \
	src/lfo.c

DESKTOP_SOURCES := \
	platform/desktop/main.c \
	platform/desktop/audio/audio_miniaudio.c \
	platform/desktop/midi/midi_portmidi.c

SOURCES := $(CORE_SOURCES) $(DESKTOP_SOURCES)

.PHONY: all run test clean

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(OSCILLATOR_TEST_TARGET) $(ENVELOPE_TEST_TARGET) $(FILTER_TEST_TARGET) $(VOICE_TEST_TARGET) $(MIDI_TYPES_TEST_TARGET)
	./$(OSCILLATOR_TEST_TARGET)
	./$(ENVELOPE_TEST_TARGET)
	./$(FILTER_TEST_TARGET)
	./$(VOICE_TEST_TARGET)
	./$(MIDI_TYPES_TEST_TARGET)

$(TARGET): $(SOURCES) third_party/miniaudio/miniaudio.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDLIBS)

$(OSCILLATOR_TEST_TARGET): tests/test_oscillator.c src/oscillator.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(ENVELOPE_TEST_TARGET): tests/test_envelope.c src/envelope.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(FILTER_TEST_TARGET): tests/test_filter.c src/filter.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(VOICE_TEST_TARGET): tests/test_voice.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(MIDI_TYPES_TEST_TARGET): tests/test_midi_types.c src/midi_types.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

build:
	mkdir -p build

clean:
	rm -rf build
