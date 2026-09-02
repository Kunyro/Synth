#include "synth/chorus.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../internal/synth_internal.h"

#define SYNTH_CHORUS_OUTPUT_COMPENSATION 0.65f
#define SYNTH_CHORUS_TWO_PI 6.28318530717958647692f

// starts each chorus voice at a different point in its sweep
static const float voice_phase_offsets[SYNTH_CHORUS_VOICE_COUNT] = {
    0.0f,
    0.33333334f,
    0.66666669f
};

// lets each voice move at a slightly different speed for a wider sound
static const float voice_rate_multipliers[SYNTH_CHORUS_VOICE_COUNT] = {
    0.83f,
    1.0f,
    1.37f
};

// keeps the sample rate from becoming zero or negative
static float sanitize_sample_rate(float sample_rate)
{
    return sample_rate >= SYNTH_CHORUS_MIN_SAMPLE_RATE ?
        sample_rate :
        SYNTH_CHORUS_MIN_SAMPLE_RATE;
}

// works out how much memory the chorus needs for its short echoes
static size_t max_delay_frames_for_sample_rate(float sample_rate)
{
    const float max_seconds =
        SYNTH_CHORUS_MAX_DELAY_SECONDS + SYNTH_CHORUS_MAX_MODULATION_SECONDS;
    const size_t frames = (size_t)((max_seconds * sample_rate) + 3.0f);

    return frames < 2 ? 2 : frames;
}

// checks whether the chorus has working left and right echo buffers
static int delay_line_has_storage(const synth_chorus_delay_line *line)
{
    return line->left != 0 && line->right != 0 && line->capacity_frames > 1;
}

// creates empty left and right echo buffers for the chorus
static int allocate_delay_line(synth_chorus_delay_line *line, size_t capacity_frames)
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

// releases the echo buffers and marks the delay line as empty
static void free_delay_line(synth_chorus_delay_line *line)
{
    free(line->left);
    free(line->right);
    line->left = 0;
    line->right = 0;
    line->write_index = 0;
    line->capacity_frames = 0;
}

// rebuilds the echo buffers when the sample rate changes
static void resize_delay_line(synth_chorus_delay_line *line, size_t capacity_frames)
{
    free_delay_line(line);
    (void)allocate_delay_line(line, capacity_frames);
}

// wraps a read position back around the circular echo buffer
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

// blends between two neighboring samples for smoother moving echoes
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

// reads an older sample from the echo buffer, including in-between positions
static float read_delay_sample(
    const float *buffer,
    size_t capacity_frames,
    size_t write_index,
    float delay_frames)
{
    const float bounded_delay = synth_clampf(
        delay_frames,
        1.0f,
        (float)(capacity_frames - 1));
    const float read_position = wrap_position(
        (float)write_index - bounded_delay,
        capacity_frames);

    return interpolate_sample(buffer, capacity_frames, read_position);
}

// turns a moving phase value into a smooth wave from low to high and back
static float lfo_value_at_phase(float phase)
{
    const float wrapped = synth_wrap_phase(phase);

    return sinf(wrapped * SYNTH_CHORUS_TWO_PI);
}

// converts the depth knob into how far the chorus delay can move
static float modulation_seconds(const synth_chorus *chorus)
{
    return SYNTH_CHORUS_MAX_MODULATION_SECONDS * chorus->depth;
}

// finds the current echo time for one chorus voice
static float delay_seconds_for_voice(
    const synth_chorus *chorus,
    size_t voice_index,
    float channel_offset)
{
    const float phase = chorus->phases[voice_index] +
        voice_phase_offsets[voice_index] +
        channel_offset;
    const float bipolar = lfo_value_at_phase(phase);
    const float unipolar = (bipolar + 1.0f) * 0.5f;

    return chorus->delay_seconds + (modulation_seconds(chorus) * unipolar);
}

// moves every chorus voice forward by one audio sample
static void advance_phases(synth_chorus *chorus)
{
    for (size_t i = 0; i < SYNTH_CHORUS_VOICE_COUNT; ++i) {
        chorus->phases[i] = synth_wrap_phase(
            chorus->phases[i] +
            ((chorus->rate_hz * voice_rate_multipliers[i]) / chorus->sample_rate));
    }
}

// combines the original sound with the chorus sound while keeping level sensible
static float mix_sample(float dry, float wet, float mix)
{
    const float compensated_gain = 1.0f + (mix * SYNTH_CHORUS_OUTPUT_COMPENSATION);

    return (dry + (wet * mix)) / compensated_gain;
}

// prepares a chorus effect with default settings and empty echo buffers
void synth_chorus_init(synth_chorus *chorus, float sample_rate)
{
    memset(chorus, 0, sizeof(*chorus));
    chorus->sample_rate = sanitize_sample_rate(sample_rate);
    chorus->rate_hz = SYNTH_CHORUS_DEFAULT_RATE_HZ;
    chorus->depth = SYNTH_CHORUS_DEFAULT_DEPTH;
    chorus->mix = 0.0f;
    chorus->width = SYNTH_CHORUS_DEFAULT_WIDTH;
    chorus->delay_seconds = SYNTH_CHORUS_DEFAULT_DELAY_SECONDS;
    chorus->feedback = 0.0f;

    for (size_t i = 0; i < SYNTH_CHORUS_VOICE_COUNT; ++i) {
        chorus->phases[i] = voice_phase_offsets[i];
    }

    (void)allocate_delay_line(
        &chorus->delay,
        max_delay_frames_for_sample_rate(chorus->sample_rate));
}

// cleans up memory owned by the chorus effect
void synth_chorus_uninit(synth_chorus *chorus)
{
    if (chorus == 0) {
        return;
    }

    free_delay_line(&chorus->delay);
}

// updates the sample rate and rebuilds the echo buffers to match it
void synth_chorus_set_sample_rate(synth_chorus *chorus, float sample_rate)
{
    chorus->sample_rate = sanitize_sample_rate(sample_rate);
    resize_delay_line(
        &chorus->delay,
        max_delay_frames_for_sample_rate(chorus->sample_rate));
}

// sets how fast the chorus voices drift back and forth
void synth_chorus_set_rate(synth_chorus *chorus, float hz)
{
    chorus->rate_hz = synth_clampf(
        hz,
        SYNTH_CHORUS_MIN_RATE_HZ,
        SYNTH_CHORUS_MAX_RATE_HZ);
}

void synth_chorus_set_depth(synth_chorus *chorus, float depth)
{
    chorus->depth = synth_clampf(depth, 0.0f, 1.0f);
}

void synth_chorus_set_mix(synth_chorus *chorus, float mix)
{
    chorus->mix = synth_clampf(mix, 0.0f, 1.0f);
}

void synth_chorus_set_width(synth_chorus *chorus, float width)
{
    chorus->width = synth_clampf(width, 0.0f, 1.0f);
}

void synth_chorus_set_delay(synth_chorus *chorus, float seconds)
{
    chorus->delay_seconds = synth_clampf(
        seconds,
        SYNTH_CHORUS_MIN_DELAY_SECONDS,
        SYNTH_CHORUS_MAX_DELAY_SECONDS);
}

void synth_chorus_set_feedback(synth_chorus *chorus, float feedback)
{
    chorus->feedback = synth_clampf(
        feedback,
        -SYNTH_CHORUS_MAX_FEEDBACK,
        SYNTH_CHORUS_MAX_FEEDBACK);
}

float synth_chorus_get_rate(const synth_chorus *chorus)
{
    return chorus->rate_hz;
}

float synth_chorus_get_depth(const synth_chorus *chorus)
{
    return chorus->depth;
}

float synth_chorus_get_mix(const synth_chorus *chorus)
{
    return chorus->mix;
}

float synth_chorus_get_width(const synth_chorus *chorus)
{
    return chorus->width;
}

float synth_chorus_get_delay(const synth_chorus *chorus)
{
    return chorus->delay_seconds;
}

float synth_chorus_get_feedback(const synth_chorus *chorus)
{
    return chorus->feedback;
}

// processes one stereo sample through the chorus
synth_stereo_sample synth_chorus_process(
    synth_chorus *chorus,
    synth_stereo_sample input)
{
    synth_stereo_sample wet = {0.0f, 0.0f};
    synth_stereo_sample output;

    if (!delay_line_has_storage(&chorus->delay)) {
        return input;
    }

    // gather the delayed sound from each chorus voice.
    for (size_t i = 0; i < SYNTH_CHORUS_VOICE_COUNT; ++i) {
        const float channel_offset = 0.25f * chorus->width;
        const float left_delay_frames = delay_seconds_for_voice(
            chorus,
            i,
            0.0f) * chorus->sample_rate;
        const float right_delay_frames = delay_seconds_for_voice(
            chorus,
            i,
            channel_offset) * chorus->sample_rate;

        wet.left += read_delay_sample(
            chorus->delay.left,
            chorus->delay.capacity_frames,
            chorus->delay.write_index,
            left_delay_frames);
        wet.right += read_delay_sample(
            chorus->delay.right,
            chorus->delay.capacity_frames,
            chorus->delay.write_index,
            right_delay_frames);
    }

    // average the voices so the chorus stays controlled instead of just louder.
    wet.left /= (float)SYNTH_CHORUS_VOICE_COUNT;
    wet.right /= (float)SYNTH_CHORUS_VOICE_COUNT;

    // save the new sound so future samples can hear it as a short moving echo.
    chorus->delay.left[chorus->delay.write_index] =
        input.left + (wet.left * chorus->feedback);
    chorus->delay.right[chorus->delay.write_index] =
        input.right + (wet.right * chorus->feedback);
    chorus->delay.write_index =
        (chorus->delay.write_index + 1) % chorus->delay.capacity_frames;

    // move the chorus voices forward so the next sample uses slightly new timing.
    advance_phases(chorus);

    // blend the original sound with the chorus sound.
    output.left = mix_sample(input.left, wet.left, chorus->mix);
    output.right = mix_sample(input.right, wet.right, chorus->mix);
    return output;
}
