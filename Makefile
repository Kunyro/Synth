CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude -Iplatform/desktop -Ithird_party
LDLIBS ?= -lm

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
HOST_OS := $(UNAME_S)
EXEEXT :=

ifeq ($(OS),Windows_NT)
HOST_OS := Windows
endif
ifneq (,$(findstring MINGW,$(UNAME_S)))
HOST_OS := Windows
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
HOST_OS := Windows
endif

ifeq ($(HOST_OS),Darwin)
LDLIBS += -lpthread -framework CoreFoundation -framework CoreAudio -framework AudioToolbox -framework AudioUnit
else ifeq ($(HOST_OS),Linux)
LDLIBS += -lpthread -ldl
else ifeq ($(HOST_OS),Windows)
EXEEXT := .exe
endif

TARGET := build/synth$(EXEEXT)
MIDI_MONITOR_TARGET := build/midi_monitor$(EXEEXT)
OSCILLATOR_TEST_TARGET := build/test_oscillator$(EXEEXT)
ENVELOPE_TEST_TARGET := build/test_envelope$(EXEEXT)
FILTER_TEST_TARGET := build/test_filter$(EXEEXT)
DISTORTION_TEST_TARGET := build/test_distortion$(EXEEXT)
SATURATION_TEST_TARGET := build/test_saturation$(EXEEXT)
BITCRUSHER_TEST_TARGET := build/test_bitcrusher$(EXEEXT)
COMPRESSOR_TEST_TARGET := build/test_compressor$(EXEEXT)
FLANGER_TEST_TARGET := build/test_flanger$(EXEEXT)
CHORUS_TEST_TARGET := build/test_chorus$(EXEEXT)
RING_MOD_TEST_TARGET := build/test_ring_mod$(EXEEXT)
EQ_TEST_TARGET := build/test_eq$(EXEEXT)
DELAY_TEST_TARGET := build/test_delay$(EXEEXT)
PLATE_REVERB_TEST_TARGET := build/test_plate_reverb$(EXEEXT)
MODULATION_TEST_TARGET := build/test_modulation$(EXEEXT)
PITCH_TEST_TARGET := build/test_pitch$(EXEEXT)
LFO_TEST_TARGET := build/test_lfo$(EXEEXT)
VOICE_TEST_TARGET := build/test_voice$(EXEEXT)
MIDI_TYPES_TEST_TARGET := build/test_midi_types$(EXEEXT)
LFO_MAPPING_TEST_TARGET := build/test_lfo_mapping$(EXEEXT)
MIDI_MAPPING_TEST_TARGET := build/test_midi_mapping$(EXEEXT)
CHORD_MODE_TEST_TARGET := build/test_chord_mode$(EXEEXT)

CORE_SOURCES := $(shell sed -e '/^[[:space:]]*\#/d' -e '/^[[:space:]]*$$/d' cmake/synth_core_sources.txt)
DESKTOP_AUDIO_SOURCES := $(shell sed -e '/^[[:space:]]*\#/d' -e '/^[[:space:]]*$$/d' cmake/synth_desktop_audio_sources.txt)
DESKTOP_MIDI_SOURCES := $(shell sed -e '/^[[:space:]]*\#/d' -e '/^[[:space:]]*$$/d' cmake/synth_desktop_midi_sources.txt)
DESKTOP_SYSTEM_SOURCES := $(shell sed -e '/^[[:space:]]*\#/d' -e '/^[[:space:]]*$$/d' cmake/synth_desktop_system_sources.txt)
MIDI_MAPPING_SOURCES := \
	platform/desktop/midi/chord_mode.c \
	platform/desktop/midi/midi_mapping.c \
	platform/desktop/midi/midi_mapping_io.c \
	platform/desktop/midi/midi_mapping_validation.c \
	platform/desktop/midi/midi_mapping_registry.c \
	platform/desktop/midi/midi_mapping_runtime.c \
	platform/desktop/midi/midi_text.c

DESKTOP_SOURCES := platform/desktop/main.c $(DESKTOP_AUDIO_SOURCES) $(DESKTOP_MIDI_SOURCES) $(DESKTOP_SYSTEM_SOURCES)

SOURCES := $(CORE_SOURCES) $(DESKTOP_SOURCES)

.PHONY: all run midi-monitor test clean

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

midi-monitor: $(MIDI_MONITOR_TARGET)
	./$(MIDI_MONITOR_TARGET)

test: $(LFO_MAPPING_TEST_TARGET) $(MODULATION_TEST_TARGET) $(PITCH_TEST_TARGET) $(OSCILLATOR_TEST_TARGET) $(ENVELOPE_TEST_TARGET) $(FILTER_TEST_TARGET) $(DISTORTION_TEST_TARGET) $(SATURATION_TEST_TARGET) $(BITCRUSHER_TEST_TARGET) $(COMPRESSOR_TEST_TARGET) $(FLANGER_TEST_TARGET) $(CHORUS_TEST_TARGET) $(RING_MOD_TEST_TARGET) $(EQ_TEST_TARGET) $(DELAY_TEST_TARGET) $(PLATE_REVERB_TEST_TARGET) $(LFO_TEST_TARGET) $(VOICE_TEST_TARGET) $(MIDI_TYPES_TEST_TARGET) $(MIDI_MAPPING_TEST_TARGET) $(CHORD_MODE_TEST_TARGET)
	./$(LFO_MAPPING_TEST_TARGET)
	./$(MODULATION_TEST_TARGET)
	./$(PITCH_TEST_TARGET)
	./$(OSCILLATOR_TEST_TARGET)
	./$(ENVELOPE_TEST_TARGET)
	./$(FILTER_TEST_TARGET)
	./$(DISTORTION_TEST_TARGET)
	./$(SATURATION_TEST_TARGET)
	./$(BITCRUSHER_TEST_TARGET)
	./$(COMPRESSOR_TEST_TARGET)
	./$(FLANGER_TEST_TARGET)
	./$(CHORUS_TEST_TARGET)
	./$(RING_MOD_TEST_TARGET)
	./$(EQ_TEST_TARGET)
	./$(DELAY_TEST_TARGET)
	./$(PLATE_REVERB_TEST_TARGET)
	./$(LFO_TEST_TARGET)
	./$(VOICE_TEST_TARGET)
	./$(MIDI_TYPES_TEST_TARGET)
	./$(MIDI_MAPPING_TEST_TARGET)
	./$(CHORD_MODE_TEST_TARGET)

$(TARGET): $(SOURCES) third_party/miniaudio/miniaudio.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDLIBS)

$(MIDI_MONITOR_TARGET): tools/midi_monitor.c $(DESKTOP_MIDI_SOURCES) $(DESKTOP_SYSTEM_SOURCES) $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(OSCILLATOR_TEST_TARGET): tests/test_oscillator.c src/oscillator.c src/wavetable.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(ENVELOPE_TEST_TARGET): tests/test_envelope.c src/envelope.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(FILTER_TEST_TARGET): tests/test_filter.c src/filter.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(DISTORTION_TEST_TARGET): tests/test_distortion.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(SATURATION_TEST_TARGET): tests/test_saturation.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(BITCRUSHER_TEST_TARGET): tests/test_bitcrusher.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(COMPRESSOR_TEST_TARGET): tests/test_compressor.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(FLANGER_TEST_TARGET): tests/test_flanger.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(CHORUS_TEST_TARGET): tests/test_chorus.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(RING_MOD_TEST_TARGET): tests/test_ring_mod.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(EQ_TEST_TARGET): tests/test_eq.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(DELAY_TEST_TARGET): tests/test_delay.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(PLATE_REVERB_TEST_TARGET): tests/test_plate_reverb.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(LFO_TEST_TARGET): tests/test_lfo.c src/lfo.c src/wavetable.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(VOICE_TEST_TARGET): tests/test_voice.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(MIDI_TYPES_TEST_TARGET): tests/test_midi_types.c platform/desktop/midi/midi_types.c src/pitch.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(MIDI_MAPPING_TEST_TARGET): tests/test_midi_mapping.c $(MIDI_MAPPING_SOURCES) $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(CHORD_MODE_TEST_TARGET): tests/test_chord_mode.c platform/desktop/midi/chord_mode.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

build:
	mkdir -p build

clean:
	rm -rf build

$(MODULATION_TEST_TARGET): tests/test_modulation.c $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(PITCH_TEST_TARGET): tests/test_pitch.c src/pitch.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

$(LFO_MAPPING_TEST_TARGET): tests/test_lfo_mapping.c $(MIDI_MAPPING_SOURCES) $(CORE_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm
