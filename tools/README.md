# Tools

Developer utilities live in this folder.

## MIDI Monitor

`midi_monitor.c` builds to `build/midi_monitor`. It helps inspect MIDI input and
create controller mapping files.

Build and run the monitor:

```sh
make midi-monitor
```

Or run a specific command after building:

```sh
./build/midi_monitor monitor
./build/midi_monitor show --config config/midi/akai_mpk_mini_mk2.conf
./build/midi_monitor validate --config config/midi/akai_mpk_mini_mk2.conf
./build/midi_monitor learn --output config/midi/my_controller.conf
./build/midi_monitor learn --edit config/midi/akai_mpk_mini_mk2.conf
```

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
