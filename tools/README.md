# Tools

Developer utilities live in this folder.

## MIDI Monitor

`midi_monitor.c` builds to `build/dev/midi_monitor` with the default
development preset. It helps inspect MIDI input and create controller mapping
files.

With the Visual Studio preset, the debug executable is
`build/vs2022/Debug/midi_monitor.exe`.

Build and run the monitor:

```sh
cmake --preset dev
cmake --build --preset dev --target run_midi_monitor
```

Or run a specific command after building:

```sh
cmake --build --preset dev --target midi_monitor
./build/dev/midi_monitor monitor
./build/dev/midi_monitor show --config config/midi/akai_mpk_mini_mk2.conf
./build/dev/midi_monitor validate --config config/midi/akai_mpk_mini_mk2.conf
./build/dev/midi_monitor learn --output config/midi/my_controller.conf
./build/dev/midi_monitor learn --edit config/midi/akai_mpk_mini_mk2.conf
```

Run direct executable commands from the repository root so relative config paths
resolve to `config/midi/`.

## Monitor Mode

Touch one controller at a time and the monitor prints the incoming MIDI message,
including channel, message type, values, and raw bytes.

Recognized messages include:

- Note on and note off
- Control change
- Pitch bend
- Program change
- Channel pressure
- Poly pressure
- Common system messages
- Realtime messages

## Learn Mode

`learn` starts an interactive MIDI-learn prompt.

Useful commands:

| Command | Description |
| --- | --- |
| `list` | Show mappable parameters and current bindings |
| `bind <name\|number>` | Move a knob or fader and assign it to one parameter |
| `map-all` | Step through every parameter in order |
| `unbind <name\|number>` | Clear a binding |
| `name <text>` | Rename the config |
| `save` | Write the config file |
| `help` | Show learn-mode help |
| `quit` | Leave learn mode |

In `map-all`, move a control to bind the current parameter, press Enter to skip
it, or type `done`, `quit`, `exit`, or `cancel` to leave the mode. Editing an
existing config writes a `.bak` backup the first time you save.

Effects are available in learn mode by their parameter names, such as
`saturation_drive`, `flanger_rate`, `plate_reverb_mix`,
`compressor_threshold`, and `compressor_attack_seconds`.

LFO and envelope route amounts are listed alongside base/source parameters,
for 160 discoverable controls. Use commands such as `bind lfo_amount.delay_mix`,
`bind envelope_amount.filter_cutoff`, or `bind mod_envelope_depth`;
base and amount bindings are independent.
New amount bindings default to the signed `linear:-1:1` range. `map-all` includes
all 52 LFO amounts and 48 envelope amounts. Saving writes controller bindings
only, never current parameter values, route amounts, or pickup state.
`show` and `validate` accept the same
canonical names and validation rules as the desktop app.
