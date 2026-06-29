# Synth
Synth engine written in C

## First oscillator test

This project currently uses [miniaudio](https://miniaud.io/) for desktop audio output.
The starter program opens the default playback device and generates a stereo sine wave.

Build:

```sh
make
```

Run until Enter is pressed:

```sh
make run
```

Run a specific frequency for a fixed number of seconds:

```sh
./build/synth 440 2
```

Arguments:

- `frequency`: sine frequency in Hz, default `220`
- `seconds`: optional run duration; omit it to stop with Enter
