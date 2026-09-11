# LFO modulation contract

The engine supports one global LFO with 52 independent signed destination
amounts: 15 synth parameters and 37 controls across all ten effects. Every
amount and global depth initialize to zero. The LFO is free-running and advances
once per stereo frame, including silence and effect tails. Notes do not reset
its phase. Global LFO rate, shape, and depth cannot be destinations; neither can
chord mode, effect selectors, controller macros, or route amounts themselves.

## Engine API and ownership

```c
#include "synth/synth.h"

synth instrument;
synth_init(&instrument, 48000.0f);
synth_set_parameter(&instrument, SYNTH_PARAM_DELAY_MIX, 0.5f);
synth_set_lfo_rate(&instrument, 2.0f);
synth_set_lfo_amount(&instrument, SYNTH_PARAM_DELAY_MIX, -0.5f);
synth_set_lfo_depth(&instrument, 0.8f);
synth_note_on(&instrument, 60, 0.8f);

float left[256], right[256];
synth_audio_buffer output = {left, right, 256};
synth_render_stereo(&instrument, &output);
synth_uninit(&instrument);
```

`parameter.h` defines ordinary C enum identities and immutable metadata.
`parameter.c` owns the catalog and dispatches base access through existing
setters/getters. Each setting has one authoritative stored base. Catalog
metadata includes canonical name, unit, legal range, discrete type, domain, and
fixed modulation span. IDs identify engine parameters; config syntax lives
exclusively in the desktop adapter. Enumeration runs from zero to
`SYNTH_PARAM_COUNT - 1`; metadata pointers have static lifetime.

`modulation.c` stores only route amounts and implements the shared evaluation
law. `synth_set_lfo_amount()` and `synth_set_parameter()` reject nonfinite values,
invalid identities, and null instances. Route setters additionally reject
excluded targets and clamp finite amounts to `[-1, 1]`. Getters report base
values or route amounts, never effective render values. Invalid getters return
zero. `synth_reset_lfo_amounts()` removes all routes without changing the LFO.

A stack-local typed render frame is resolved from current base getters every
sample. Its private descriptor offsets address only control fields in that
frame. Each module consumes its own `synth_*_params` subset through
`process_with_params()`. These calls advance DSP history and do not call base
setters, allocate memory, or store a second patch. Unchanged EQ/compressor
coefficients are cached in their owning modules. Standalone module processing
uses the same implementation with a copy of its stored controls.

The engine includes no MIDI parser, config loader, controller layout, audio
backend, or OS API. Packet parsing is in the desktop MIDI adapter; musical pitch
conversion is in `pitch.c`. Hosts own synchronization and call initialization
and destruction outside their audio callback. No generated source, `.def` file,
or general modulation graph is needed.

## Evaluation

Let `u = lfo_value * global_depth * signed_amount`.

- Linear destination: `effective = base + u * span`.
- Logarithmic destination: `effective = base * exp2(u * span)`; its span is in
  octaves (doublings), including positive timing parameters.

Clamp to the destination's legal bounds, then round integer destinations to
the nearest integer, with half steps away from zero. Each stepped destination
quantizes independently. Zero amount or zero depth returns the base exactly,
without logarithmic round-trip error. For example, mix base `0.5`, amount `-0.5`,
depth `0.8`, and LFO `+1` produce mix `0.3`. A cutoff amount `0.2` with depth `1`
has a one-octave excursion, so a 1 kHz base moves between 500 Hz and 2 kHz.

All five former destinations use this same rule. Gain modulation is centered,
and cutoff's fixed span is now five octaves. Clamping can flatten a waveform
near a legal boundary. There is no global smoothing that would erase the chosen
saw/square LFO shape. Continuous destinations are evaluated per sample, not per
host buffer. ADSR is the note-on exception below.

## Destination inventory

An unmarked span is in the parameter's native unit. “Octaves” in the span column
indicates logarithmic evaluation. Times permitting zero use linear evaluation.
ADSR times retain their existing unbounded nonnegative float range. Cutoff is
limited to Nyquist and bitcrusher rate to the host rate (with the module's 1 Hz
minimum); metadata's static maximum for these two controls is `FLT_MAX`.

| Canonical parameter | Unit | Legal range | Full excursion | Type |
| --- | --- | --- | --- | --- |
| `attack` | seconds | 0–FLT_MAX | 1 | continuous |
| `decay` | seconds | 0–FLT_MAX | 1 | continuous |
| `sustain` | normalized | 0–1 | 0.5 | continuous |
| `release` | seconds | 0–FLT_MAX | 1.5 | continuous |
| `master_gain` | normalized | 0–1 | 0.5 | continuous |
| `filter_cutoff` | Hz | 10–sample rate / 2 | 5 octaves | continuous |
| `filter_poles` | poles | 1–8 | 3.5 | integer |
| `oscillator_morph` | normalized | 0–1 | 0.5 | continuous |
| `first_oscillator_gain` | normalized | 0–1 | 0.5 | continuous |
| `second_oscillator_gain` | normalized | 0–1 | 0.5 | continuous |
| `second_oscillator_morph` | normalized | 0–1 | 0.5 | continuous |
| `second_oscillator_octave` | octaves | -1–1 | 1 | integer |
| `second_oscillator_pitch` | semitones | -6–6 | 6 | integer |
| `second_oscillator_fine_tune` | cents | -50–50 | 50 | continuous |
| `stereo_spread` | normalized | 0–1 | 0.5 | continuous |
| `saturation_drive` | dimensionless | 0–24 | 12 | continuous |
| `saturation_mix` | normalized | 0–1 | 0.5 | continuous |
| `distortion_drive` | dimensionless | 0–32 | 16 | continuous |
| `distortion_mix` | normalized | 0–1 | 0.5 | continuous |
| `bitcrusher_sample_rate` | Hz | 1–sample rate | 4.5 octaves | continuous |
| `bitcrusher_bits` | bits | 1–16 | 7.5 | integer |
| `bitcrusher_mix` | normalized | 0–1 | 0.5 | continuous |
| `flanger_rate` | Hz | 0.02–16 | 4.8 octaves | continuous |
| `flanger_intensity` | normalized | 0–1 | 0.5 | continuous |
| `flanger_depth` | normalized | 0–1 | 0.5 | continuous |
| `flanger_feedback` | normalized | -0.95–0.95 | 0.95 | continuous |
| `flanger_mix` | normalized | 0–1 | 0.5 | continuous |
| `flanger_manual` | seconds | 0.0002–0.008 | 0.0039 | continuous |
| `ring_mod_frequency` | Hz | 20–10000 | 4.5 octaves | continuous |
| `ring_mod_rectify` | normalized | -1–1 | 1 | continuous |
| `ring_mod_mix` | normalized | 0–1 | 0.5 | continuous |
| `chorus_rate` | Hz | 0.05–8 | 3.7 octaves | continuous |
| `chorus_depth` | normalized | 0–1 | 0.5 | continuous |
| `chorus_mix` | normalized | 0–1 | 0.5 | continuous |
| `chorus_width` | normalized | 0–1 | 0.5 | continuous |
| `chorus_delay` | seconds | 0.006–0.03 | 0.012 | continuous |
| `chorus_feedback` | normalized | -0.35–0.35 | 0.35 | continuous |
| `eq_low` | dB | -12–12 | 12 | continuous |
| `eq_mid` | dB | -12–12 | 12 | continuous |
| `eq_high` | dB | -12–12 | 12 | continuous |
| `delay_time` | seconds | 0.001–2 | 0.9995 | continuous |
| `delay_feedback` | normalized | 0–0.95 | 0.475 | continuous |
| `delay_mix` | normalized | 0–1 | 0.5 | continuous |
| `plate_reverb_decay` | seconds | 0.1–10 | 4.95 | continuous |
| `plate_reverb_damping` | normalized | 0–1 | 0.5 | continuous |
| `plate_reverb_mix` | normalized | 0–1 | 0.5 | continuous |
| `plate_reverb_predelay` | seconds | 0–0.2 | 0.1 | continuous |
| `compressor_threshold` | dB | -60–0 | 30 | continuous |
| `compressor_ratio` | ratio | 1–20 | 9.5 | continuous |
| `compressor_makeup_gain` | dB | 0–24 | 12 | continuous |
| `compressor_attack_seconds` | seconds | 0.001–0.2 | 3.9 octaves | continuous |
| `compressor_release_seconds` | seconds | 0.01–2 | 3.9 octaves | continuous |

## Module behavior

- **Envelope:** Capture attack, decay, sustain, and release into the voice at
  note-on using the current LFO phase without advancing it. Changes to routes or
  global LFO controls affect future notes. Manual `synth_set_adsr()` still
  replaces all four settings for all voices, preserving envelope stage and
  level. Editing one ADSR field through generic/config access retains that
  whole-envelope behavior and supersedes earlier captures.
- **Oscillators:** Quantize secondary octave and semitone separately; fine tune
  remains continuous. Combine them with existing note tuning and pitch bend.
  Effective frequency drives both wavetable band selection and phase advance;
  modulation preserves oscillator phase and base frequency. No pitch glide is
  added between discrete settings.
- **Filter:** Integer pole targets blend between stage outputs over 2 ms. Stages
  run through modulation transitions without clearing their memory. Once the
  base topology is settled, the original manual processing path and unused
  stage histories are preserved. Returning to base may finish the local blend.
- **Bitcrusher:** Effective reduced sample rate advances the existing sampling
  clock without resetting it. Bit-depth quantization changes at the next clock
  tick, preserving the held sample between ticks. The discrete target is never
  interpolated into a fractional bit depth.
- **Flanger:** Apply the intensity-induced depth delta and feedback-curve delta
  to the manually stored component bases; add direct depth/feedback offsets
  afterward and clamp last. Setting route amounts in a different order has no
  effect. Internal phase and delay history are retained. Flanger and chorus
  bound their joint delay/depth read positions inside their allocated buffers.
- **Delay:** Apply the effective time's offset from the current base to existing
  fractional read heads, including retained tails. This continuously moves
  taps and produces pitch bends. It never starts the manual settling timer or
  clears a delay voice. Manual edits retain their existing settle/fade/tail
  lifecycle; the LFO offset follows those base taps. Removing modulation
  returns to the current manual taps. Modulated tails use a conservative silence
  interval before retirement, allowing moved taps to revisit history.
- **EQ and compressor:** Recompute coefficients from effective controls only
  when those controls change, retaining filter memory and the linked RMS
  detector. EQ keeps its existing near-neutral coefficient behavior; gains
  and positive attack/release times remain within the module's legal bounds.
- **Dry crossings and reverb:** Existing bypass semantics are retained.
  Saturation freezes its DC-blocker state at exactly zero mix. Plate reverb
  freezes its tank at exactly zero mix, and zero predelay bypasses the predelay
  line without clearing or advancing it. Returning above zero resumes history.
  Decay modulation derives bounded feedback without resetting the tank. Other
  effects retain their existing dry processing. No modulation update clears
  buffers or restarts effect oscillators.

The effect order remains saturation, distortion, bitcrusher, flanger, ring
modulator, chorus, EQ, delay, plate reverb, compressor, then master gain.

## Desktop configuration and migration

```text
lfo_rate=cc:1:21:log:0.05:20
lfo_depth=cc:1:22:linear:0:1
lfo_amount.delay_mix=cc:1:41:linear:-1:1
lfo_amount.flanger_rate=cc:1:42:linear:-1:1
```

These lines bind knobs to controls. They do not initialize parameter values,
amounts, or global depth. A target can have an amount binding without a base
binding. Changing an effect selector never redirects a parameter route.

Full bipolar linear bindings map CC 0 to -1, CC 63 and 64 to exact zero, and CC
127 to +1. Positive-only `linear:0:1` is supported. Pickup uses the same center
conversion and compares stored base/amount values, independent of the LFO.
Config knob ranges do not change the engine modulation spans.

The desktop catalog exposes 55 base controls and 52 amount controls, with room
for 256 direct bindings. Names, eligibility, and engine identities come from the
core catalog; controller scales, ranges, pickup, and syntax are adapter-owned.
Learn, list, bind, unbind, map-all, show, validate, and save share those identities.
Serialization writes bindings only, with float precision preserved, and reload
resets runtime pickup state.

The old dedicated route APIs and config aliases have been removed:

| Former config name | Replacement |
| --- | --- |
| `lfo_first_oscillator_morph_amount` | `lfo_amount.oscillator_morph` |
| `lfo_second_oscillator_morph_amount` | `lfo_amount.second_oscillator_morph` |
| `lfo_first_oscillator_gain_amount` | `lfo_amount.first_oscillator_gain` |
| `lfo_second_oscillator_gain_amount` | `lfo_amount.second_oscillator_gain` |
| `lfo_filter_amount` | `lfo_amount.filter_cutoff` |

Engine clients replace the corresponding dedicated setters/getters with
`synth_set_lfo_amount()` / `synth_get_lfo_amount()` and the destination ID.
The bundled Akai config uses the new names with its existing knob assignments
and positive-only ranges. MIDI protocol clients now include the adapter-local
`midi/midi_types.h`; engine clients use `synth_note_to_frequency()` from
`synth/pitch.h`. Voice note identity is named `note_number`.

## Validation

The CMake desktop suite and Makefile suite run 21 programs; the core-only suite
runs 17. Added coverage includes an independent 52-target inventory, an audio
consequence for every target, unchanged base getters, signed/domain math,
ADSR capture/manual interaction, mono/stereo and buffer-partition invariance,
stateful effect behavior, all routes with full polyphony, 107-binding round trips,
malformed/excluded config declarations, and pickup under modulation.

On macOS with AppleClang, the added modulation/config tests pass AddressSanitizer
and UndefinedBehaviorSanitizer. A standalone core client with 12 voices and all
effects produced byte-identical unmodulated output to the original revision,
including manual filter, delay, EQ, bit-depth, and pitch-bend changes. With all
routes enabled at 44.1/48/96 kHz, a release-build one-second render took about
0.05/0.05/0.10 seconds of CPU time. The largest measured 64-frame block was
0.14 ms, below the respective 1.45/1.33/0.67 ms budgets on this machine.
These measurements are a local offline check; live hardware audition and
Windows/Linux execution are not covered by this run.
