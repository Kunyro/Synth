#include "synth/bitcrusher.h"

#include <math.h>

#include "../internal/synth_internal.h"

// blends from clean signal to fully crushed signal.
static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

// snaps amplitude to the nearest level available at the current bit depth.
static float quantize_sample(float input, int bits)
{
    const int level_count = 1 << bits;
    const float clamped = synth_clampf(input, -1.0f, 1.0f);
    const float step = 2.0f / (float)(level_count - 1);
    const float level = roundf((clamped + 1.0f) / step);

    return (level * step) - 1.0f;
}

// refreshes the held sample only when the reduced sample clock ticks.
static void update_held_sample(synth_bitcrusher *bitcrusher, synth_stereo_sample input)
{
    bitcrusher->held_sample.left = quantize_sample(input.left, bitcrusher->bits);
    bitcrusher->held_sample.right = quantize_sample(input.right, bitcrusher->bits);
    bitcrusher->has_held_sample = 1;
}

// moves the fractional reduced-rate clock forward by one host sample.
static void advance_sample_clock(synth_bitcrusher *bitcrusher)
{
    const float ratio = bitcrusher->sample_rate / bitcrusher->host_sample_rate;

    bitcrusher->phase += synth_clampf(ratio, 0.0f, 1.0f);
}

void synth_bitcrusher_init(synth_bitcrusher *bitcrusher, float host_sample_rate)
{
    bitcrusher->host_sample_rate =
        host_sample_rate < SYNTH_BITCRUSHER_MIN_SAMPLE_RATE
            ? SYNTH_BITCRUSHER_MIN_SAMPLE_RATE
            : host_sample_rate;
    bitcrusher->sample_rate = bitcrusher->host_sample_rate;
    bitcrusher->bits = SYNTH_BITCRUSHER_DEFAULT_BITS;
    bitcrusher->mix = 0.0f;
    bitcrusher->phase = 1.0f;
    bitcrusher->has_held_sample = 0;
    bitcrusher->held_sample.left = 0.0f;
    bitcrusher->held_sample.right = 0.0f;
}

void synth_bitcrusher_set_sample_rate(synth_bitcrusher *bitcrusher, float sample_rate)
{
    bitcrusher->sample_rate = synth_clampf(
        sample_rate,
        SYNTH_BITCRUSHER_MIN_SAMPLE_RATE,
        bitcrusher->host_sample_rate);
    bitcrusher->phase = 1.0f;
}

void synth_bitcrusher_set_bits(synth_bitcrusher *bitcrusher, int bits)
{
    bitcrusher->bits = synth_clampi(
        bits,
        SYNTH_BITCRUSHER_MIN_BITS,
        SYNTH_BITCRUSHER_MAX_BITS);
    bitcrusher->phase = 1.0f;
}

void synth_bitcrusher_set_mix(synth_bitcrusher *bitcrusher, float mix)
{
    bitcrusher->mix = synth_clampf(mix, 0.0f, 1.0f);
}

float synth_bitcrusher_get_sample_rate(const synth_bitcrusher *bitcrusher)
{
    return bitcrusher->sample_rate;
}

int synth_bitcrusher_get_bits(const synth_bitcrusher *bitcrusher)
{
    return bitcrusher->bits;
}

float synth_bitcrusher_get_mix(const synth_bitcrusher *bitcrusher)
{
    return bitcrusher->mix;
}

synth_stereo_sample synth_bitcrusher_process(
    synth_bitcrusher *bitcrusher,
    synth_stereo_sample input)
{
    synth_stereo_sample output;

    if (!bitcrusher->has_held_sample || bitcrusher->phase >= 1.0f) {
        update_held_sample(bitcrusher, input);
        bitcrusher->phase -= floorf(bitcrusher->phase);
    }
    advance_sample_clock(bitcrusher);

    output.left = mix_sample(input.left, bitcrusher->held_sample.left, bitcrusher->mix);
    output.right = mix_sample(input.right, bitcrusher->held_sample.right, bitcrusher->mix);
    return output;
}
