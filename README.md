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
- Saturation, distortion, bitcrusher, and delay
- Desktop audio playback through miniaudio
- Optional MIDI input and controller mapping through runtime-loaded PortMidi

## Quick Start

Configure the development build:

```sh
cmake --preset dev
```

Build the desktop synth, MIDI monitor, and tests:

```sh
cmake --build --preset dev
```

Start the desktop app from the repository root:

```sh
cmake --build --preset dev --target run-synth
```

With no MIDI controller connected, the app plays a default test note until you
press Enter. With a MIDI input source connected, the app starts silent and waits
for MIDI notes.

Bind MIDI controls:

```sh
cmake --build --preset dev --target midi_monitor
./build/dev/midi_monitor learn --output config/midi/my_keyboard.conf
./build/dev/synth --midi-config config/midi/my_keyboard.conf
```

Run commands from the repository root so relative config paths resolve to
`config/midi/`.

In the learn prompt, type `list` to see synth parameters, then use
`bind <name|number>` and move a knob, fader, or other control on your MIDI
keyboard. Type `save` when you are done. More detail is available in
[MIDI controller mapping](config/midi/README.md).

## Requirements

- A C99 compiler
- CMake 3.21 or newer
- macOS, Linux, or Windows with a CMake-supported C compiler
- Windows: Visual Studio 2022 is supported through the `vs2022` preset
- Optional: PortMidi for MIDI input

miniaudio is vendored under `third_party/miniaudio/`. PortMidi is loaded
dynamically at runtime, so the project still builds when PortMidi is not
installed.

On macOS, PortMidi can be installed with:

```sh
brew install portmidi
```

On Windows, copy `portmidi.dll` into the same folder as `synth.exe`, or pass
`--portmidi-path`. With the Visual Studio preset, the debug executable lives
under `build/vs2022/Debug/`.

On macOS or Windows, pass `--portmidi-path` followed by the full path of the
PortMidi library file:

```sh
# macOS
./build/dev/synth --portmidi-path /custom/path/libportmidi.dylib
```

```powershell
# Windows PowerShell
.\build\vs2022\Debug\synth.exe --portmidi-path "C:\custom\path\portmidi.dll"
```

The `PORTMIDI_PATH` environment variable remains available as an alternative.
A command-line path takes precedence over the environment variable. The custom
path is tried first, followed by the normal platform locations. Without an
available PortMidi library, the synth still runs, but MIDI input is disabled.

## Common Commands

```sh
cmake --preset dev                         # configure a debug build
cmake --build --preset dev                 # build app, tools, and tests
ctest --preset dev                         # run the test suite
cmake --build --preset dev --target run-synth
cmake --build --preset dev --target run-midi-monitor
cmake --preset release                     # configure an optimized build
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
| `tests/` | C test programs run through CTest |
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
