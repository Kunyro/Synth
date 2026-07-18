#include "synth/delay.h"
#include "synth/effect_chain.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

static void expect_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "FAIL: %s: got %.6f expected %.6f\n", message, actual, expected);
        ++failures;
    }
}

static void expect_sample_near(
    synth_stereo_sample actual,
    synth_stereo_sample expected,
    float tolerance,
    const char *message)
{
    expect_near(actual.left, expected.left, tolerance, message);
    expect_near(actual.right, expected.right, tolerance, message);
}

static synth_delay_voice *main_voice(synth_delay *delay)
{
    return &delay->voices[delay->main_voice_index];
}

static int count_voices_with_state(synth_delay *delay, synth_delay_voice_state state)
{
    int count = 0;

    for (size_t i = 0; i < SYNTH_DELAY_VOICE_COUNT; ++i) {
        if (delay->voices[i].state == state) {
            ++count;
        }
    }

    return count;
}

static synth_delay_voice *find_tail_voice_with_delay(synth_delay *delay, float delay_frames)
{
    for (size_t i = 0; i < SYNTH_DELAY_VOICE_COUNT; ++i) {
        synth_delay_voice *voice = &delay->voices[i];

        if (voice->state == SYNTH_DELAY_VOICE_TAIL &&
            fabsf(voice->line.delay_frames - delay_frames) < 0.0001f) {
            return voice;
        }
    }

    return 0;
}

static void test_delay_defaults_to_dry(void)
{
    synth_delay delay;
    const synth_stereo_sample input = {0.25f, -0.5f};
    synth_stereo_sample output;

    synth_delay_init(&delay, 48000.0f);
    output = synth_delay_process(&delay, input);

    expect_near(synth_delay_get_time(&delay), SYNTH_DELAY_DEFAULT_TIME_SECONDS, 0.0001f, "delay starts at default time");
    expect_near(synth_delay_get_feedback(&delay), 0.0f, 0.0001f, "delay starts with no feedback");
    expect_near(synth_delay_get_mix(&delay), 0.0f, 0.0001f, "delay starts dry");
    expect_sample_near(output, input, 0.0001f, "dry delay returns the input");
}

static void test_delay_parameters_are_bounded(void)
{
    synth_delay delay;

    synth_delay_init(&delay, 48000.0f);

    synth_delay_set_time(&delay, 0.0f);
    expect_near(
        synth_delay_get_time(&delay),
        SYNTH_DELAY_MIN_TIME_SECONDS,
        0.000001f,
        "delay time clamps to the minimum time");

    synth_delay_set_time(&delay, 10.0f);
    expect_near(synth_delay_get_time(&delay), SYNTH_DELAY_MAX_TIME_SECONDS, 0.0001f, "delay time clamps high");

    synth_delay_set_feedback(&delay, -1.0f);
    expect_near(synth_delay_get_feedback(&delay), 0.0f, 0.0001f, "delay feedback clamps low");

    synth_delay_set_feedback(&delay, 2.0f);
    expect_near(synth_delay_get_feedback(&delay), SYNTH_DELAY_MAX_FEEDBACK, 0.0001f, "delay feedback clamps high");

    synth_delay_set_mix(&delay, -1.0f);
    expect_near(synth_delay_get_mix(&delay), 0.0f, 0.0001f, "delay mix clamps low");

    synth_delay_set_mix(&delay, 2.0f);
    expect_near(synth_delay_get_mix(&delay), 1.0f, 0.0001f, "delay mix clamps high");
}

static void test_delay_outputs_an_impulse_after_delay_time(void)
{
    synth_delay delay;
    synth_stereo_sample output;

    synth_delay_init(&delay, 10.0f);
    synth_delay_set_time(&delay, 0.2f);
    synth_delay_set_mix(&delay, 1.0f);

    output = synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.25f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "wet delay starts silent");

    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "wet delay waits for delay frames");

    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(output, (synth_stereo_sample){1.0f, 0.25f}, 0.0001f, "wet delay outputs the delayed stereo impulse");
}

static void test_delay_feedback_repeats_decay(void)
{
    synth_delay delay;
    synth_stereo_sample output;

    synth_delay_init(&delay, 10.0f);
    synth_delay_set_time(&delay, 0.2f);
    synth_delay_set_feedback(&delay, 0.5f);
    synth_delay_set_mix(&delay, 1.0f);

    synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.0f});
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(output.left, 1.0f, 0.0001f, "first delay repeat is full impulse");

    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(output.left, 0.5f, 0.0001f, "feedback repeat decays");
}

static void test_delay_time_change_crossfades_after_audio_starts(void)
{
    synth_delay delay;
    const float original_frames = 1.0f * 1000.0f;
    const float target_frames = 0.01f * 1000.0f;

    synth_delay_init(&delay, 1000.0f);
    synth_delay_set_time(&delay, 1.0f);
    synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.0f});

    synth_delay_set_time(&delay, 0.01f);
    expect_near(main_voice(&delay)->line.delay_frames, original_frames, 0.0001f, "delay time change keeps the old main voice");
    expect_near(delay.pending_delay_frames, target_frames, 0.0001f, "delay time change stores the pending target");
    expect_near((float)delay.crossfading, 0.0f, 0.0001f, "delay time change waits for the knob to settle");

    delay.settle_frames_remaining = 1;
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(main_voice(&delay)->line.delay_frames, target_frames, 0.0001f, "delay time change starts a new main voice");
    expect_true(find_tail_voice_with_delay(&delay, original_frames) != 0, "delay time change keeps the old voice as a tail");
    expect_near((float)count_voices_with_state(&delay, SYNTH_DELAY_VOICE_TAIL), 1.0f, 0.0001f, "delay time change leaves one tail active");
    expect_near((float)delay.crossfade_position, 1.0f, 0.0001f, "delay time change advances the crossfade");
}

static void test_delay_time_retargets_without_restarting_crossfade(void)
{
    synth_delay delay;
    size_t position_before_retarget;

    synth_delay_init(&delay, 1000.0f);
    delay.settle_frames = 1;
    synth_delay_set_time(&delay, 1.0f);
    synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.0f});

    synth_delay_set_time(&delay, 0.5f);
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    position_before_retarget = delay.crossfade_position;

    synth_delay_set_time(&delay, 0.25f);
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(main_voice(&delay)->line.delay_frames, 250.0f, 0.0001f, "retarget updates the fresh main voice");
    if (!(delay.crossfade_position > position_before_retarget)) {
        fprintf(stderr, "FAIL: retarget keeps crossfade progress moving\n");
        ++failures;
    }
}

static void test_delay_time_crossfade_finishes_with_tail_voice(void)
{
    synth_delay delay;
    size_t first_main_voice_index;

    synth_delay_init(&delay, 1000.0f);
    delay.crossfade_frames = 2;
    delay.settle_frames = 1;
    synth_delay_set_time(&delay, 1.0f);
    synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.0f});
    first_main_voice_index = delay.main_voice_index;

    synth_delay_set_time(&delay, 0.5f);
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(main_voice(&delay)->line.delay_frames, 500.0f, 0.0001f, "finished crossfade keeps the new voice as main");
    expect_true(find_tail_voice_with_delay(&delay, 1000.0f) != 0, "finished crossfade keeps the old voice as a tail");
    expect_near((float)delay.crossfading, 0.0f, 0.0001f, "finished crossfade clears transition state");
    if (!(delay.main_voice_index != first_main_voice_index)) {
        fprintf(stderr, "FAIL: time change swaps the main voice\n");
        ++failures;
    }
}

static void test_delay_time_crossfade_does_not_feed_back_transition(void)
{
    synth_delay delay;
    synth_delay_voice *old_tail;

    synth_delay_init(&delay, 1000.0f);
    synth_delay_set_time(&delay, 0.1f);
    synth_delay_set_feedback(&delay, 0.5f);
    synth_delay_set_mix(&delay, 1.0f);

    main_voice(&delay)->line.write_index = 200;
    delay.has_processed = 1;
    delay.settle_frames = 1;
    main_voice(&delay)->line.left[100] = 1.0f;
    main_voice(&delay)->line.left[101] = 1.0f;

    synth_delay_set_time(&delay, 0.01f);
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    old_tail = find_tail_voice_with_delay(&delay, 100.0f);
    expect_true(old_tail != 0, "old delay voice becomes a tail");
    if (old_tail != 0) {
        expect_near(old_tail->line.left[201], 0.5f, 0.0001f, "old feedback stays in the tail voice");
    }
    expect_near(main_voice(&delay)->line.left[201], 0.0f, 0.0001f, "fresh main voice starts with clean history");
}

static void test_delay_time_change_keeps_tail_audible_after_crossfade(void)
{
    synth_delay delay;
    synth_stereo_sample output;

    synth_delay_init(&delay, 1000.0f);
    synth_delay_set_time(&delay, 0.1f);
    synth_delay_set_feedback(&delay, 0.5f);
    synth_delay_set_mix(&delay, 1.0f);

    main_voice(&delay)->line.write_index = 200;
    delay.crossfade_frames = 1;
    delay.has_processed = 1;
    delay.settle_frames = 1;
    main_voice(&delay)->line.left[100] = 1.0f;
    main_voice(&delay)->line.left[101] = 1.0f;

    synth_delay_set_time(&delay, 0.01f);
    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(output.left, 1.0f, 0.0001f, "old tail is audible during the handoff");

    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    expect_near(output.left, 1.0f, 0.0001f, "old tail remains audible after the handoff");
}

static void test_repeated_delay_time_changes_keep_existing_tail_voice(void)
{
    synth_delay delay;
    synth_stereo_sample output;

    synth_delay_init(&delay, 1000.0f);
    synth_delay_set_time(&delay, 0.1f);
    synth_delay_set_feedback(&delay, 0.5f);
    synth_delay_set_mix(&delay, 1.0f);

    main_voice(&delay)->line.write_index = 200;
    delay.crossfade_frames = 1;
    delay.has_processed = 1;
    delay.settle_frames = 1;
    main_voice(&delay)->line.left[100] = 1.0f;
    main_voice(&delay)->line.left[101] = 1.0f;

    synth_delay_set_time(&delay, 0.01f);
    synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});
    synth_delay_set_time(&delay, 0.02f);
    output = synth_delay_process(&delay, (synth_stereo_sample){0.0f, 0.0f});

    expect_true(find_tail_voice_with_delay(&delay, 100.0f) != 0, "second time change preserves the first tail voice");
    expect_near(output.left, 1.0f, 0.0001f, "first tail is still audible after another time change");
    expect_near((float)count_voices_with_state(&delay, SYNTH_DELAY_VOICE_TAIL), 2.0f, 0.0001f, "second time change keeps two tail voices");
}

static void test_delay_sample_rate_change_updates_tap_immediately(void)
{
    synth_delay delay;

    synth_delay_init(&delay, 1000.0f);
    synth_delay_set_time(&delay, 0.25f);
    synth_delay_process(&delay, (synth_stereo_sample){1.0f, 0.0f});

    synth_delay_set_sample_rate(&delay, 2000.0f);
    expect_near(main_voice(&delay)->line.delay_frames, 500.0f, 0.0001f, "sample rate change updates main voice");
    expect_near((float)delay.crossfading, 0.0f, 0.0001f, "sample rate change clears crossfade");
    expect_near((float)count_voices_with_state(&delay, SYNTH_DELAY_VOICE_TAIL), 0.0f, 0.0001f, "sample rate change clears tail voices");
    expect_near((float)delay.has_pending_time_change, 0.0f, 0.0001f, "sample rate change clears pending time");
}

static void test_effect_chain_runs_delay_after_distortion(void)
{
    synth_effect_chain chain;
    synth_stereo_sample output;
    synth_stereo_sample expected;

    synth_effect_chain_init(&chain, 10.0f);
    synth_distortion_set_drive(&chain.distortion, 2.0f);
    synth_distortion_set_mix(&chain.distortion, 1.0f);
    synth_delay_set_time(&chain.delay, 0.1f);
    synth_delay_set_mix(&chain.delay, 1.0f);

    expected = synth_distortion_process(
        &chain.distortion,
        (synth_stereo_sample){0.5f, -0.5f});

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.5f, -0.5f});
    expect_sample_near(output, (synth_stereo_sample){0.0f, 0.0f}, 0.0001f, "delay hides the first wet sample");

    output = synth_effect_chain_process(&chain, (synth_stereo_sample){0.0f, 0.0f});
    expect_sample_near(output, expected, 0.0001f, "delay repeats the distorted sample");
}

int main(void)
{
    test_delay_defaults_to_dry();
    test_delay_parameters_are_bounded();
    test_delay_outputs_an_impulse_after_delay_time();
    test_delay_feedback_repeats_decay();
    test_delay_time_change_crossfades_after_audio_starts();
    test_delay_time_retargets_without_restarting_crossfade();
    test_delay_time_crossfade_finishes_with_tail_voice();
    test_delay_time_crossfade_does_not_feed_back_transition();
    test_delay_time_change_keeps_tail_audible_after_crossfade();
    test_repeated_delay_time_changes_keep_existing_tail_voice();
    test_delay_sample_rate_change_updates_tap_immediately();
    test_effect_chain_runs_delay_after_distortion();

    if (failures != 0) {
        fprintf(stderr, "%d delay test(s) failed\n", failures);
        return 1;
    }

    printf("All delay tests passed.\n");
    return 0;
}
