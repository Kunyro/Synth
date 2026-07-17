#include "synth/delay.h"

#include <string.h>

#include "../internal/synth_internal.h"

static size_t delay_frames_for_time(float sample_rate, float seconds)
{
    size_t frames = (size_t)((seconds * sample_rate) + 0.5f);

    if (frames < 1) {
        return 1;
    }

    if (frames > SYNTH_DELAY_MAX_FRAMES) {
        return SYNTH_DELAY_MAX_FRAMES;
    }

    return frames;
}

static size_t wrapped_read_index(size_t write_index, size_t delay_frames)
{
    return (write_index + SYNTH_DELAY_MAX_FRAMES - delay_frames) % SYNTH_DELAY_MAX_FRAMES;
}

static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

void synth_delay_init(synth_delay *delay, float sample_rate)
{
    memset(delay, 0, sizeof(*delay));
    delay->sample_rate = sample_rate;
    synth_delay_set_time(delay, SYNTH_DELAY_DEFAULT_TIME_SECONDS);
    delay->feedback = 0.0f;
    delay->mix = 0.0f;
}

void synth_delay_set_sample_rate(synth_delay *delay, float sample_rate)
{
    delay->sample_rate = sample_rate;
    synth_delay_set_time(delay, delay->time_seconds);
}

void synth_delay_set_time(synth_delay *delay, float seconds)
{
    const float clamped_seconds = synth_clampf(
        seconds,
        SYNTH_DELAY_MIN_TIME_SECONDS,
        SYNTH_DELAY_MAX_TIME_SECONDS);

    delay->delay_frames = delay_frames_for_time(delay->sample_rate, clamped_seconds);
    delay->time_seconds = (float)delay->delay_frames / delay->sample_rate;
}

void synth_delay_set_feedback(synth_delay *delay, float feedback)
{
    delay->feedback = synth_clampf(feedback, 0.0f, SYNTH_DELAY_MAX_FEEDBACK);
}

void synth_delay_set_mix(synth_delay *delay, float mix)
{
    delay->mix = synth_clampf(mix, 0.0f, 1.0f);
}

float synth_delay_get_time(const synth_delay *delay)
{
    return delay->time_seconds;
}

float synth_delay_get_feedback(const synth_delay *delay)
{
    return delay->feedback;
}

float synth_delay_get_mix(const synth_delay *delay)
{
    return delay->mix;
}

synth_stereo_sample synth_delay_process(
    synth_delay *delay,
    synth_stereo_sample input)
{
    const size_t read_index = wrapped_read_index(delay->write_index, delay->delay_frames);
    const synth_stereo_sample delayed = {
        delay->left[read_index],
        delay->right[read_index]
    };
    synth_stereo_sample output;

    delay->left[delay->write_index] = input.left + (delayed.left * delay->feedback);
    delay->right[delay->write_index] = input.right + (delayed.right * delay->feedback);

    output.left = mix_sample(input.left, delayed.left, delay->mix);
    output.right = mix_sample(input.right, delayed.right, delay->mix);

    delay->write_index = (delay->write_index + 1) % SYNTH_DELAY_MAX_FRAMES;
    return output;
}
