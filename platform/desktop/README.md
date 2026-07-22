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

## Build And Run

Build the app:

```sh
make
```

Run until Enter is pressed:

```sh
make run
```

Run a MIDI note for a fixed number of seconds:

```sh
./build/synth 69 2
```

Run a direct frequency:

```sh
./build/synth freq:440 2
```

Try waveform, filter cutoff, and filter pole arguments:

```sh
./build/synth 45 2 saw 1200
./build/synth 45 2 square 900
./build/synth freq:2000 1 saw 100 4
```

## Arguments

MIDI config flags come first:

```sh
./build/synth --midi-config config/midi/akai_mpk_mini_mk2.conf
./build/synth --midi-config=path/to/controller.conf
./build/synth --no-midi-config
```

After any MIDI config flags, positional arguments are:

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

Controller mapping details live in [config/midi/README.md](../../config/midi/README.md).
