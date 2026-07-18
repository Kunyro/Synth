#include "synth/delay.h"

#include <math.h>
#include <string.h>

#include "../internal/synth_internal.h"

// converts seconds into the circular-buffer distance used by the read head.
static float delay_frames_for_time(float sample_rate, float seconds)
{
    const float frames = seconds * sample_rate;

    return synth_clampf(frames, 1.0f, (float)SYNTH_DELAY_MAX_FRAMES);
}

// turns the crossfade time into a sample count.
static size_t crossfade_frames_for_sample_rate(float sample_rate)
{
    const size_t frames = (size_t)((SYNTH_DELAY_TIME_CROSSFADE_SECONDS * sample_rate) + 0.5f);

    if (frames < 1) {
        return 1;
    }

    return frames;
}

// turns the knob-settle time into a sample count.
static size_t settle_frames_for_sample_rate(float sample_rate)
{
    const size_t frames = (size_t)((SYNTH_DELAY_TIME_SETTLE_SECONDS * sample_rate) + 0.5f);

    if (frames < 1) {
        return 1;
    }

    return frames;
}

// wraps fractional read positions back into the delay buffer.
static float wrap_read_position(float position)
{
    while (position < 0.0f) {
        position += (float)SYNTH_DELAY_MAX_FRAMES;
    }

    while (position >= (float)SYNTH_DELAY_MAX_FRAMES) {
        position -= (float)SYNTH_DELAY_MAX_FRAMES;
    }

    return position;
}

// reads between samples so delay taps are not limited to whole frames.
static float interpolate_sample(const float *buffer, float position)
{
    const size_t first_index = (size_t)position;
    const size_t second_index = (first_index + 1) % SYNTH_DELAY_MAX_FRAMES;
    const float fraction = position - (float)first_index;

    return buffer[first_index] + ((buffer[second_index] - buffer[first_index]) * fraction);
}

// reads the delayed stereo sample from one independent delay line.
static synth_stereo_sample read_delay_line(const synth_delay_line *line)
{
    const float read_position = wrap_read_position(
        (float)line->write_index - line->delay_frames);
    synth_stereo_sample delayed;

    delayed.left = interpolate_sample(line->left, read_position);
    delayed.right = interpolate_sample(line->right, read_position);
    return delayed;
}

// clears a line so a new delay time starts with its own clean history.
static void reset_delay_line(synth_delay_line *line, float delay_frames, size_t write_index)
{
    memset(line, 0, sizeof(*line));
    line->delay_frames = delay_frames;
    line->write_index = write_index;
}

// prepares a voice for a new role in the delay engine.
static void reset_delay_voice(
    synth_delay_voice *voice,
    float delay_frames,
    size_t write_index,
    synth_delay_voice_state state,
    float output_gain)
{
    reset_delay_line(&voice->line, delay_frames, write_index);
    voice->state = state;
    voice->quiet_frames = 0;
    voice->output_gain = output_gain;
    voice->last_level = 0.0f;
}

// advances one delay line and returns its wet sample.
static synth_stereo_sample process_delay_line(
    synth_delay_line *line,
    synth_stereo_sample input,
    float feedback)
{
    const synth_stereo_sample delayed = read_delay_line(line);

    line->left[line->write_index] = input.left + (delayed.left * feedback);
    line->right[line->write_index] = input.right + (delayed.right * feedback);
    line->write_index = (line->write_index + 1) % SYNTH_DELAY_MAX_FRAMES;

    return delayed;
}

// blends from fully dry to fully delayed.
static float mix_sample(float dry, float wet, float mix)
{
    return dry + ((wet - dry) * mix);
}

static float sample_level(synth_stereo_sample sample)
{
    const float left = fabsf(sample.left);
    const float right = fabsf(sample.right);

    return left > right ? left : right;
}

static synth_stereo_sample scaled_sample(synth_stereo_sample sample, float gain)
{
    sample.left *= gain;
    sample.right *= gain;
    return sample;
}

static synth_stereo_sample add_samples(synth_stereo_sample a, synth_stereo_sample b)
{
    a.left += b.left;
    a.right += b.right;
    return a;
}

static synth_delay_voice *main_delay_voice(synth_delay *delay)
{
    return &delay->voices[delay->main_voice_index];
}

static int voice_is_recyclable(const synth_delay_voice *voice)
{
    return voice->state == SYNTH_DELAY_VOICE_INACTIVE;
}

// tails need a full delay interval before silence proves they are done.
static size_t quiet_frame_limit_for_voice(const synth_delay_voice *voice)
{
    const size_t delay_frames = (size_t)(voice->line.delay_frames + 1.0f);

    if (delay_frames < 1) {
        return 1;
    }

    return delay_frames;
}

// marks quiet tails inactive without touching their buffers.
static void retire_quiet_tail(synth_delay_voice *voice)
{
    if (voice->state != SYNTH_DELAY_VOICE_TAIL) {
        return;
    }

    if (voice->quiet_frames > quiet_frame_limit_for_voice(voice)) {
        voice->state = SYNTH_DELAY_VOICE_INACTIVE;
        voice->output_gain = 0.0f;
    }
}

// inactive voices are preferred, then the quietest old tail is recycled.
static size_t choose_voice_for_new_main(synth_delay *delay)
{
    size_t quietest_tail_index = delay->main_voice_index;
    size_t quietest_tail_frames = 0;
    float quietest_tail_level = 0.0f;
    int found_tail = 0;

    for (size_t i = 0; i < SYNTH_DELAY_VOICE_COUNT; ++i) {
        const synth_delay_voice *voice = &delay->voices[i];

        if (i == delay->main_voice_index) {
            continue;
        }

        if (voice_is_recyclable(voice)) {
            return i;
        }

        if (voice->state == SYNTH_DELAY_VOICE_TAIL &&
            (!found_tail ||
                voice->quiet_frames > quietest_tail_frames ||
                (voice->quiet_frames == quietest_tail_frames &&
                    voice->last_level < quietest_tail_level))) {
            quietest_tail_index = i;
            quietest_tail_frames = voice->quiet_frames;
            quietest_tail_level = voice->last_level;
            found_tail = 1;
        }
    }

    return quietest_tail_index;
}

// turns the old main delay into a tail and starts a fresh main delay.
static void start_delay_time_transition(synth_delay *delay, float target_delay_frames)
{
    synth_delay_voice *old_main = main_delay_voice(delay);
    const size_t new_main_index = choose_voice_for_new_main(delay);
    const size_t aligned_write_index = old_main->line.write_index;

    delay->crossfade_position = 0;
    delay->crossfading = delay->crossfade_frames > 0 &&
        old_main->line.delay_frames != target_delay_frames;

    if (!delay->crossfading) {
        old_main->line.delay_frames = target_delay_frames;
        old_main->output_gain = 1.0f;
        return;
    }

    old_main->state = SYNTH_DELAY_VOICE_TAIL;
    old_main->output_gain = 1.0f;
    old_main->quiet_frames = 0;

    reset_delay_voice(
        &delay->voices[new_main_index],
        target_delay_frames,
        aligned_write_index,
        SYNTH_DELAY_VOICE_MAIN,
        0.0f);
    delay->main_voice_index = new_main_index;
}

// retargets the fresh main delay without creating another tail.
static void retarget_delay_time_transition(synth_delay *delay, float target_delay_frames)
{
    synth_delay_voice *main = main_delay_voice(delay);
    const float output_gain = main->output_gain;
    const size_t write_index = main->line.write_index;

    if (main->line.delay_frames == target_delay_frames) {
        return;
    }

    reset_delay_voice(
        main,
        target_delay_frames,
        write_index,
        SYNTH_DELAY_VOICE_MAIN,
        output_gain);
}

// installs a delay time without leaving any transition state behind.
static void set_delay_time_immediately(synth_delay *delay, float delay_frames)
{
    for (size_t i = 0; i < SYNTH_DELAY_VOICE_COUNT; ++i) {
        reset_delay_voice(
            &delay->voices[i],
            delay_frames,
            0,
            SYNTH_DELAY_VOICE_INACTIVE,
            0.0f);
    }

    delay->main_voice_index = 0;
    delay->voices[delay->main_voice_index].state = SYNTH_DELAY_VOICE_MAIN;
    delay->voices[delay->main_voice_index].output_gain = 1.0f;
    delay->pending_delay_frames = delay_frames;
    delay->crossfade_position = 0;
    delay->settle_frames_remaining = 0;
    delay->crossfading = 0;
    delay->has_pending_time_change = 0;
}

// applies a settled knob value after midi has stopped sending changes.
static void apply_pending_delay_time(synth_delay *delay)
{
    if (!delay->has_pending_time_change) {
        return;
    }

    if (delay->settle_frames_remaining > 0) {
        --delay->settle_frames_remaining;
        if (delay->settle_frames_remaining > 0) {
            return;
        }
    }

    if (delay->crossfading) {
        retarget_delay_time_transition(delay, delay->pending_delay_frames);
    } else {
        start_delay_time_transition(delay, delay->pending_delay_frames);
    }

    delay->has_pending_time_change = 0;
}

// finishes the main fade-in while old tails keep decaying.
static void advance_main_fade(synth_delay *delay)
{
    synth_delay_voice *main = main_delay_voice(delay);

    if (!delay->crossfading) {
        main->output_gain = 1.0f;
        return;
    }

    main->output_gain = (float)delay->crossfade_position / (float)delay->crossfade_frames;
    ++delay->crossfade_position;

    if (delay->crossfade_position >= delay->crossfade_frames) {
        main->output_gain = 1.0f;
        delay->crossfade_position = 0;
        delay->crossfading = 0;
    }
}

// processes every active delay voice with separate feedback histories.
static synth_stereo_sample process_delay_voices(synth_delay *delay, synth_stereo_sample input)
{
    const synth_stereo_sample silence = {0.0f, 0.0f};
    synth_stereo_sample output = {0.0f, 0.0f};

    advance_main_fade(delay);

    for (size_t i = 0; i < SYNTH_DELAY_VOICE_COUNT; ++i) {
        synth_delay_voice *voice = &delay->voices[i];
        synth_stereo_sample voice_input;
        synth_stereo_sample voice_output;

        if (voice->state == SYNTH_DELAY_VOICE_INACTIVE) {
            continue;
        }

        voice_input = voice->state == SYNTH_DELAY_VOICE_MAIN ? input : silence;
        voice_output = process_delay_line(&voice->line, voice_input, delay->feedback);
        voice->last_level = sample_level(voice_output);
        if (voice->last_level <= SYNTH_DELAY_TAIL_SILENCE_THRESHOLD) {
            ++voice->quiet_frames;
        } else {
            voice->quiet_frames = 0;
        }

        output = add_samples(output, scaled_sample(voice_output, voice->output_gain));
        retire_quiet_tail(voice);
    }

    return output;
}

void synth_delay_init(synth_delay *delay, float sample_rate)
{
    memset(delay, 0, sizeof(*delay));
    delay->sample_rate = sample_rate;
    delay->crossfade_frames = crossfade_frames_for_sample_rate(sample_rate);
    delay->settle_frames = settle_frames_for_sample_rate(sample_rate);
    synth_delay_set_time(delay, SYNTH_DELAY_DEFAULT_TIME_SECONDS);
    delay->feedback = 0.0f;
    delay->mix = 0.0f;
}

void synth_delay_set_sample_rate(synth_delay *delay, float sample_rate)
{
    const float current_time_seconds = delay->time_seconds;
    float delay_frames;

    delay->sample_rate = sample_rate;
    delay->crossfade_frames = crossfade_frames_for_sample_rate(sample_rate);
    delay->settle_frames = settle_frames_for_sample_rate(sample_rate);
    delay_frames = delay_frames_for_time(delay->sample_rate, current_time_seconds);
    delay->time_seconds = delay_frames / delay->sample_rate;
    set_delay_time_immediately(delay, delay_frames);
}

void synth_delay_set_time(synth_delay *delay, float seconds)
{
    const float clamped_seconds = synth_clampf(
        seconds,
        SYNTH_DELAY_MIN_TIME_SECONDS,
        SYNTH_DELAY_MAX_TIME_SECONDS);
    const float requested_delay_frames = delay_frames_for_time(delay->sample_rate, clamped_seconds);

    delay->time_seconds = requested_delay_frames / delay->sample_rate;

    // before audio starts, the delay can jump straight to its first setting.
    if (!delay->has_processed) {
        set_delay_time_immediately(delay, requested_delay_frames);
    } else {
        // while the knob is moving, only remember the newest destination.
        delay->pending_delay_frames = requested_delay_frames;
        delay->settle_frames_remaining = delay->settle_frames;
        delay->has_pending_time_change = 1;
    }
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
    synth_stereo_sample output_delayed;
    synth_stereo_sample output;

    // wait for the time knob to stop moving before changing delay voices.
    apply_pending_delay_time(delay);

    output_delayed = process_delay_voices(delay, input);

    output.left = mix_sample(input.left, output_delayed.left, delay->mix);
    output.right = mix_sample(input.right, output_delayed.right, delay->mix);

    delay->has_processed = 1;
    return output;
}
