# Synth

A portable C synth engine with a desktop host for realtime audio and MIDI control.

Synth is organized around a reusable engine in `include/synth/` and `src/`, plus
a desktop app in `platform/desktop/` that connects the engine to miniaudio and
PortMidi. The goal is to keep the audio engine portable while still making it
easy to play, test, and experiment from a regular computer.

## Highlights

- Portable C99 synth engine
- 8-voice polyphony
- ADSR envelope, pitch bend, master gain, and MIDI note handling
- Spectral bandlimited wavetable oscillator
- Sine, saw, square, and continuous waveform morphing
- Two oscillator controls with level, morph, pitch, fine-tune, octave, and stereo spread
- Low-pass filter with variable intensity
- Global morphable LFO
- Distortion, bitcrusher, and delay
- Desktop audio playback through miniaudio
- Optional MIDI input and controller mapping through runtime-loaded PortMidi

## Quick Start

Build the desktop synth:

```sh
make
```

Start the desktop app:

```sh
make run
```

With no MIDI controller connected, `make run` plays a default test note until
you press Enter. With a MIDI input source connected, the app starts silent and
waits for MIDI notes.

Bind MIDI controls:

```sh
make midi-monitor
./build/midi_monitor learn --output config/midi/my_keyboard.conf
./build/synth --midi-config config/midi/my_keyboard.conf
```

In the learn prompt, type `list` to see synth parameters, then use
`bind <name|number>` and move a knob, fader, or other control on your MIDI
keyboard. Type `save` when you are done. More detail is available in
[MIDI controller mapping](config/midi/README.md).

## Requirements

- A C99 compiler
- `make`
- macOS or Linux for the desktop host
- Optional: PortMidi for MIDI input

miniaudio is vendored under `third_party/miniaudio/`. PortMidi is loaded
dynamically at runtime, so the project still builds when PortMidi is not
installed.

On macOS, PortMidi can be installed with:

```sh
brew install portmidi
```

## Common Commands

```sh
make              # build build/synth
make run          # build and run the desktop synth
make test         # build and run the test suite
make midi-monitor # build and run the MIDI monitor
make clean        # remove build artifacts
```

## Project Layout

| Path | Purpose |
| --- | --- |
| `include/synth/` | Public synth engine headers |
| `src/` | Portable synth engine implementation |
| `src/effects/` | Portable post-filter effects |
| `src/internal/` | Private engine helpers and wavetable internals |
| `platform/desktop/` | Desktop audio app, miniaudio adapter, and MIDI input |
| `config/midi/` | Controller mapping files |
| `tools/` | Developer utilities such as the MIDI monitor |
| `tests/` | C test programs built by `make test` |
| `third_party/` | Vendored or reserved third-party dependencies |
| `platform/teensy/` | Reserved for a future Teensy 4.1 port |

## Documentation

- [Engine notes](src/README.md)
- [Desktop app usage](platform/desktop/README.md)
- [MIDI controller mapping](config/midi/README.md)
- [Tools](tools/README.md)
- [Tests](tests/README.md)

## Status

The synth currently focuses on a portable engine, a simple desktop host, and
MIDI-driven experimentation. Future platform work can build on the same engine
without pulling desktop audio or MIDI dependencies into the core.
