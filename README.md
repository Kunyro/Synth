# Synth

Synth engine written in C

## Synth engine roadmap

This project uses [miniaudio](https://miniaud.io/) for desktop audio output, but
the actual synth engine does not depend on miniaudio:

- `include/synth/` contains the public synth engine headers
- `src/` contains the portable synth engine implementation
- `platform/desktop/audio/` contains the miniaudio desktop adapter
- `platform/desktop/midi/` contains the PortMidi desktop adapter
- `platform/teensy/` is reserved for the future Teensy 4.1 port
- `third_party/miniaudio/` contains the miniaudio header
- `third_party/portmidi/` is reserved for optional PortMidi implementation

Build:

```sh
make
```

Install optional desktop MIDI support:

```sh
brew install portmidi
```

PortMidi is loaded dynamically at runtime. If it is installed with Homebrew on
macOS, the desktop app should find it automatically from `/opt/homebrew/lib` or
`/usr/local/lib`. If no MIDI device is connected, the app will still run and
print that no MIDI input devices were found.

Run portable engine tests:

```sh
make test
```

Build and run the MIDI monitor:

```sh
make midi-monitor
```

Touch one knob, pad, or button at a time and the monitor prints the incoming
MIDI message, including channel, message type, CC or note number, value, and raw
bytes. Use this to build controller mappings.

MIDI controller configs live in `config/midi/`. The default desktop config is
`config/midi/akai_mpk_mini_mk2.conf`, which currently maps CC knobs 1 through 4
on channel 1 to attack, decay, sustain, and release, CC knob 5 to filter cutoff,
CC knob 6 to filter poles, CC knob 7 to oscillator morph, and CC knob 8 to
master gain.

Run with another config or disable config mapping:

```sh
./build/synth --midi-config config/midi/akai_mpk_mini_mk2.conf
./build/synth --no-midi-config
```

Config lines use:

```text
parameter=cc:channel:control:scale:min:max
```

Supported scales are `linear`, `log`, and `step`.

For example:

```text
filter_cutoff=cc:1:5:log:20.0:20000.0
```

Run until Enter is pressed:

```sh
make run
```

With no arguments, the desktop app plays MIDI note `57` as a quick audio test
only when no MIDI input device is detected. If a MIDI input device is connected,
the app starts silent and waits for notes from the device.

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
./build/synth freq:2000 1 saw 100 4
```

Arguments:

- `midi_note`: MIDI note number, defaults to `57` (`220 Hz`) only when no MIDI input device is detected
- `freq:<hz>`: direct oscillator frequency, for example `freq:440`
- `seconds`: optional run duration; omit it to stop with Enter
- `waveform`: optional `sine`, `saw`, or `square`
- `filter_cutoff_hz`: optional low-pass cutoff
- `filter_poles`: optional pole count after cutoff, clamped to `1..8`; each pole adds about `6 dB/oct`

Implemented so far:

- miniaudio playback device setup
- spectral bandlimited wavetable oscillator with sine-to-saw-to-square morph tables
- MIDI note number to frequency conversion
- note on / note off API
- ADSR envelope
- 8 voice polyphony
- stereo audio buffer and desktop output path
- sine, saw and square waveform presets
- low-pass filter with configurable pole count
- MIDI controller input through PortMidi when `libportmidi` is installed
- tests for oscillator, envelope, voice, and MIDI type behavior

PortMidi is loaded dynamically, so the project still builds when the library is
not installed. MIDI input activates automatically when PortMidi is available at
runtime.
