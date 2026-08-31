# Tests

The test suite is a set of small C programs under `tests/`. CMake builds each
test as its own binary, and CTest runs them in sequence from the repository root.

Run all tests:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The current suite covers:

- Oscillator behavior
- LFO behavior
- ADSR envelope behavior
- Filter behavior
- Saturation
- Distortion
- Bitcrusher
- Flanger
- Ring mod
- Delay
- Plate reverb
- Voice and synth behavior
- MIDI parsing
- MIDI controller mapping

Build artifacts are written under `build/` and can be removed with:

```sh
cmake --build --preset dev --target clean
```
