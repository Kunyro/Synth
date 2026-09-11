#include "synth/bitcrusher.h"

#include <math.h>

#include "../internal/synth_internal.h"

// blends from clean signal to fully crushed signal
static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

// snaps amplitude to the nearest level available at the current bit depth
static float quantize_sample(float input, int bits)
{
    // n bits provide 2^n amplitude levels spread them across the full -1..+1 span;
    // shifting by +1 lets rounding work with a level index starting at zero
    const int level_count = 1 << bits;
    const float clamped = synth_clampf(input, -1.0f, 1.0f);
    const float step = 2.0f / (float)(level_count - 1);
    const float level = roundf((clamped + 1.0f) / step);

    return (level * step) - 1.0f;
}

// refreshes the held sample only when the reduced sample clock ticks
static void update_held_sample(synth_bitcrusher *bitcrusher, synth_stereo_sample input, int bits)
{
    bitcrusher->held_sample.left = quantize_sample(input.left, bits);
    bitcrusher->held_sample.right = quantize_sample(input.right, bits);
    bitcrusher->has_held_sample = 1;
}

// moves the fractional reduced-rate clock forward by one host sample
static void advance_sample_clock(synth_bitcrusher *bitcrusher, float sample_rate)
{
    // a ratio of 0.25 takes four host samples to reach one reduced-clock tick
    const float ratio = sample_rate / bitcrusher->host_sample_rate;

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

// uses temporary rate/bits/mix without resetting the reduced-rate sampling clock
synth_stereo_sample synth_bitcrusher_process_with_params(
    synth_bitcrusher *bitcrusher,
    synth_stereo_sample input,
    const synth_bitcrusher_params *params)
{
    synth_stereo_sample output;

    // rate and bit-depth modulation must wait for the next clock tick calling
    // their manual setters here would reset phase every frame and defeat sample holding
    if (!bitcrusher->has_held_sample || bitcrusher->phase >= 1.0f) {
        update_held_sample(bitcrusher, input, params->bits);
        // keep the fractional remainder so a changing rate does not lose clock progress
        bitcrusher->phase -= floorf(bitcrusher->phase);
    }
    advance_sample_clock(bitcrusher, params->sample_rate);

    output.left = mix_sample(input.left, bitcrusher->held_sample.left, params->mix);
    output.right = mix_sample(input.right, bitcrusher->held_sample.right, params->mix);
    return output;
}

// copies stored controls into a value struct; buffers, phases, and other history stay in the effect
synth_bitcrusher_params synth_bitcrusher_get_params(const synth_bitcrusher *effect)
{
    const synth_bitcrusher_params params = {
        effect->sample_rate,
        effect->bits,
        effect->mix
    };
    return params;
}

// processes a sample using the stored controls through the same path used for modulation
synth_stereo_sample synth_bitcrusher_process(
    synth_bitcrusher *effect,
    synth_stereo_sample input)
{
    const synth_bitcrusher_params params = synth_bitcrusher_get_params(effect);
    return synth_bitcrusher_process_with_params(effect, input, &params);
}
