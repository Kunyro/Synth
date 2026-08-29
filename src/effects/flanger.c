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

static float lfo_value_at_phase(float phase)
{
    phase = synth_wrap_phase(phase);

    // turns the sine wave into triangle like wave for a more obvious effect
    return phase < 0.5f ? phase * 2.0f : 2.0f - (phase * 2.0f);
}

static float delay_seconds_for_lfo(const synth_flanger *flanger, float lfo)
{
    const float sweep_seconds = SYNTH_FLANGER_MAX_DELAY_SECONDS - flanger->manual_delay_seconds;

    return flanger->manual_delay_seconds + (sweep_seconds * flanger->depth * lfo);
}

static float feedback_for_intensity(float intensity)
{
    const float shaped = sqrtf(synth_clampf(intensity, 0.0f, 1.0f));

    // intensity brings feedback in early so the comb filter becomes recognizable fast.
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

    // flanging needs dry plus wet interference, so mix controls added wet level.
    return (dry + (wet * mix)) / compensated_gain;
}

static void advance_phase(synth_flanger *flanger)
{
    flanger->phase = synth_wrap_phase(
        flanger->phase + (flanger->rate_hz / flanger->sample_rate));
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

synth_stereo_sample synth_flanger_process(
    synth_flanger *flanger,
    synth_stereo_sample input)
{
    synth_stereo_sample delayed;
    synth_stereo_sample output;
    float left_delay_frames;
    float right_delay_frames;

    if (!delay_line_has_storage(&flanger->delay)) {
        return input;
    }

    left_delay_frames = delay_seconds_for_lfo(
        flanger,
        lfo_value_at_phase(flanger->phase)) * flanger->sample_rate;
    right_delay_frames = delay_seconds_for_lfo(
        flanger,
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
        input.left + (delayed.left * flanger->feedback);
    flanger->delay.right[flanger->delay.write_index] =
        input.right + (delayed.right * flanger->feedback);
    flanger->delay.write_index =
        (flanger->delay.write_index + 1) % flanger->delay.capacity_frames;
    advance_phase(flanger);

    output.left = mix_sample(input.left, -delayed.left, flanger->mix);
    output.right = mix_sample(input.right, -delayed.right, flanger->mix);
    return output;
}
