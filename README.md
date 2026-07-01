# Synth
Synth engine written in C

## Synth engine roadmap

This project uses [miniaudio](https://miniaud.io/) for desktop audio output, while
the actual synth engine lives in plain C files that do not depend on miniaudio:

- `include/synth/` contains the public synth engine headers
- `src/` contains the portable synth engine implementation
- `platform/desktop/audio/` contains the miniaudio desktop adapter
- `platform/desktop/midi/` contains the PortMidi desktop adapter
- `platform/teensy/` is reserved for the future Teensy 4.1 port
- `third_party/miniaudio/` contains the vendored miniaudio header
- `third_party/portmidi/` is reserved for optional PortMidi vendoring

Build:

```sh
make
```

Run portable engine tests:

```sh
make test
```

Run until Enter is pressed:

```sh
make run
```

Run a specific MIDI note for a fixed number of seconds:

```sh
./build/synth 69 2
```

Run a direct frequency:

```sh
./build/synth freq:440 2
```

Try saw or square with an optional low-pass cutoff:

```sh
./build/synth 45 2 saw 1200
./build/synth 45 2 square 900
```

Run silence through miniaudio:

```sh
./build/synth silence 2
```

Arguments:

- `midi_note`: MIDI note number, default `57` (`220 Hz`)
- `freq:<hz>`: direct oscillator frequency, for example `freq:440`
- `seconds`: optional run duration; omit it to stop with Enter
- `waveform`: optional `sine`, `saw`, or `square`
- `filter_cutoff_hz`: optional one-pole low-pass cutoff

Implemented so far:

- miniaudio playback device setup
- silence output path
- sine oscillator
- MIDI note number to frequency conversion
- note on / note off API
- ADSR envelope
- 8 voice polyphony in the portable engine
- saw and square waveform functions in the portable engine
- simple one-pole low-pass filter
- MIDI controller input through PortMidi when `libportmidi` is installed
- portable tests for oscillator, envelope, voice, and MIDI type behavior

PortMidi is loaded dynamically, so the project still builds when the library is
not installed. MIDI input activates automatically when PortMidi is available at
runtime.
