# Synth Engine

The portable synth engine lives in `src/` with public headers in
`../include/synth/`. It does not depend on miniaudio or PortMidi; platform code
feeds it note/control events and asks it to render audio buffers.

## Main Modules

| Path | Purpose |
| --- | --- |
| `synth.c` | Main synth state, voice control, parameter setters, rendering |
| `voice.c` | Per-voice note state and oscillator/envelope behavior |
| `oscillator.c` | Waveform selection and oscillator rendering |
| `wavetable.c` | Spectral bandlimited wavetable generation and lookup |
| `envelope.c` | ADSR envelope implementation |
| `filter.c` | Chained one-pole low-pass filter |
| `lfo.c` | Global morphable LFO |
| `midi_types.c` | Portable MIDI note, note off, and pitch bend parsing |
| `effects/` | Saturation, distortion, bitcrusher, delay, and effect-chain code |
| `internal/` | Private headers used by the engine implementation |

## Public API

Most engine users should start with `../include/synth/synth.h`.

Important entry points:

- `synth_init()` initializes a synth with defaults.
- `synth_note_on()`, `synth_note_off()`, and `synth_all_notes_off()` manage MIDI notes.
- `synth_note_on_frequency()` starts a direct-frequency voice.
- `synth_set_*()` functions update envelope, oscillator, filter, LFO, and effect parameters.
- `synth_render_stereo()` renders planar stereo audio.
- `synth_render_mono()` renders mono audio.

Project-wide defaults live in `../include/synth/synth_config.h`.

## Signal Path

At a high level, each active voice renders oscillators through the envelope.
The mixed voice output then passes through the filter and post-filter effects
before master gain is applied.

Current post-filter effects:

- Saturation with drive, even-harmonic emphasis, and wet/dry mix
- Distortion with drive and wet/dry mix
- Bitcrusher with reduced sample rate, bit depth, and wet/dry mix
- Delay with time, feedback, and wet/dry mix

The effects default to dry or neutral settings, so existing patches render
unchanged until the relevant mix or amount is raised.

## Oscillators

The engine uses spectral bandlimited wavetables for sine, saw, and square
waveforms. Oscillator morphing moves continuously from:

```text
0.0 = sine
0.5 = saw
1.0 = square
```

The synth has first and second oscillator controls. The second oscillator can
use its own morph, level, octave offset, semitone offset, and fine-tune amount.
Stereo spread moves the first and second oscillators toward opposite channels
while preserving their mono average.

## Filter

The filter is a one-pole low-pass design with `1..8` chained poles. Each pole
adds roughly `6 dB/oct` of low-pass slope. Cutoff is clamped to the valid audio
range before rendering.

## LFO

The global LFO runs continuously, including while no voices are active. It uses
the same wavetable morph shape as the audio oscillators.

The LFO can modulate:

- First oscillator morph
- Second oscillator morph
- First oscillator volume
- Second oscillator volume
- Filter cutoff

`lfo_depth` is a master multiplier for every route amount. Morph modulation
moves each oscillator around its stored base morph. Volume modulation moves down
from each stored base gain. Filter modulation moves the stored cutoff
exponentially by up to eight octaves in either direction. Render-time modulation
does not overwrite the underlying knob settings.

## MIDI Parsing

`midi_types.c` contains the portable MIDI parser used by desktop MIDI input and
tests. It parses short MIDI packets into note on, note off, and pitch bend
messages.

Pitch bend messages are normalized to `-1.0..+1.0`. A full-up message such as
`e0 7f 7f` becomes value `8191`, normalized to `+1.0`. The synth maps the wheel
to a two-semitone span: `-1` semitone at full down, center at `0`, and `+1`
semitone at full up.
