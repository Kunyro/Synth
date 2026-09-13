#include "synth/flanger.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../internal/synth_internal.h"

#define SYNTH_FLANGER_STEREO_PHASE_OFFSET 0.25f
#define SYNTH_FLANGER_OUTPUT_COMPENSATION 0.50f

static float sanitize_sample_rate(float sample_rate)
{
    return sample_rate >= SYNTH_FLANGER_MIN_SAMPLE_RATE ?
        sample_rate :
        SYNTH_FLANGER_MIN_SAMPLE_RATE;
}

static size_t max_delay_frames_for_sample_rate(float sample_rate)
{
    const size_t frames = (size_t)((SYNTH_FLANGER_MAX_DELAY_SECONDS * sample_rate) + 3.0f);

    return frames < 2 ? 2 : frames;
}

static int delay_line_has_storage(const synth_flanger_delay_line *line)
{
    return line->left != 0 && line->right != 0 && line->capacity_frames > 1;
}

static int allocate_delay_line(synth_flanger_delay_line *line, size_t capacity_frames)
{
    line->capacity_frames = capacity_frames;
    line->left = (float *)calloc(capacity_frames, sizeof(float));
    line->right = (float *)calloc(capacity_frames, sizeof(float));

    if (!delay_line_has_storage(line)) {
        free(line->left);
        free(line->right);
        line->left = 0;
        line->right = 0;
        line->capacity_frames = 0;
        return 0;
    }

    return 1;
}

static void free_delay_line(synth_flanger_delay_line *line)
{
    free(line->left);
    free(line->right);
    line->left = 0;
    line->right = 0;
    line->write_index = 0;
    line->capacity_frames = 0;
}

static void resize_delay_line(synth_flanger_delay_line *line, size_t capacity_frames)
{
    free_delay_line(line);
    (void)allocate_delay_line(line, capacity_frames);
}

static float wrap_position(float position, size_t capacity_frames)
{
    const float capacity = (float)capacity_frames;

    while (position < 0.0f) {
        position += capacity;
    }

    while (position >= capacity) {
        position -= capacity;
    }

    return position;
}

static float interpolate_sample(
    const float *buffer,
    size_t capacity_frames,
    float position)
{
    const size_t first_index = (size_t)position;
    const size_t second_index = (first_index + 1) % capacity_frames;
    const float fraction = position - (float)first_index;

    return buffer[first_index] +
        ((buffer[second_index] - buffer[first_index]) * fraction);
}

static float read_delay_sample(
    const float *buffer,
    size_t capacity_frames,
    size_t write_index,
    float delay_frames)
{
    const float bounded_delay = synth_clampf(delay_frames, 1.0f, (float)(capacity_frames - 1));
    
    const float read_position = wrap_position((float) write_index - bounded_delay, capacity_frames);

    return interpolate_sample(buffer, capacity_frames, read_position);
}

// reads the flanger's internal triangle sweep at a normalized cycle position
static float lfo_value_at_phase(float phase)
{
    phase = synth_wrap_phase(phase);

    // a 0..1 triangle rises for half a cycle and falls for the other half,
    // moving the delay tap at a steady speed within each half of the sweep
    return phase < 0.5f ? phase * 2.0f : 2.0f - (phase * 2.0f);
}

// starts at the manual delay and uses depth to sweep through the remaining safe delay range
static float delay_seconds_for_lfo(const synth_flanger_params *params, float lfo)
{
    // the internal wave is 0..1 use only the space above the manual delay so
    // combining a large manual time with full depth cannot exceed the delay limit
    const float sweep_seconds = SYNTH_FLANGER_MAX_DELAY_SECONDS - params->manual_delay_seconds;

    return params->manual_delay_seconds + (sweep_seconds * params->depth * lfo);
}

static float feedback_for_intensity(float intensity)
{
    const float shaped = sqrtf(synth_clampf(intensity, 0.0f, 1.0f));

    // intensity brings feedback in early so the comb filter becomes recognizable fast
    return SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK +
        (shaped * (SYNTH_FLANGER_MAX_FEEDBACK - SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK));
}

static void apply_intensity(synth_flanger *flanger)
{
    flanger->depth = flanger->intensity;
    flanger->feedback = feedback_for_intensity(flanger->intensity);
}

static float mix_sample(float dry, float wet, float mix)
{
    const float compensated_gain = 1.0f + (mix * SYNTH_FLANGER_OUTPUT_COMPENSATION);

    // flanging needs dry plus wet interference, so mix controls added wet level
    return (dry + (wet * mix)) / compensated_gain;
}

// advances the flanger's own modulation clock at the effective rate without restarting it
static void advance_phase(synth_flanger *flanger, float rate_hz)
{
    // convert cycles per second to cycles per sample, retaining the current phase
    flanger->phase = synth_wrap_phase(
        flanger->phase + (rate_hz / flanger->sample_rate));
}

void synth_flanger_init(synth_flanger *flanger, float sample_rate)
{
    memset(flanger, 0, sizeof(*flanger));
    flanger->sample_rate = sanitize_sample_rate(sample_rate);
    flanger->rate_hz = SYNTH_FLANGER_DEFAULT_RATE_HZ;
    flanger->intensity = SYNTH_FLANGER_DEFAULT_INTENSITY;
    apply_intensity(flanger);
    flanger->mix = 0.0f;
    flanger->manual_delay_seconds = SYNTH_FLANGER_DEFAULT_MANUAL_SECONDS;
    flanger->phase = 0.0f;
    (void)allocate_delay_line(
        &flanger->delay,
        max_delay_frames_for_sample_rate(flanger->sample_rate));
}

void synth_flanger_uninit(synth_flanger *flanger)
{
    if (flanger == 0) {
        return;
    }

    free_delay_line(&flanger->delay);
}

void synth_flanger_set_sample_rate(synth_flanger *flanger, float sample_rate)
{
    flanger->sample_rate = sanitize_sample_rate(sample_rate);
    resize_delay_line(
        &flanger->delay,
        max_delay_frames_for_sample_rate(flanger->sample_rate));
}

void synth_flanger_set_rate(synth_flanger *flanger, float hz)
{
    flanger->rate_hz = synth_clampf(
        hz,
        SYNTH_FLANGER_MIN_RATE_HZ,
        SYNTH_FLANGER_MAX_RATE_HZ);
}

void synth_flanger_set_intensity(synth_flanger *flanger, float intensity)
{
    flanger->intensity = synth_clampf(intensity, 0.0f, 1.0f);
    apply_intensity(flanger);
}

void synth_flanger_set_depth(synth_flanger *flanger, float depth)
{
    flanger->depth = synth_clampf(depth, 0.0f, 1.0f);
}

void synth_flanger_set_feedback(synth_flanger *flanger, float feedback)
{
    flanger->feedback = synth_clampf(
        feedback,
        -SYNTH_FLANGER_MAX_FEEDBACK,
        SYNTH_FLANGER_MAX_FEEDBACK);
}

void synth_flanger_set_mix(synth_flanger *flanger, float mix)
{
    flanger->mix = synth_clampf(mix, 0.0f, 1.0f);
}

void synth_flanger_set_manual(synth_flanger *flanger, float seconds)
{
    flanger->manual_delay_seconds = synth_clampf(
        seconds,
        SYNTH_FLANGER_MIN_MANUAL_SECONDS,
        SYNTH_FLANGER_MAX_MANUAL_SECONDS);
}

float synth_flanger_get_rate(const synth_flanger *flanger)
{
    return flanger->rate_hz;
}

float synth_flanger_get_intensity(const synth_flanger *flanger)
{
    return flanger->intensity;
}

float synth_flanger_get_depth(const synth_flanger *flanger)
{
    return flanger->depth;
}

float synth_flanger_get_feedback(const synth_flanger *flanger)
{
    return flanger->feedback;
}

float synth_flanger_get_mix(const synth_flanger *flanger)
{
    return flanger->mix;
}

float synth_flanger_get_manual(const synth_flanger *flanger)
{
    return flanger->manual_delay_seconds;
}

// moves the flanger's stereo taps with temporary controls, keeping its clock and echo history
synth_stereo_sample synth_flanger_process_with_params(
    synth_flanger *flanger,
    synth_stereo_sample input,
    const synth_flanger_params *params)
{
    synth_stereo_sample delayed;
    synth_stereo_sample output;
    float left_delay_frames;
    float right_delay_frames;

    if (!delay_line_has_storage(&flanger->delay)) {
        return input;
    }

    // delay helpers return seconds; multiplying by sample rate gives buffer frames
    // a quarter-cycle offset makes the channels sweep at different times for stereo width
    left_delay_frames = delay_seconds_for_lfo(
        params,
        lfo_value_at_phase(flanger->phase)) * flanger->sample_rate;
    right_delay_frames = delay_seconds_for_lfo(
        params,
        lfo_value_at_phase(flanger->phase + SYNTH_FLANGER_STEREO_PHASE_OFFSET)) *
        flanger->sample_rate;

    delayed.left = read_delay_sample(
        flanger->delay.left,
        flanger->delay.capacity_frames,
        flanger->delay.write_index,
        left_delay_frames);
    delayed.right = read_delay_sample(
        flanger->delay.right,
        flanger->delay.capacity_frames,
        flanger->delay.write_index,
        right_delay_frames);

    flanger->delay.left[flanger->delay.write_index] =
        input.left + (delayed.left * params->feedback);
    flanger->delay.right[flanger->delay.write_index] =
        input.right + (delayed.right * params->feedback);
    flanger->delay.write_index =
        (flanger->delay.write_index + 1) % flanger->delay.capacity_frames;
    advance_phase(flanger, params->rate_hz);

    output.left = mix_sample(input.left, -delayed.left, params->mix);
    output.right = mix_sample(input.right, -delayed.right, params->mix);
    return output;
}

// copies stored controls into a value struct; buffers, phases, and other history stay in the effect
synth_flanger_params synth_flanger_get_params(const synth_flanger *effect)
{
    const synth_flanger_params params = {
        effect->rate_hz,
        effect->intensity,
        effect->depth,
        effect->feedback,
        effect->mix,
        effect->manual_delay_seconds
    };
    return params;
}

// processes a sample using the stored controls through the same path used for modulation
synth_stereo_sample synth_flanger_process(
    synth_flanger *effect,
    synth_stereo_sample input)
{
    const synth_flanger_params params = synth_flanger_get_params(effect);
    return synth_flanger_process_with_params(effect, input, &params);
}

// adds only the depth/feedback change caused by intensity, preserving manual component edits
void synth_flanger_resolve_intensity(synth_flanger_params *params, float intensity)
{
    // apply only the macro's change, retaining manually adjusted component bases
    // subtract the old curve result from the new one; assigning the new curve alone
    // would overwrite a user's separate feedback setting on every audio frame
    params->depth += intensity - params->intensity;
    params->feedback += feedback_for_intensity(intensity) - feedback_for_intensity(params->intensity);
    params->intensity = intensity;
    // components are clamped after their direct modulation is added for example,
    // depth 0.9 + intensity change 0.5 - direct change 0.5 should remain 0.9;
    // clamping the intermediate 1.4 would incorrectly produce 0.5
}
