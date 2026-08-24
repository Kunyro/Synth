#include "synth/plate_reverb.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../internal/synth_internal.h"

// pick a prime number here makes it better
#define SYNTH_PLATE_REVERB_REFERENCE_SAMPLE_RATE 29761.0f
#define SYNTH_PLATE_REVERB_INPUT_BANDWIDTH 0.70f
#define SYNTH_PLATE_REVERB_INPUT_GAIN 0.50f
#define SYNTH_PLATE_REVERB_WET_GAIN 0.42f

static float sanitize_sample_rate(float sample_rate)
{
    return sample_rate > SYNTH_PLATE_REVERB_MIN_SAMPLE_RATE
        ? sample_rate
        : SYNTH_PLATE_REVERB_MIN_SAMPLE_RATE;
}

static size_t frames_for_seconds(float sample_rate, float seconds)
{
    const size_t frames = (size_t)((sample_rate * seconds) + 0.5f);

    return frames < 1 ? 1 : frames;
}

static size_t scaled_frames(float sample_rate, size_t reference_frames)
{
    const float scaled =
        ((float)reference_frames * sample_rate) /
        SYNTH_PLATE_REVERB_REFERENCE_SAMPLE_RATE;
    const size_t frames = (size_t)(scaled + 0.5f);

    return frames < 1 ? 1 : frames;
}

static int delay_line_has_storage(const synth_plate_reverb_delay_line *line)
{
    return line->samples != 0 && line->capacity_frames > 0;
}

static int allocate_delay_line(
    synth_plate_reverb_delay_line *line,
    size_t capacity_frames)
{
    line->samples = (float *)calloc(capacity_frames, sizeof(float));
    line->capacity_frames = capacity_frames;
    line->write_index = 0;

    if (!delay_line_has_storage(line)) {
        free(line->samples);
        line->samples = 0;
        line->capacity_frames = 0;
        return 0;
    }

    return 1;
}

static void free_delay_line(synth_plate_reverb_delay_line *line)
{
    free(line->samples);
    line->samples = 0;
    line->write_index = 0;
    line->capacity_frames = 0;
}

static void resize_delay_line(
    synth_plate_reverb_delay_line *line,
    size_t capacity_frames)
{
    free_delay_line(line);
    (void)allocate_delay_line(line, capacity_frames);
}

static void clear_delay_line(synth_plate_reverb_delay_line *line)
{
    if (!delay_line_has_storage(line)) {
        return;
    }

    memset(line->samples, 0, sizeof(float) * line->capacity_frames);
    line->write_index = 0;
}

static float wrap_read_position(float position, size_t capacity_frames)
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

static float read_fractional_delay(
    const synth_plate_reverb_delay_line *line,
    float delay_frames)
{
    float bounded_delay;
    float read_position;

    if (!delay_line_has_storage(line) || line->capacity_frames < 2) {
        return 0.0f;
    }

    bounded_delay = synth_clampf(
        delay_frames,
        1.0f,
        (float)(line->capacity_frames - 1));
    read_position = wrap_read_position(
        (float)line->write_index - bounded_delay,
        line->capacity_frames);
    return interpolate_sample(line->samples, line->capacity_frames, read_position);
}

static float process_delay_line(
    synth_plate_reverb_delay_line *line,
    float input)
{
    float output;

    if (!delay_line_has_storage(line)) {
        return 0.0f;
    }

    output = line->samples[line->write_index];
    line->samples[line->write_index] = input;
    line->write_index = (line->write_index + 1) % line->capacity_frames;
    return output;
}

static float process_predelay(synth_plate_reverb *reverb, float input)
{
    float output;

    if (reverb->predelay_seconds <= 0.0f) {
        return input;
    }

    output = read_fractional_delay(
        &reverb->predelay,
        reverb->predelay_seconds * reverb->sample_rate);

    if (delay_line_has_storage(&reverb->predelay)) {
        reverb->predelay.samples[reverb->predelay.write_index] = input;
        reverb->predelay.write_index =
            (reverb->predelay.write_index + 1) %
            reverb->predelay.capacity_frames;
    }

    return output;
}

static float process_allpass(
    synth_plate_reverb_allpass *allpass,
    float input)
{
    float delayed;
    float output;

    if (!delay_line_has_storage(&allpass->delay)) {
        return input;
    }

    delayed = allpass->delay.samples[allpass->delay.write_index];
    output = delayed - (allpass->feedback * input);
    allpass->delay.samples[allpass->delay.write_index] =
        input + (allpass->feedback * output);
    allpass->delay.write_index =
        (allpass->delay.write_index + 1) %
        allpass->delay.capacity_frames;
    return output;
}

static float process_one_pole(
    synth_plate_reverb_one_pole *filter,
    float input,
    float coefficient)
{
    filter->state += coefficient * (input - filter->state);

    return filter->state;
}

static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

static float branch_seconds(float sample_rate, size_t delay_a, size_t delay_b)
{
    return
        (float)(scaled_frames(sample_rate, delay_a) + scaled_frames(sample_rate, delay_b)) /
        sample_rate;
}

static float decay_feedback_for_seconds(float seconds, float sample_rate)
{
    const float loop_seconds =
        (branch_seconds(sample_rate, 4453, 4217) +
         branch_seconds(sample_rate, 3720, 3163)) *
        0.5f;
    const float feedback = powf(
        0.001f,
        loop_seconds / synth_clampf(
            seconds,
            SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS,
            SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS));

    return synth_clampf(feedback, 0.0f, SYNTH_PLATE_REVERB_MAX_FEEDBACK);
}

static float damping_coefficient(float damping)
{
    return synth_clampf(1.0f - (damping * 0.92f), 0.04f, 1.0f);
}

static float process_input_diffusion(synth_plate_reverb *reverb, float input)
{
    float sample = input;

    for (size_t i = 0; i < SYNTH_PLATE_REVERB_INPUT_ALLPASS_COUNT; ++i) {
        sample = process_allpass(&reverb->input_diffusers[i], sample);
    }

    return sample;
}

static synth_stereo_sample process_tank(
    synth_plate_reverb *reverb,
    float input)
{
    synth_plate_reverb_tank *tank = &reverb->tank;
    const float damping = damping_coefficient(reverb->damping);
    float left;
    float right;
    synth_stereo_sample wet;

    left = input + (tank->right_feedback * reverb->feedback);
    left = process_allpass(&tank->left_diffuser_1, left);
    left = process_delay_line(&tank->left_delay_1, left);
    left = process_one_pole(&tank->left_damping_filter, left, damping);
    left = process_allpass(&tank->left_diffuser_2, left);
    left = process_delay_line(&tank->left_delay_2, left);

    right = input + (tank->left_feedback * reverb->feedback);
    right = process_allpass(&tank->right_diffuser_1, right);
    right = process_delay_line(&tank->right_delay_1, right);
    right = process_one_pole(&tank->right_damping_filter, right, damping);
    right = process_allpass(&tank->right_diffuser_2, right);
    right = process_delay_line(&tank->right_delay_2, right);

    tank->left_feedback = left;
    tank->right_feedback = right;

    wet.left = (left - (right * 0.60f)) * SYNTH_PLATE_REVERB_WET_GAIN;
    wet.right = (right - (left * 0.60f)) * SYNTH_PLATE_REVERB_WET_GAIN;
    return wet;
}

static void configure_allpass(
    synth_plate_reverb_allpass *allpass,
    float sample_rate,
    size_t reference_frames,
    float feedback)
{
    resize_delay_line(&allpass->delay, scaled_frames(sample_rate, reference_frames));
    allpass->feedback = feedback;
}

static void configure_delay(
    synth_plate_reverb_delay_line *line,
    float sample_rate,
    size_t reference_frames)
{
    resize_delay_line(line, scaled_frames(sample_rate, reference_frames));
}

static void clear_allpass(synth_plate_reverb_allpass *allpass)
{
    clear_delay_line(&allpass->delay);
}

static void clear_tank(synth_plate_reverb_tank *tank)
{
    clear_allpass(&tank->left_diffuser_1);
    clear_allpass(&tank->left_diffuser_2);
    clear_allpass(&tank->right_diffuser_1);
    clear_allpass(&tank->right_diffuser_2);
    clear_delay_line(&tank->left_delay_1);
    clear_delay_line(&tank->left_delay_2);
    clear_delay_line(&tank->right_delay_1);
    clear_delay_line(&tank->right_delay_2);
    tank->left_damping_filter.state = 0.0f;
    tank->right_damping_filter.state = 0.0f;
    tank->left_feedback = 0.0f;
    tank->right_feedback = 0.0f;
}

static void clear_reverb_history(synth_plate_reverb *reverb)
{
    clear_delay_line(&reverb->predelay);
    reverb->bandwidth_filter.state = 0.0f;

    for (size_t i = 0; i < SYNTH_PLATE_REVERB_INPUT_ALLPASS_COUNT; ++i) {
        clear_allpass(&reverb->input_diffusers[i]);
    }

    clear_tank(&reverb->tank);
}

static void free_allpass(synth_plate_reverb_allpass *allpass)
{
    free_delay_line(&allpass->delay);
}

static void free_tank(synth_plate_reverb_tank *tank)
{
    free_allpass(&tank->left_diffuser_1);
    free_allpass(&tank->left_diffuser_2);
    free_allpass(&tank->right_diffuser_1);
    free_allpass(&tank->right_diffuser_2);
    free_delay_line(&tank->left_delay_1);
    free_delay_line(&tank->left_delay_2);
    free_delay_line(&tank->right_delay_1);
    free_delay_line(&tank->right_delay_2);
}

static int allpass_has_storage(const synth_plate_reverb_allpass *allpass)
{
    return delay_line_has_storage(&allpass->delay);
}

static int tank_has_storage(const synth_plate_reverb_tank *tank)
{
    return
        allpass_has_storage(&tank->left_diffuser_1) &&
        allpass_has_storage(&tank->left_diffuser_2) &&
        allpass_has_storage(&tank->right_diffuser_1) &&
        allpass_has_storage(&tank->right_diffuser_2) &&
        delay_line_has_storage(&tank->left_delay_1) &&
        delay_line_has_storage(&tank->left_delay_2) &&
        delay_line_has_storage(&tank->right_delay_1) &&
        delay_line_has_storage(&tank->right_delay_2);
}

static int reverb_has_storage(const synth_plate_reverb *reverb)
{
    for (size_t i = 0; i < SYNTH_PLATE_REVERB_INPUT_ALLPASS_COUNT; ++i) {
        if (!allpass_has_storage(&reverb->input_diffusers[i])) {
            return 0;
        }
    }

    return delay_line_has_storage(&reverb->predelay) &&
        tank_has_storage(&reverb->tank);
}

static void configure_reverb_lines(synth_plate_reverb *reverb, float sample_rate)
{
    configure_allpass(&reverb->input_diffusers[0], sample_rate, 142, 0.75f);
    configure_allpass(&reverb->input_diffusers[1], sample_rate, 107, 0.75f);
    configure_allpass(&reverb->input_diffusers[2], sample_rate, 379, 0.625f);
    configure_allpass(&reverb->input_diffusers[3], sample_rate, 277, 0.625f);

    configure_allpass(&reverb->tank.left_diffuser_1, sample_rate, 672, 0.70f);
    configure_delay(&reverb->tank.left_delay_1, sample_rate, 4453);
    configure_allpass(&reverb->tank.left_diffuser_2, sample_rate, 1800, 0.50f);
    configure_delay(&reverb->tank.left_delay_2, sample_rate, 4217);

    configure_allpass(&reverb->tank.right_diffuser_1, sample_rate, 908, 0.70f);
    configure_delay(&reverb->tank.right_delay_1, sample_rate, 3720);
    configure_allpass(&reverb->tank.right_diffuser_2, sample_rate, 2656, 0.50f);
    configure_delay(&reverb->tank.right_delay_2, sample_rate, 3163);

    resize_delay_line(
        &reverb->predelay,
        frames_for_seconds(sample_rate, SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS) + 2);
}

void synth_plate_reverb_init(synth_plate_reverb *reverb, float sample_rate)
{
    memset(reverb, 0, sizeof(*reverb));
    reverb->sample_rate = sanitize_sample_rate(sample_rate);
    reverb->decay_seconds = SYNTH_PLATE_REVERB_DEFAULT_DECAY_SECONDS;
    reverb->damping = 0.35f;
    reverb->mix = 0.0f;
    reverb->predelay_seconds = SYNTH_PLATE_REVERB_DEFAULT_PREDELAY_SECONDS;
    reverb->feedback = decay_feedback_for_seconds(
        reverb->decay_seconds,
        reverb->sample_rate);
    configure_reverb_lines(reverb, reverb->sample_rate);
}

void synth_plate_reverb_uninit(synth_plate_reverb *reverb)
{
    if (reverb == 0) {
        return;
    }

    free_delay_line(&reverb->predelay);
    for (size_t i = 0; i < SYNTH_PLATE_REVERB_INPUT_ALLPASS_COUNT; ++i) {
        free_allpass(&reverb->input_diffusers[i]);
    }
    free_tank(&reverb->tank);
}

void synth_plate_reverb_set_sample_rate(
    synth_plate_reverb *reverb,
    float sample_rate)
{
    reverb->sample_rate = sanitize_sample_rate(sample_rate);
    reverb->feedback = decay_feedback_for_seconds(
        reverb->decay_seconds,
        reverb->sample_rate);
    configure_reverb_lines(reverb, reverb->sample_rate);
    clear_reverb_history(reverb);
}

void synth_plate_reverb_set_decay(synth_plate_reverb *reverb, float seconds)
{
    reverb->decay_seconds = synth_clampf(
        seconds,
        SYNTH_PLATE_REVERB_MIN_DECAY_SECONDS,
        SYNTH_PLATE_REVERB_MAX_DECAY_SECONDS);
    reverb->feedback = decay_feedback_for_seconds(
        reverb->decay_seconds,
        reverb->sample_rate);
}

void synth_plate_reverb_set_damping(synth_plate_reverb *reverb, float damping)
{
    reverb->damping = synth_clampf(damping, 0.0f, 1.0f);
}

void synth_plate_reverb_set_mix(synth_plate_reverb *reverb, float mix)
{
    reverb->mix = synth_clampf(mix, 0.0f, 1.0f);
}

void synth_plate_reverb_set_predelay(
    synth_plate_reverb *reverb,
    float seconds)
{
    reverb->predelay_seconds = synth_clampf(
        seconds,
        0.0f,
        SYNTH_PLATE_REVERB_MAX_PREDELAY_SECONDS);
}

float synth_plate_reverb_get_decay(const synth_plate_reverb *reverb)
{
    return reverb->decay_seconds;
}

float synth_plate_reverb_get_damping(const synth_plate_reverb *reverb)
{
    return reverb->damping;
}

float synth_plate_reverb_get_mix(const synth_plate_reverb *reverb)
{
    return reverb->mix;
}

float synth_plate_reverb_get_predelay(const synth_plate_reverb *reverb)
{
    return reverb->predelay_seconds;
}

synth_stereo_sample synth_plate_reverb_process(
    synth_plate_reverb *reverb,
    synth_stereo_sample input)
{
    const float mono_input =
        ((input.left + input.right) * 0.5f) * SYNTH_PLATE_REVERB_INPUT_GAIN;
    float predelayed;
    float bandwidth_limited;
    float diffused;
    synth_stereo_sample wet;
    synth_stereo_sample output;

    if (reverb->mix == 0.0f || !reverb_has_storage(reverb)) {
        return input;
    }

    predelayed = process_predelay(reverb, mono_input);
    bandwidth_limited = process_one_pole(
        &reverb->bandwidth_filter,
        predelayed,
        SYNTH_PLATE_REVERB_INPUT_BANDWIDTH);
    diffused = process_input_diffusion(reverb, bandwidth_limited);
    wet = process_tank(reverb, diffused);

    output.left = mix_sample(input.left, wet.left, reverb->mix);
    output.right = mix_sample(input.right, wet.right, reverb->mix);
    return output;
}
