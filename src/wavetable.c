#include "wavetable.h"

#include <math.h>
#include <stddef.h>

#include "synth_internal.h"

// the wavetable bank is deliberately static: it avoids heap allocation, keeps
// the synth core portable, and makes the future embedded memory cost explicit.
#define SYNTH_WAVETABLE_SIZE 1024
#define SYNTH_WAVETABLE_GUARD_SIZE (SYNTH_WAVETABLE_SIZE + 1)
#define SYNTH_WAVETABLE_MIP_LEVELS 10
#define SYNTH_WAVETABLE_MORPH_POINTS 9
#define SYNTH_WAVETABLE_MAX_HARMONIC (SYNTH_WAVETABLE_SIZE / 2)

// one full circle in radians as a float.
#define SYNTH_WAVETABLE_PI 3.14159265358979323846f
#define SYNTH_WAVETABLE_TAU 6.28318530717958647692f

typedef struct synth_wavetable_mip {
    int harmonic_limit;
    float samples[SYNTH_WAVETABLE_GUARD_SIZE];
} synth_wavetable_mip;

typedef struct synth_wavetable_morph_frame {
    float morph;
    synth_wavetable_mip mips[SYNTH_WAVETABLE_MIP_LEVELS];
} synth_wavetable_morph_frame;

static synth_wavetable_morph_frame wavetable_bank[SYNTH_WAVETABLE_MORPH_POINTS];
static int wavetable_bank_ready = 0;

// returns the absolute value without forcing callers to include math details.
static float absf(float value)
{
    return value < 0.0f ? -value : value;
}

// returns the saw contribution for one harmonic.
// positive sine coefficients keep the saw's fundamental phase aligned with the
// sine and square tables, which makes morphing feel continuous instead of
// flipping polarity halfway through the knob travel.
static float saw_harmonic_amplitude(int harmonic)
{
    return 2.0f / (SYNTH_WAVETABLE_PI * (float)harmonic);
}

// returns the square contribution for one harmonic.
static float square_harmonic_amplitude(int harmonic)
{
    // since square is only odd harmonics
    if ((harmonic % 2) == 0) {
        return 0.0f;
    }

    return 4.0f / (SYNTH_WAVETABLE_PI * (float)harmonic);
}

// returns the sine contribution for one harmonic.
static float sine_harmonic_amplitude(int harmonic)
{
    return harmonic == 1 ? 1.0f : 0.0f;
}

// returns a harmonic amplitude for a normalized sine-to-saw-to-square morph.
// the morph happens in spectrum space, not by crossfading final waveforms. this
// keeps the timbre change smooth and makes every stored table naturally
// bandlimited by the mip's harmonic limit.
static float morphed_harmonic_amplitude(float morph, int harmonic)
{
    float first_half_amount;
    float second_half_amount;

    if (morph <= 0.5f) {
        first_half_amount = morph * 2.0f;
        return ((1.0f - first_half_amount) * sine_harmonic_amplitude(harmonic)) +
            (first_half_amount * saw_harmonic_amplitude(harmonic));
    }

    second_half_amount = (morph - 0.5f) * 2.0f;
    return ((1.0f - second_half_amount) * saw_harmonic_amplitude(harmonic)) +
        (second_half_amount * square_harmonic_amplitude(harmonic));
}

// scales a table to fit inside roughly -1..1.
static void normalize_table(synth_wavetable_mip *mip)
{
    float peak = 0.0f;

    for (size_t sample_index = 0; sample_index < SYNTH_WAVETABLE_SIZE; ++sample_index) {
        const float magnitude = absf(mip->samples[sample_index]);

        if (magnitude > peak) {
            peak = magnitude;
        }
    }

    if (peak <= 0.0f) {
        return;
    }

    for (size_t sample_index = 0; sample_index < SYNTH_WAVETABLE_SIZE; ++sample_index) {
        mip->samples[sample_index] /= peak;
    }

    mip->samples[SYNTH_WAVETABLE_SIZE] = mip->samples[0];
}

// fills one mip level by summing only the harmonics that are safe for that mip.
static void build_mip(synth_wavetable_mip *mip, float morph, int harmonic_limit)
{
    mip->harmonic_limit = harmonic_limit;

    for (size_t sample_index = 0; sample_index < SYNTH_WAVETABLE_SIZE; ++sample_index) {
        const float phase = (float)sample_index / (float)SYNTH_WAVETABLE_SIZE;
        float value = 0.0f;

        for (int harmonic = 1; harmonic <= harmonic_limit; ++harmonic) {
            const float amplitude = morphed_harmonic_amplitude(morph, harmonic);
            const float angle = SYNTH_WAVETABLE_TAU * (float)harmonic * phase;

            value += amplitude * sinf(angle);
        }

        mip->samples[sample_index] = value;
    }

    mip->samples[SYNTH_WAVETABLE_SIZE] = mip->samples[0];
    normalize_table(mip);
}

// initializes the shared wavetable bank the first time an oscillator asks for it.
void synth_wavetable_prepare(void)
{
    if (wavetable_bank_ready) {
        return;
    }

    for (int morph_index = 0; morph_index < SYNTH_WAVETABLE_MORPH_POINTS; ++morph_index) {
        const float morph = (float)morph_index / (float)(SYNTH_WAVETABLE_MORPH_POINTS - 1);
        int harmonic_limit = SYNTH_WAVETABLE_MAX_HARMONIC;

        wavetable_bank[morph_index].morph = morph;

        for (int mip_index = 0; mip_index < SYNTH_WAVETABLE_MIP_LEVELS; ++mip_index) {
            // each next mip keeps fewer harmonics so high notes stay clean.
            build_mip(&wavetable_bank[morph_index].mips[mip_index], morph, harmonic_limit);

            if (harmonic_limit > 1) {
                harmonic_limit /= 2;
            }
        }
    }

    wavetable_bank_ready = 1;
}

// chooses the richest mip whose harmonics still fit under Nyquist.
static int select_mip_index(float frequency, float sample_rate)
{
    const float safe_frequency = frequency > 1.0f ? frequency : 1.0f;
    const float nyquist = sample_rate * 0.5f;
    int allowed_harmonics;

    if (sample_rate <= 0.0f) {
        return SYNTH_WAVETABLE_MIP_LEVELS - 1;
    }

    // higher notes can fit fewer harmonics before they fold back as aliasing.
    allowed_harmonics = (int)floorf(nyquist / safe_frequency);
    if (allowed_harmonics < 1) {
        allowed_harmonics = 1;
    }

    for (int mip_index = 0; mip_index < SYNTH_WAVETABLE_MIP_LEVELS; ++mip_index) {
        const synth_wavetable_mip *mip = &wavetable_bank[0].mips[mip_index];

        if (mip->harmonic_limit <= allowed_harmonics) {
            return mip_index;
        }
    }

    return SYNTH_WAVETABLE_MIP_LEVELS - 1;
}

// reads a sample from one table with linear interpolation between table cells.
static float interpolate_table_sample(const synth_wavetable_mip *mip, float phase)
{
    const float wrapped_phase = phase - floorf(phase);
    const float table_position = wrapped_phase * (float)SYNTH_WAVETABLE_SIZE;
    const int base_index = (int)table_position;
    const float fraction = table_position - (float)base_index;
    const float first = mip->samples[base_index];
    const float second = mip->samples[base_index + 1];

    return first + ((second - first) * fraction);
}

// renders one sample from the closest mip and interpolates between morph frames.
float synth_wavetable_render(float phase, float frequency, float sample_rate, float morph)
{
    const float clamped_morph = synth_clampf(morph, 0.0f, 1.0f);
    const float morph_position = clamped_morph * (float)(SYNTH_WAVETABLE_MORPH_POINTS - 1);
    const int lower_morph_index = (int)floorf(morph_position);
    int mip_index;
    int upper_morph_index = lower_morph_index + 1;
    float morph_fraction = morph_position - (float)lower_morph_index;
    float lower_sample;
    float upper_sample;

    synth_wavetable_prepare();
    mip_index = select_mip_index(frequency, sample_rate);

    if (upper_morph_index >= SYNTH_WAVETABLE_MORPH_POINTS) {
        upper_morph_index = lower_morph_index;
        morph_fraction = 0.0f;
    }

    lower_sample = interpolate_table_sample(
        &wavetable_bank[lower_morph_index].mips[mip_index],
        phase);
    // the morph knob lands between stored frames, so blend the two neighbors.
    upper_sample = interpolate_table_sample(
        &wavetable_bank[upper_morph_index].mips[mip_index],
        phase);

    return lower_sample + ((upper_sample - lower_sample) * morph_fraction);
}
