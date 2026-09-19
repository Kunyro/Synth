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
- LFO behavior and every one of the 52 modulation destinations
- ADSR envelope behavior and independent per-voice modulation envelopes
- Partial initialization cleanup and allocation-free rendering/voice replacement
- Filter behavior
- Saturation
- Distortion
- Bitcrusher
- Flanger
- Chorus
- Compressor
- Ring mod
- EQ
- Delay
- Plate reverb
- Voice and synth behavior
- Musical pitch conversion
- MIDI parsing
- MIDI controller mapping, chord mode, and 160-control config round trips

Build artifacts are written under `build/` and can be removed with:

```sh
cmake --build --preset dev --target clean
```

`test_modulation` checks destination coverage and audio consequences, unchanged
base values, signed/domain evaluation, envelope capture and manual updates,
buffer partitioning, mono/stereo agreement, bitcrusher clock preservation,
moving delay history, flanger composition order, unmodulated filter history,
and all routes with 12 voices at 44.1, 48, and 96 kHz. `test_lfo_mapping` checks
signed endpoints and exact center zero, pickup while modulation runs, save/load
without startup values, more than 64 bindings, and malformed/excluded targets.

`test_mod_envelope` checks all 48 endpoint/domain rules, additive source math,
full-duration release/editing, independent voices, nonlinear per-voice reference
audio, default-off identity, delayed tails, exact steal fading, and pending-note
cancellation. `test_allocations` injects a failure at every allocation site,
verifies cleanup, and checks the render/note/control paths do not allocate.
MIDI tests cover the five source controls, both amount kinds, positive-only
pickup, and acceptance/rejection at the 256-binding boundary.

Build the engine and its 19 tests without any desktop adapters:

```sh
cmake -S . -B build/core-check -DSYNTH_BUILD_DESKTOP=OFF -DBUILD_TESTING=ON
cmake --build build/core-check
ctest --test-dir build/core-check --output-on-failure
```

The desktop configuration and `make test` run all 23 test programs. The new
modulation and config tests also support AddressSanitizer and UndefinedBehaviorSanitizer
through the compiler/linker flags supplied to CMake.
