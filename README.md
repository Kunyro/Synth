# Synth

A portable C synth engine with a desktop host for realtime audio and MIDI control.

The core synth lives in `include/synth/` and `src/`. Desktop audio and MIDI input
controller mappings live under `platform/desktop/`
so the engine can stay independent of miniaudio and PortMidi.

## Project Layout

- `include/synth/`: public synth engine headers
- `src/`: portable synth engine implementation
- `src/effects/`: portable post-filter effect implementations
- `src/internal/`: private engine helpers and wavetable internals
- `platform/desktop/audio/`: miniaudio desktop audio adapter
- `platform/desktop/midi/`: PortMidi transport and MIDI CC mapping layer
- `config/midi/`: controller mapping files
- `tools/midi_monitor.c`: MIDI inspection utility
- `tests/`: C test programs built by `make test`
- `third_party/`: reserved for third_party libraries
- `platform/teensy/`: reserved for a future Teensy 4.1 port

## Build And Test

Build the desktop synth:

```sh
make
```

Run the test suite:

```sh
make test
```

This builds and runs tests for the oscillator, LFO, envelope, filter, distortion,
delay, voice/synth behavior, MIDI parsing, and MIDI mapping.

Clean build artifacts:

```sh
make clean
```

## Desktop Audio

The desktop app uses miniaudio for playback. The synth engine renders into planar stereo buffers with `synth_render_stereo()`. Its stereo spread control moves the first and second oscillators toward opposite channels while keeping their mono average unchanged.

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
- CC 9: second oscillator octave
- CC 10: second oscillator pitch
- CC 11: second oscillator fine tune
- CC 12: stereo spread
- CC 13: first oscillator volume
- CC 14: second oscillator volume
- CC 15: second oscillator morph
- CC 16: master gain
- CC 17: first oscillator LFO volume amount
- CC 18: second oscillator LFO volume amount
- CC 19: first oscillator morph LFO amount
- CC 20: second oscillator morph LFO amount
- CC 21: LFO rate
- CC 22: global LFO depth
- CC 23: filter LFO amount
- CC 24: LFO shape morph
- CC 25: distortion dry/wet
- CC 26: distortion drive
- CC 27: delay dry/wet
- CC 28: delay time
- CC 29: delay feedback

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
- `first_oscillator_gain`
- `second_oscillator_gain`
- `second_oscillator_morph`
- `second_oscillator_octave`
- `second_oscillator_pitch`
- `second_oscillator_fine_tune`
- `stereo_spread`
- `lfo_rate`
- `lfo_shape_morph`
- `lfo_depth`
- `lfo_first_oscillator_morph_amount`
- `lfo_second_oscillator_morph_amount`
- `lfo_first_oscillator_gain_amount`
- `lfo_second_oscillator_gain_amount`
- `lfo_filter_amount`
- `distortion_drive`
- `distortion_mix`
- `delay_time`
- `delay_feedback`
- `delay_mix`

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
- continuous global morphable LFO with morph, oscillator volume, and filter routes
- post-filter distortion with drive and wet/dry mix
- post-distortion delay with time, feedback, and wet/dry mix

The filter cutoff is clamped to the valid audio range. Each filter pole adds
roughly `6 dB/oct` of low-pass slope.

The global LFO runs continuously, including while no voices are active. Its
shape uses the same spectral wavetable morph as the audio oscillators:
`0.0 = sine`, `0.5 = saw`, and `1.0 = square`. Its depth is a master multiplier
for every route amount. Morph modulation moves both oscillator morph positions
independently around their stored base values, oscillator volume modulation
moves down from each stored base gain, and filter modulation moves the stored
cutoff exponentially by up to eight octaves in either direction. Render-time
modulation does not overwrite the underlying knob settings.

Distortion runs after the synth filter and before final master gain. The effect
defaults to a dry mix, so existing patches render unchanged until distortion mix
is raised.

Delay runs after distortion and before final master gain. Its time is clamped to
the available delay line, feedback stays below unity, and the effect defaults to
a dry mix.

## Know Bugs
