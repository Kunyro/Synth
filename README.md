# Synth

A small portable C synth engine with a desktop host for realtime audio and MIDI control.

The core synth lives in `include/synth/` and `src/`. Desktop audio, MIDI input,
controller mappings, and runtime library loading live under `platform/desktop/`
so the engine can stay independent of miniaudio and PortMidi.

## Project Layout

- `include/synth/`: public synth engine headers
- `src/`: portable synth engine implementation
- `src/internal/`: private engine helpers and wavetable internals
- `platform/desktop/audio/`: miniaudio desktop audio adapter
- `platform/desktop/midi/`: PortMidi transport and MIDI CC mapping layer
- `config/midi/`: controller mapping files
- `tools/midi_monitor.c`: MIDI inspection utility
- `tests/`: C test programs built by `make test`
- `third_party/miniaudio/`: vendored miniaudio header
- `third_party/portmidi/`: reserved for an optional local PortMidi copy
- `platform/teensy/`: reserved for a future Teensy 4.1 port

`include/synth/lfo.h`, `src/lfo.c`, and `tools/render_wav.c` are currently
empty placeholders.

## Build And Test

Build the desktop synth:

```sh
make
```

Run the test suite:

```sh
make test
```

This builds and runs tests for the oscillator, envelope, filter, voice/synth
behavior, MIDI parsing, and MIDI mapping.

Clean build artifacts:

```sh
make clean
```

## Desktop Audio

The desktop app uses miniaudio for playback. The synth engine renders into planar stereo buffers with `synth_render_stereo()`. The current signal is centered, so left and right receive the same sample until panning or stereo imaging is added.

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

Positional arguments, after any MIDI config flags:

- `midi_note`: MIDI note number
- `freq:<hz>`: direct oscillator frequency, for example `freq:440`
- `silence`: start without a test note
- `seconds`: optional run duration; omit it to stop with Enter
- `waveform`: optional `sine`, `saw`, or `square`
- `filter_cutoff_hz`: optional low-pass cutoff
- `filter_poles`: optional pole count after cutoff, clamped to `1..8`

With no note or frequency argument, the app plays MIDI note `57` only when no
MIDI input source is detected. If a MIDI input source is connected, the app
starts silent and waits for MIDI notes.

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

Build and run the MIDI monitor:

```sh
make midi-monitor
```

Touch one controller at a time and the monitor prints the incoming MIDI message,
including channel, message type, values, and raw bytes. It recognizes note
on/off, control change, pitch bend, program change, channel pressure, poly
pressure, common system messages, and realtime messages.

Pitch bend messages are parsed by the portable MIDI parser. A full-up message such as `e0 7f 7f` becomes value `8191`, normalized to `+1.0`. The synth maps the wheel to a two-semitone span: `-1` semitone at full down, center at `0`, and `+1` semitone at full up.

## MIDI Controller Mapping

Controller config files live in `config/midi/`. The default desktop config is:

```text
config/midi/akai_mpk_mini_mk2.conf
```

It maps Akai MPK Mini MK2-style CC knobs on channel 1:

- CC 1: attack
- CC 2: decay
- CC 3: sustain
- CC 4: release
- CC 5: filter cutoff
- CC 6: filter poles
- CC 7: oscillator morph
- CC 8: master gain

Run with another config, or disable config mapping:

```sh
./build/synth --midi-config config/midi/akai_mpk_mini_mk2.conf
./build/synth --midi-config=path/to/controller.conf
./build/synth --no-midi-config
```

Config lines use:

```text
parameter=cc:channel:control:scale:min:max
```

Supported parameters:

- `attack`
- `decay`
- `sustain`
- `release`
- `master_gain`
- `filter_cutoff`
- `filter_poles`
- `oscillator_morph`

Supported scales:

- `linear`
- `log`
- `step`

Example:

```text
filter_cutoff=cc:1:5:log:20.0:20000.0
```

CC mappings use soft takeover: a knob must reach or cross the current synth
value before it starts changing that parameter.

## Engine Features

Implemented so far:

- 8-voice polyphony
- MIDI note to frequency conversion
- note on, note off, all notes off, and direct-frequency note APIs
- ADSR envelope
- master gain
- pitch bend with a `-1..+1` semitone range
- spectral bandlimited wavetable oscillator
- sine, saw, and square waveform presets
- continuous oscillator morph from sine to saw to square
- one-pole low-pass filter with `1..8` chained poles
- planar stereo render API and mono render helper
- short MIDI parser for notes and pitch bend
- desktop MIDI input through runtime-loaded PortMidi
- config-driven MIDI CC mapping for synth parameters

The filter cutoff is clamped to the valid audio range. Each filter pole adds
roughly `6 dB/oct` of low-pass slope.
