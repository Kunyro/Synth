# Tests

The test suite is a set of small C programs under `tests/`. Each test builds to
its own binary in `build/`, and `make test` runs them in sequence.

Run all tests:

```sh
make test
```

The current suite covers:

- Oscillator behavior
- LFO behavior
- ADSR envelope behavior
- Filter behavior
- Saturation
- Distortion
- Bitcrusher
- Delay
- Voice and synth behavior
- MIDI parsing
- MIDI controller mapping

Build artifacts are written to `build/` and can be removed with:

```sh
make clean
```
