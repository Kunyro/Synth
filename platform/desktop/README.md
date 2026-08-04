# Desktop Host

The desktop host connects the portable synth engine to realtime audio and MIDI.
It lives under `platform/desktop/` so the core engine can stay independent of
desktop-only dependencies.

## Components

| Path | Purpose |
| --- | --- |
| `main.c` | Desktop app entry point and command-line parsing |
| `audio/` | miniaudio device adapter |
| `midi/` | PortMidi runtime loader, MIDI input transport, and controller mapping |
| `system/` | Small OS adapter for console polling and sleeping |

## Build And Run

Configure the development build:

```sh
cmake --preset dev
```

Build the desktop synth:

```sh
cmake --build --preset dev --target synth
```

Run until Enter is pressed:

```sh
cmake --build --preset dev --target run-synth
```

Run direct executable commands from the repository root so MIDI config paths
resolve relative to `config/midi/`.

With the default single-config preset, the executable is `build/dev/synth`. With
the Visual Studio preset, the debug executable is `build/vs2022/Debug/synth.exe`.
The examples below use the default development preset.

Run a MIDI note for a fixed number of seconds:

```sh
./build/dev/synth 69 2
```

Run a direct frequency:

```sh
./build/dev/synth freq:440 2
```

Try waveform, filter cutoff, and filter pole arguments:

```sh
./build/dev/synth 45 2 saw 1200
./build/dev/synth 45 2 square 900
./build/dev/synth freq:2000 1 saw 100 4
```

## Arguments

Options come before positional arguments:

```sh
./build/dev/synth --midi-config config/midi/akai_mpk_mini_mk2.conf
./build/dev/synth --midi-config=path/to/controller.conf
./build/dev/synth --no-midi-config
./build/dev/synth --portmidi-path /custom/path/libportmidi.dylib
```

After any options, positional arguments are:

| Argument | Description |
| --- | --- |
| `midi_note` | MIDI note number |
| `freq:<hz>` | Direct oscillator frequency, for example `freq:440` |
| `silence` | Start without a test note |
| `seconds` | Optional run duration; omit it to stop with Enter |
| `waveform` | Optional `sine`, `saw`, or `square` |
| `filter_cutoff_hz` | Optional low-pass cutoff |
| `filter_poles` | Optional pole count after cutoff, clamped to `1..8` |

With no note or frequency argument, the app plays MIDI note `57` only when no
MIDI input source is detected. If a MIDI input source is connected, the app
starts silent and waits for MIDI notes.

## Audio

The desktop app uses miniaudio for playback. The engine renders into planar
stereo buffers with `synth_render_stereo()`, and the desktop adapter writes
those frames to the audio device.

## MIDI

MIDI is split into three layers:

- PortMidi layer: transport only. It loads PortMidi at runtime, opens input
  streams, unpacks short messages into raw bytes, and forwards those bytes.
- MIDI parser: translation. It parses short MIDI packets into note on, note off,
  and pitch bend messages.
- Synth engine: musical behavior. It decides how notes and pitch bend affect
  active voices.

PortMidi is loaded dynamically at runtime, so the project still builds when
PortMidi is not installed. On macOS, Homebrew PortMidi is found from common
paths such as `/opt/homebrew/lib/libportmidi.dylib` and
`/usr/local/lib/libportmidi.dylib`.

Optional install on macOS:

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
available PortMidi library, MIDI input is disabled.

Controller mapping details live in [config/midi/README.md](../../config/midi/README.md).
