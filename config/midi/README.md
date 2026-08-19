# MIDI Controller Mapping

MIDI controller config files live in this folder. The desktop app loads this
default config unless another path is provided:

```text
config/midi/akai_mpk_mini_mk2.conf
```

## Running With A Config

```sh
./build/dev/synth --midi-config config/midi/akai_mpk_mini_mk2.conf
./build/dev/synth --midi-config=path/to/controller.conf
./build/dev/synth --no-midi-config
```

Run these commands from the repository root so relative config paths resolve to
`config/midi/`.

## File Format

Config lines use:

```text
parameter=cc:channel:control:scale:min:max
chord_pad=cc:channel:control
```

Example:

```text
filter_cutoff=cc:1:5:log:20.0:20000.0
chord_major=cc:1:35
```

Supported scales:

- `linear`
- `log`
- `step`

CC mappings use soft takeover: a knob must reach or cross the current synth
value before it starts changing that parameter.

Chord pad mappings are momentary controls: CC values `1` through `127` mean
held, and CC value `0` means released.

## Creating A Mapping

Use the MIDI monitor to inspect, validate, and learn mappings:

```sh
cmake --build --preset dev --target midi_monitor
./build/dev/midi_monitor show --config config/midi/akai_mpk_mini_mk2.conf
./build/dev/midi_monitor validate --config config/midi/akai_mpk_mini_mk2.conf
./build/dev/midi_monitor learn --output config/midi/my_controller.conf
./build/dev/midi_monitor learn --edit config/midi/akai_mpk_mini_mk2.conf
```

## Supported Parameters

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
- `saturation_drive`
- `saturation_mix`
- `distortion_drive`
- `distortion_mix`
- `bitcrusher_sample_rate`
- `bitcrusher_bits`
- `bitcrusher_mix`
- `delay_time`
- `delay_feedback`
- `delay_mix`

## Supported Chord Pads

- `chord_diminished`
- `chord_minor`
- `chord_major`
- `chord_suspended`
- `chord_6`
- `chord_minor_7`
- `chord_major_7`
- `chord_9`

## Default Akai MPK Mini Mk II Mapping

The included config maps Akai MPK Mini MK2-style CC knobs on channel 1:

| CC | Parameter |
| --- | --- |
| 1 | `attack` |
| 2 | `decay` |
| 3 | `sustain` |
| 4 | `release` |
| 5 | `filter_cutoff` |
| 6 | `filter_poles` |
| 7 | `oscillator_morph` |
| 8 | `master_gain` |
| 9 | `second_oscillator_octave` |
| 10 | `second_oscillator_pitch` |
| 11 | `second_oscillator_fine_tune` |
| 12 | `stereo_spread` |
| 13 | `first_oscillator_gain` |
| 14 | `second_oscillator_gain` |
| 15 | `second_oscillator_morph` |
| 16 | `master_gain` |
| 17 | `lfo_first_oscillator_gain_amount` |
| 18 | `lfo_second_oscillator_gain_amount` |
| 19 | `lfo_first_oscillator_morph_amount` |
| 20 | `lfo_second_oscillator_morph_amount` |
| 21 | `lfo_rate` |
| 22 | `lfo_depth` |
| 23 | `lfo_filter_amount` |
| 24 | `lfo_shape_morph` |
| 25 | `distortion_mix` |
| 26 | `distortion_drive` |
| 27 | `bitcrusher_mix` |
| 28 | `bitcrusher_sample_rate` |
| 29 | `bitcrusher_bits` |
| 30 | `delay_mix` |
| 31 | `delay_time` |
| 32 | `delay_feedback` |
| 33 | `chord_diminished` |
| 34 | `chord_minor` |
| 35 | `chord_major` |
| 36 | `chord_suspended` |
| 37 | `chord_6` |
| 38 | `chord_minor_7` |
| 39 | `chord_major_7` |
| 40 | `chord_9` |

See [tools/README.md](../../tools/README.md) for the full MIDI monitor workflow.
