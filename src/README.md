# Synth Engine

The portable synth engine lives in `src/` with public headers in
`../include/synth/`. It has no dependency on MIDI, config syntax, controller layouts, audio backends,
or operating-system APIs. Adapters feed it musical note/control operations and
ask it to render audio buffers. All adapters depend on the engine.

## Main Modules

| Path | Purpose |
| --- | --- |
| `synth.c` | Main synth state, voice control, and base parameter setters |
| `synth_render.c` | Shared stereo/mono rendering pipeline |
| `parameter.c` | Immutable parameter metadata, base access, and effective control resolution |
| `modulation.c` | Signed route amounts and common modulation evaluation |
| `voice.c` | Per-voice note state and oscillator/envelope behavior |
| `oscillator.c` | Waveform selection and oscillator rendering |
| `wavetable.c` | Spectral bandlimited wavetable generation and lookup |
| `envelope.c` | ADSR envelope implementation |
| `filter.c` | Chained one-pole low-pass filter |
| `lfo.c` | Global morphable LFO |
| `pitch.c` | Equal-tempered note-number to frequency conversion |
| `effects/` | Saturation, distortion, bitcrusher, flanger, ring mod, chorus, EQ, delay, plate reverb, compressor, and effect-chain code |
| `internal/` | Private headers used by the engine implementation |

## Public API

Most engine users should start with `../include/synth/synth.h`.

Important entry points:

- `synth_init()` initializes a synth with defaults; call `synth_uninit()` before discarding it.
- `synth_note_on()`, `synth_note_off()`, and `synth_all_notes_off()` manage numbered musical notes.
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
- Flanger with rate, intensity, depth, feedback, manual delay, and wet/dry mix
- Ring modulator with sine frequency, modulator rectification, and wet/dry mix
- Juno-style multi-voice chorus with rate, depth, width, delay, feedback, and wet/dry mix
- Three-band EQ with low shelf, mid bell, and high shelf gains
- Delay with time, feedback, and wet/dry mix
- Plate reverb with decay, damping, predelay, and wet/dry mix
- Linked RMS compressor with threshold, ratio, makeup gain, attack, and release

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

The LFO has 52 destinations: every actual synth parameter exposed by the desktop
config, including all ten effects. Global LFO controls, chord mode, selectors,
and controller macros are excluded. All route amounts and global depth start
at zero.

```c
synth_set_delay_mix(&instrument, 0.5f);
synth_set_lfo_rate(&instrument, 2.0f);
synth_set_lfo_amount(&instrument, SYNTH_PARAM_DELAY_MIX, -0.5f);
synth_set_lfo_depth(&instrument, 0.8f);
```

Amounts range from `-1` to `1`. A negative amount reverses direction. Each
parameter has a fixed excursion in its linear or logarithmic domain, multiplied
by the signed amount and global depth. Getters always return stored base values.

The immutable catalog in `parameter.h` / `parameter.c` exposes IDs, names, units,
bounds, types, and modulation spans. Generic `synth_get_parameter()` and
`synth_set_parameter()` use the same base settings as the named API. No `.def`
file, generator, or separate mutable parameter store is involved. A small
stack-local frame carries effective controls into each module's typed
`process_with_params()` API; persistent DSP history stays in the module.

Octave/semitone tuning, filter poles, and bit depth snap independently to steps.
Filter topology transitions blend over 2 ms; tuning preserves oscillator phase
without pitch glide. The bitcrusher keeps its clock and updates quantization
on its next sampling tick. ADSR modulation is captured at note-on, including
release; explicit manual ADSR edits retain their existing whole-envelope
replacement behavior. Delay-time modulation moves fractional read heads and
produces pitch bends. Flanger intensity contributes before direct depth and
feedback routes, with final clamping afterward.

See [the modulation contract](../docs/modulation.md) for all destinations,
spans, state behavior, config examples, and API migration details.

## Hosting the Engine

Configure with `SYNTH_BUILD_DESKTOP=OFF` to build and test only the portable
engine. Hosts provide sample rate, buffers, and synchronization; initialize and
release resource-owning modules outside the audio callback. Parameter changes
and rendering on the same synth instance must be serialized by the host.

MIDI packet parsing is in `platform/desktop/midi/midi_types.c`. That adapter
normalizes pitch bend to `-1..1` and passes musical operations to the engine.
The existing engine bend span remains one semitone in either direction.
