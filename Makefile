CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
CPPFLAGS ?= -I.
LDLIBS ?= -lm -lpthread

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
LDLIBS += -framework CoreFoundation -framework CoreAudio -framework AudioToolbox -framework AudioUnit
else ifeq ($(UNAME_S),Linux)
LDLIBS += -ldl
endif

TARGET := build/synth
SOURCES := engine/main.c

.PHONY: all run clean

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

$(TARGET): $(SOURCES) miniaudio.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDLIBS)

build:
	mkdir -p build

clean:
	rm -rf build
