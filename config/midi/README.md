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
effect_selector_N=cc:channel:control
effect_N_macro_M=cc:channel:control
chord_pad=cc:channel:control
```

Example:

```text
filter_cutoff=cc:1:5:log:20.0:20000.0
effect_selector_1=cc:1:10
effect_1_macro_1=cc:1:11
effect_1_macro_2=cc:1:12
effect_1_macro_3=cc:1:13
effect_selector_2=cc:1:14
effect_2_macro_1=cc:1:15
effect_2_macro_2=cc:1:16
effect_2_macro_3=cc:1:17
chord_major=cc:1:35
```

Supported scales:

- `linear`
- `log`
- `step`

CC mappings use soft takeover: a knob must reach or cross the current synth
value before it starts changing that parameter.

Effect macro controls are an optional alternative to binding every effect
parameter directly. Each selector row chooses one of five fixed pages:

Bank 1:

- values `0` through `25`: saturation
- values `26` through `51`: distortion
- values `52` through `76`: bitcrusher
- values `77` through `102`: delay
- values `103` through `127`: plate reverb

Bank 2:

- values `0` through `25`: flanger
- values `26` through `51`: ring mod
- values `52` through `127`: blank until more effects are added

The three macro knobs then control the selected page:

| Effect | Macro 1 | Macro 2 | Macro 3 |
| --- | --- | --- | --- |
| saturation | `saturation_drive` | unused | `saturation_mix` |
| distortion | `distortion_drive` | unused | `distortion_mix` |
| bitcrusher | `bitcrusher_sample_rate` | `bitcrusher_bits` | `bitcrusher_mix` |
| delay | `delay_time` | `delay_feedback` | `delay_mix` |
| plate reverb | `plate_reverb_decay` | `plate_reverb_damping` | `plate_reverb_mix` |
| flanger | `flanger_rate` | `flanger_intensity` | `flanger_mix` |
| ring mod | `ring_mod_frequency` | `ring_mod_rectify` | `ring_mod_mix` |

Macro knobs use independent soft takeover for each selector row and effect page,
so switching pages does not make a knob jump the newly selected effect
parameter.

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
- `flanger_rate`
- `flanger_intensity`
- `flanger_depth`
- `flanger_feedback`
- `flanger_mix`
- `flanger_manual`
- `ring_mod_frequency`
- `ring_mod_rectify`
- `ring_mod_mix`
- `delay_time`
- `delay_feedback`
- `delay_mix`
- `plate_reverb_decay`
- `plate_reverb_damping`
- `plate_reverb_mix`
- `plate_reverb_predelay`

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
| 25 | `effect_selector_1` |
| 26 | `effect_1_macro_1` |
| 27 | `effect_1_macro_2` |
| 28 | `effect_1_macro_3` |
| 29 | `effect_selector_2` |
| 30 | `effect_2_macro_1` |
| 31 | `effect_2_macro_2` |
| 32 | `effect_2_macro_3` |
| 33 | `chord_diminished` |
| 34 | `chord_minor` |
| 35 | `chord_major` |
| 36 | `chord_suspended` |
| 37 | `chord_6` |
| 38 | `chord_minor_7` |
| 39 | `chord_major_7` |
| 40 | `chord_9` |

See [tools/README.md](../../tools/README.md) for the full MIDI monitor workflow.
