#include "synth/synth.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

// records a failed condition while allowing the remaining modulation checks to run
static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

// checks both finiteness and numerical agreement; a nan must never pass a tolerance check
static void near(float actual, float expected, float tolerance, const char *message)
{
    check(isfinite(actual) && fabsf(actual - expected) <= tolerance, message);
}

// explicit inventory: a missing catalog entry cannot disappear from the test too
static const synth_parameter_id targets[] = {
    SYNTH_PARAM_ATTACK, SYNTH_PARAM_DECAY, SYNTH_PARAM_SUSTAIN, SYNTH_PARAM_RELEASE,
    SYNTH_PARAM_MASTER_GAIN, SYNTH_PARAM_FILTER_CUTOFF, SYNTH_PARAM_FILTER_POLES,
    SYNTH_PARAM_OSCILLATOR_MORPH, SYNTH_PARAM_FIRST_OSCILLATOR_GAIN,
    SYNTH_PARAM_SECOND_OSCILLATOR_GAIN, SYNTH_PARAM_SECOND_OSCILLATOR_MORPH,
    SYNTH_PARAM_SECOND_OSCILLATOR_OCTAVE, SYNTH_PARAM_SECOND_OSCILLATOR_PITCH,
    SYNTH_PARAM_SECOND_OSCILLATOR_FINE_TUNE, SYNTH_PARAM_STEREO_SPREAD,
    SYNTH_PARAM_SATURATION_DRIVE, SYNTH_PARAM_SATURATION_MIX,
    SYNTH_PARAM_DISTORTION_DRIVE, SYNTH_PARAM_DISTORTION_MIX,
    SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE, SYNTH_PARAM_BITCRUSHER_BITS, SYNTH_PARAM_BITCRUSHER_MIX,
    SYNTH_PARAM_FLANGER_RATE, SYNTH_PARAM_FLANGER_INTENSITY, SYNTH_PARAM_FLANGER_DEPTH,
    SYNTH_PARAM_FLANGER_FEEDBACK, SYNTH_PARAM_FLANGER_MIX, SYNTH_PARAM_FLANGER_MANUAL,
    SYNTH_PARAM_RING_MOD_FREQUENCY, SYNTH_PARAM_RING_MOD_RECTIFY, SYNTH_PARAM_RING_MOD_MIX,
    SYNTH_PARAM_CHORUS_RATE, SYNTH_PARAM_CHORUS_DEPTH, SYNTH_PARAM_CHORUS_MIX,
    SYNTH_PARAM_CHORUS_WIDTH, SYNTH_PARAM_CHORUS_DELAY, SYNTH_PARAM_CHORUS_FEEDBACK,
    SYNTH_PARAM_EQ_LOW, SYNTH_PARAM_EQ_MID, SYNTH_PARAM_EQ_HIGH,
    SYNTH_PARAM_DELAY_TIME, SYNTH_PARAM_DELAY_FEEDBACK, SYNTH_PARAM_DELAY_MIX,
    SYNTH_PARAM_PLATE_REVERB_DECAY, SYNTH_PARAM_PLATE_REVERB_DAMPING,
    SYNTH_PARAM_PLATE_REVERB_MIX, SYNTH_PARAM_PLATE_REVERB_PREDELAY,
    SYNTH_PARAM_COMPRESSOR_THRESHOLD, SYNTH_PARAM_COMPRESSOR_RATIO,
    SYNTH_PARAM_COMPRESSOR_MAKEUP_GAIN, SYNTH_PARAM_COMPRESSOR_ATTACK_SECONDS,
    SYNTH_PARAM_COMPRESSOR_RELEASE_SECONDS
};

// checks every catalog entry and the shared signed, linear/logarithmic, and stepping rules
static void test_catalog_and_evaluation(void)
{
    synth s;
    size_t eligible = 0;
    synth_init(&s, 48000.0f);
    check(sizeof(targets) / sizeof(targets[0]) == 52, "52 explicit destinations");
    for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) {
        const synth_parameter_info *info = synth_parameter_info_at((synth_parameter_id)i);
        int found = 0;
        check(info != NULL && info->name != NULL && info->id == (synth_parameter_id)i, "complete indexed catalog");
        check(synth_parameter_info_by_name(info->name) == info, "unique canonical name");
        check(info->min_value <= info->max_value, "ordered parameter bounds");
        for (size_t j = 0; j < sizeof(targets) / sizeof(targets[0]); ++j) {
            found += targets[j] == (synth_parameter_id)i;
        }
        check(found == info->modulatable, "exact destination eligibility");
        eligible += info->modulatable;
        near(synth_get_lfo_amount(&s, info->id), 0, 0, "every route starts at zero");
        if (info->modulatable) {
            const float base = synth_get_parameter(&s, info->id);
            check(info->modulation_span > 0 && isfinite(info->modulation_span), "finite positive span");
            check(synth_set_lfo_amount(&s, info->id, 1), "every destination accepts a route");
            near(synth_modulate_value(&s, info->id, base, 1), base, 0, "zero global depth is exact identity");
        } else {
            check(!synth_set_lfo_amount(&s, info->id, 1), "global LFO controls reject routing");
        }
    }
    check(eligible == 52, "catalog has exactly 52 eligible destinations");
    synth_set_lfo_depth(&s, 1);
    // mix has a 0.5 native-unit span: 0.5 +/- (0.4 * 0.5) gives 0.7 and 0.3
    synth_set_lfo_amount(&s, SYNTH_PARAM_DELAY_MIX, 0.4f);
    near(synth_modulate_value(&s, SYNTH_PARAM_DELAY_MIX, 0.5f, 1), 0.7f, 1e-6f, "centered linear excursion");
    near(synth_modulate_value(&s, SYNTH_PARAM_DELAY_MIX, 0.5f, -1), 0.3f, 1e-6f, "negative source excursion");
    synth_set_lfo_amount(&s, SYNTH_PARAM_DELAY_MIX, -0.4f);
    near(synth_modulate_value(&s, SYNTH_PARAM_DELAY_MIX, 0.5f, 1), 0.3f, 1e-6f, "negative amount reverses direction");
    near(synth_modulate_value(&s, SYNTH_PARAM_DELAY_MIX, 0.05f, 1), 0, 0, "legal bounds clamp excursion");
    synth_set_lfo_amount(&s, SYNTH_PARAM_FILTER_CUTOFF, 0.2f);
    // cutoff spans five octaves, so amount 0.2 moves one octave: 1000 hz doubles
    near(synth_modulate_value(&s, SYNTH_PARAM_FILTER_CUTOFF, 1000, 1), 2000, 0.001f, "frequency uses octaves");
    near(synth_modulate_value(&s, SYNTH_PARAM_FILTER_CUTOFF, 20000, 1), 24000, 0.001f, "cutoff clamps at Nyquist");
    synth_set_lfo_amount(&s, SYNTH_PARAM_SECOND_OSCILLATOR_PITCH, -1);
    // -1 * 0.3 * 6 semitones is -1.8, which must snap to -2 rather than truncate to -1
    near(synth_modulate_value(&s, SYNTH_PARAM_SECOND_OSCILLATOR_PITCH, 0, 0.3f), -2, 0, "negative tuning rounds symmetrically");
    synth_set_lfo_amount(&s, SYNTH_PARAM_SECOND_OSCILLATOR_FINE_TUNE, 1);
    near(synth_modulate_value(&s, SYNTH_PARAM_SECOND_OSCILLATOR_FINE_TUNE, 0, 0.013f), 0.65f, 1e-5f, "fine tune stays continuous");
    check(!synth_set_lfo_amount(&s, SYNTH_PARAM_DELAY_MIX, NAN), "NaN amount rejected");
    check(!synth_set_lfo_amount(&s, SYNTH_PARAM_COUNT, 1), "out-of-range destination rejected");
    check(!synth_set_lfo_amount(&s, (synth_parameter_id)-1, 1), "negative destination rejected");
    check(!synth_set_parameter(&s, SYNTH_PARAM_DELAY_TIME, INFINITY), "nonfinite parameter rejected");
    synth_reset_lfo_amounts(&s);
    near(synth_get_lfo_amount(&s, SYNTH_PARAM_DELAY_MIX), 0, 0, "route reset");
    synth_uninit(&s);
}

// advances audio time in small scratch buffers when a test only needs state to progress
static void render_frames(synth *s, size_t count)
{
    float scratch[64];
    while (count > 0) {
        const size_t n = count < 64 ? count : 64;
        synth_render_mono(s, scratch, n);
        count -= n;
    }
}

// checks note-on envelope capture, later route changes, and the existing manual override behavior
static void test_adsr_capture_and_manual_updates(void)
{
    synth s;
    const synth_adsr base = {0.1f, 0.2f, 0.5f, 0.4f};
    const synth_adsr manual = {0.3f, 0.4f, 0.6f, 0.2f};
    synth_init(&s, 8000);
    synth_set_adsr(&s, base);
    synth_set_lfo_depth(&s, 1);
    synth_set_lfo_amount(&s, SYNTH_PARAM_ATTACK, 0.1f);
    synth_set_lfo_amount(&s, SYNTH_PARAM_RELEASE, 0.2f);
    synth_lfo_reset(&s.lfo, 0.25f);
    // a quarter-cycle sine phase is its positive peak, making expected captures
    // base + amount * span: attack 0.1 + 0.1*1, release 0.4 + 0.2*1.5
    synth_note_on(&s, 60, 1);
    synth_note_on(&s, 64, 1);
    near(s.lfo.phase, 0.25f, 0, "note capture does not advance source");
    near(s.voices[0].envelope.adsr.attack_seconds, 0.2f, 1e-6f, "attack captured at note-on");
    near(s.voices[1].envelope.adsr.attack_seconds, 0.2f, 1e-6f, "same-time notes share phase");
    near(s.voices[0].envelope.adsr.release_seconds, 0.7f, 1e-6f, "release captured before note-off");
    synth_set_lfo_depth(&s, 0);
    render_frames(&s, 32);
    synth_note_off(&s, 60);
    near(s.voices[0].envelope.adsr.release_seconds, 0.7f, 1e-6f, "captured release survives route changes");
    {
        const synth_envelope_stage stage = s.voices[0].envelope.stage;
        const float level = s.voices[0].envelope.level;
        synth_set_adsr(&s, manual);
        near(s.voices[0].envelope.adsr.release_seconds, 0.2f, 0, "manual setter supersedes captured values");
        near(s.voices[1].envelope.adsr.attack_seconds, 0.3f, 0, "manual setter updates all active envelopes");
        near(s.voices[0].envelope.level, level, 0, "manual setter preserves level");
        check(s.voices[0].envelope.stage == stage, "manual setter preserves stage");
    }
    synth_set_lfo_depth(&s, 1);
    synth_lfo_reset(&s.lfo, 0.25f);
    synth_note_on(&s, 67, 1);
    near(s.voices[2].envelope.adsr.attack_seconds, 0.4f, 1e-6f, "future notes capture new base plus LFO");
    near(synth_get_adsr(&s).attack_seconds, manual.attack_seconds, 0, "capture preserves base envelope");
    synth_uninit(&s);
}

// creates a repeatable sound that exposes the chosen destination's effect on audio
static void setup_audible_patch(synth *s, const char *target_name)
{
    const synth_adsr adsr = {0.02f, 0.05f, 0.65f, 0.15f};
    synth_init(s, 8000);
    synth_set_adsr(s, adsr);
    synth_set_master_gain(s, 0.7f);
    synth_set_first_oscillator_gain(s, 0.65f);
    synth_set_second_oscillator_gain(s, 0.4f);
    synth_set_oscillator_morph(s, 0.6f);
    synth_set_second_oscillator_morph(s, 0.3f);
    synth_set_stereo_spread(s, 0.5f);
    synth_set_filter_cutoff(s, 1600);
    synth_set_filter_poles(s, 3);
    synth_set_saturation_drive(s, 8);
    synth_set_distortion_drive(s, 8);
    synth_set_bitcrusher_sample_rate(s, 1500);
    synth_set_bitcrusher_bits(s, 6);
    synth_set_flanger_intensity(s, 0.5f);
    synth_set_ring_mod_frequency(s, 100);
    synth_set_chorus_depth(s, 0.6f);
    synth_set_chorus_feedback(s, 0.15f);
    synth_set_delay_time(s, 0.04f);
    synth_set_delay_feedback(s, 0.5f);
    synth_set_plate_reverb_decay(s, 0.7f);
    synth_set_plate_reverb_predelay(s, 0.02f);
    synth_set_compressor_threshold(s, -30);
    synth_set_compressor_ratio(s, 4);
    synth_set_compressor_attack_seconds(s, 0.005f);
    synth_set_compressor_release_seconds(s, 0.03f);
    // enable only the effect under test so unrelated effects cannot mask its result
    if (strncmp(target_name, "saturation_", 11) == 0) synth_set_saturation_mix(s, 0.6f);
    if (strncmp(target_name, "distortion_", 11) == 0) synth_set_distortion_mix(s, 0.6f);
    if (strncmp(target_name, "bitcrusher_", 11) == 0) synth_set_bitcrusher_mix(s, 0.6f);
    if (strncmp(target_name, "flanger_", 8) == 0) synth_set_flanger_mix(s, 0.6f);
    if (strncmp(target_name, "ring_mod_", 9) == 0) synth_set_ring_mod_mix(s, 0.6f);
    if (strncmp(target_name, "chorus_", 7) == 0) synth_set_chorus_mix(s, 0.6f);
    if (strncmp(target_name, "delay_", 6) == 0) synth_set_delay_mix(s, 0.6f);
    if (strncmp(target_name, "plate_reverb_", 13) == 0) synth_set_plate_reverb_mix(s, 0.6f);
    if (strncmp(target_name, "compressor_", 11) != 0) synth_set_compressor_ratio(s, 1);
    synth_set_lfo_rate(s, 2);
    synth_lfo_reset(&s->lfo, 0.25f);
}

// compares two identical synths with one route enabled in only one copy; every target
// must change the output while all stored base parameters remain unchanged
static void test_every_destination_changes_audio_and_preserves_bases(void)
{
    for (size_t target = 0; target < sizeof(targets) / sizeof(targets[0]); ++target) {
        synth dry, wet;
        float bases[SYNTH_PARAM_COUNT];
        const synth_parameter_id id = targets[target];
        const synth_parameter_info *info = synth_parameter_info_at(id);
        double difference = 0;
        setup_audible_patch(&dry, info->name);
        setup_audible_patch(&wet, info->name);
        synth_set_lfo_depth(&wet, 1);
        synth_set_lfo_amount(&wet, id, 0.8f);
        for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) bases[i] = synth_get_parameter(&wet, (synth_parameter_id)i);
        synth_note_on(&dry, 57, 0.9f);
        synth_note_on(&wet, 57, 0.9f);
        for (size_t frame = 0; frame < 4800; frame += 32) {
            float dl[32], dr[32], wl[32], wr[32];
            synth_audio_buffer db = {dl, dr, 32};
            synth_audio_buffer wb = {wl, wr, 32};
            if (frame == 2400) {
                synth_note_off(&dry, 57);
                synth_note_off(&wet, 57);
            }
            synth_render_stereo(&dry, &db);
            synth_render_stereo(&wet, &wb);
            for (size_t j = 0; j < 32; ++j) {
                check(isfinite(wl[j]) && isfinite(wr[j]), "destination audio stays finite");
                difference += fabsf(dl[j] - wl[j]) + fabsf(dr[j] - wr[j]);
            }
        }
        if (difference <= 1e-6) {
            fprintf(stderr, "No audible effect for destination %s\n", info->name);
            ++failures;
        }
        for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) {
            near(synth_get_parameter(&wet, (synth_parameter_id)i), bases[i], 0, "render leaves every base getter unchanged");
        }
        synth_uninit(&dry);
        synth_uninit(&wet);
    }
}

// checks that splitting buffers cannot change timing or samples, and mono equals the stereo average
static void test_partition_and_mono_equivalence(void)
{
    synth whole, chunks, mono;
    float left[513], right[513], split_left[513], split_right[513], average[513];
    synth_audio_buffer buffer = {left, right, 513};
    setup_audible_patch(&whole, "chorus_mix");
    setup_audible_patch(&chunks, "chorus_mix");
    setup_audible_patch(&mono, "chorus_mix");
    for (int copy = 0; copy < 3; ++copy) {
        synth *s = copy == 0 ? &whole : copy == 1 ? &chunks : &mono;
        for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
            synth_set_lfo_amount(s, targets[i], i % 2 ? -0.2f : 0.2f);
        }
        synth_set_lfo_depth(s, 0.7f);
        synth_note_on(s, 60, 1);
    }
    synth_render_stereo(&whole, &buffer);
    for (size_t i = 0; i < 513; ) {
        const size_t count = i + 17 < 513 ? 17 : 513 - i;
        synth_audio_buffer part = {split_left + i, split_right + i, count};
        synth_render_stereo(&chunks, &part);
        i += count;
    }
    synth_render_mono(&mono, average, 513);
    check(memcmp(left, split_left, sizeof(left)) == 0, "left output independent of buffer partitions");
    check(memcmp(right, split_right, sizeof(right)) == 0, "right output independent of buffer partitions");
    for (size_t i = 0; i < 513; ++i) near(average[i], (left[i] + right[i]) * 0.5f, 0, "mono is stereo average");
    synth_uninit(&whole);
    synth_uninit(&chunks);
    synth_uninit(&mono);
}

// checks the two setter-reset traps: bitcrusher sample holding and continuously moving delay taps
static void test_stateful_overrides(void)
{
    synth_bitcrusher crusher;
    synth_bitcrusher_params cp;
    synth_delay delay;
    synth_delay_params dp;
    synth_stereo_sample input = {0.123f, 0.123f};
    synth_bitcrusher_init(&crusher, 1000);
    synth_bitcrusher_set_sample_rate(&crusher, 100);
    cp = synth_bitcrusher_get_params(&crusher);
    cp.mix = 1;
    {
        const synth_stereo_sample held = synth_bitcrusher_process_with_params(&crusher, input, &cp);
        input.left = input.right = 0.8f;
        cp.sample_rate = 110;
        cp.bits = 3;
        // at 110 hz within a 1000 hz host, five frames cannot reach the next tick
        // a rate/bits setter reset would expose the new input early and fail this check
        for (int i = 0; i < 5; ++i) {
            const synth_stereo_sample sample = synth_bitcrusher_process_with_params(&crusher, input, &cp);
            near(sample.left, held.left, 1e-7f, "rate/bits overrides preserve held sample until clock ticks");
        }
        near(crusher.sample_rate, 100, 0, "crusher base rate preserved");
        check(crusher.bits == 16, "crusher base bits preserved");
    }
    synth_delay_init(&delay, 1000);
    synth_delay_set_time(&delay, 0.02f);
    dp = synth_delay_get_params(&delay);
    dp.mix = 1;
    dp.feedback = 0.3f;
    {
        double wet_energy = 0;
        for (int i = 0; i < 600; ++i) {
            synth_stereo_sample sample;
            input.left = input.right = i % 71 == 0 ? 1.0f : 0.0f;
            // sweep 8 ms either side of 20 ms the 0.03 radians/frame rate moves
            // the tap continuously, unlike a manual time edit followed by settling
            dp.time_seconds = 0.02f + 0.008f * sinf((float)i * 0.03f);
            sample = synth_delay_process_with_params(&delay, input, &dp);
            wet_energy += fabsf(sample.left);
            check(!delay.has_pending_time_change && !delay.crossfading,
                  "continuous time override never enters manual settling/voice reset path");
        }
        check(wet_energy > 1, "moving delay retains and produces audible history");
        near(delay.time_seconds, 0.02f, 1e-6f, "moving tap leaves time getter unchanged");
        check(delay.main_voice_index == 0, "LFO does not create new delay voices");
    }
    synth_delay_uninit(&delay);
}

// compares combined routes to manually calculated controls, including an intermediate
// value above the limit that must not be clamped until direct modulation is added
static void test_flanger_composition(void)
{
    synth routed, reference, reversed;
    setup_audible_patch(&routed, "flanger_mix");
    setup_audible_patch(&reference, "flanger_mix");
    setup_audible_patch(&reversed, "flanger_mix");
    for (int i = 0; i < 3; ++i) {
        synth *s = i == 0 ? &routed : i == 1 ? &reference : &reversed;
        synth_set_flanger_intensity(s, 0.25f);
        synth_set_flanger_depth(s, 0.9f);
        synth_set_flanger_feedback(s, 0.1f);
        synth_set_lfo_rate(s, 0);
        synth_lfo_reset(&s->lfo, 0.25f);
    }
    synth_set_lfo_depth(&routed, 1);
    synth_set_lfo_depth(&reversed, 1);
    synth_set_lfo_amount(&routed, SYNTH_PARAM_FLANGER_INTENSITY, 1);
    synth_set_lfo_amount(&routed, SYNTH_PARAM_FLANGER_DEPTH, -1);
    synth_set_lfo_amount(&routed, SYNTH_PARAM_FLANGER_FEEDBACK, -0.2f);
    synth_set_lfo_amount(&reversed, SYNTH_PARAM_FLANGER_FEEDBACK, -0.2f);
    synth_set_lfo_amount(&reversed, SYNTH_PARAM_FLANGER_DEPTH, -1);
    synth_set_lfo_amount(&reversed, SYNTH_PARAM_FLANGER_INTENSITY, 1);
    // 0.9 + intensity delta 0.5 - direct depth 0.5 = 0.9
    // clamping the intermediate 1.4 would incorrectly produce 0.5
    synth_set_flanger_depth(&reference, 0.9f);
    synth_set_flanger_feedback(&reference, 0.1f +
        (sqrtf(0.75f) - sqrtf(0.25f)) *
        (SYNTH_FLANGER_MAX_FEEDBACK - SYNTH_FLANGER_MIN_INTENSITY_FEEDBACK) -
        0.2f * SYNTH_FLANGER_MAX_FEEDBACK);
    synth_note_on(&routed, 60, 1);
    synth_note_on(&reference, 60, 1);
    synth_note_on(&reversed, 60, 1);
    for (int block = 0; block < 50; ++block) {
        float actual[64], expected[64], opposite_order[64];
        synth_render_mono(&routed, actual, 64);
        synth_render_mono(&reference, expected, 64);
        synth_render_mono(&reversed, opposite_order, 64);
        for (int i = 0; i < 64; ++i) {
            near(actual[i], expected[i], 1e-6f, "flanger composes intensity before direct offsets and final clamp");
            near(actual[i], opposite_order[i], 0, "route assignment order does not change flanger audio");
        }
    }
    near(synth_get_flanger_depth(&routed), 0.9f, 0, "flanger preserves manual depth base");
    near(synth_get_flanger_feedback(&routed), 0.1f, 0, "flanger preserves manual feedback base");
    synth_uninit(&routed);
    synth_uninit(&reference);
    synth_uninit(&reversed);
}

// checks that manual pole changes produce identical audio through normal and effective-control apis
static void test_unmodulated_filter_history(void)
{
    synth_filter manual, effective;
    synth_filter_params params = {1200, 1};
    synth_filter_init(&manual, 48000, params.cutoff_hz);
    synth_filter_init(&effective, 48000, params.cutoff_hz);
    for (int i = 0; i < 1000; ++i) {
        const float input = sinf((float)i * 0.2f);
        if (i == 300 || i == 600) {
            params.pole_count = i == 300 ? 6 : 2;
            synth_filter_set_poles(&manual, params.pole_count);
            synth_filter_set_poles(&effective, params.pole_count);
        }
        near(synth_filter_process_with_params(&effective, input, &params),
             synth_filter_process(&manual, input), 0,
             "unmodulated filter preserves manual topology and hidden-stage history");
    }
}

// exercises every route together with all voices at common sample rates, looking for invalid audio
static void test_full_polyphony_at_sample_rates(void)
{
    const float rates[] = {44100, 48000, 96000};
    for (size_t rate = 0; rate < sizeof(rates) / sizeof(rates[0]); ++rate) {
        synth s;
        synth_init(&s, rates[rate]);
        synth_set_second_oscillator_gain(&s, 0.4f);
        synth_set_oscillator_morph(&s, 0.5f);
        synth_set_filter_cutoff(&s, 3000);
        synth_set_lfo_rate(&s, 20);
        synth_set_lfo_depth(&s, 1);
        // midpoint mixes sweep through dry and wet; predelay crosses zero
        for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
            const synth_parameter_info *info = synth_parameter_info_at(targets[i]);
            if (strstr(info->name, "_mix") != NULL) synth_set_parameter(&s, info->id, 0.5f);
            synth_set_lfo_amount(&s, info->id, i % 2 ? -1 : 1);
        }
        for (int i = 0; i < SYNTH_MAX_VOICES; ++i) synth_note_on(&s, 48 + i, 0.8f);
        for (size_t frame = 0; frame < (size_t)rates[rate] / 4; frame += 64) {
            float left[64], right[64];
            synth_audio_buffer buffer = {left, right, 64};
            if (frame == 4096) synth_set_pitch_bend(&s, 0.7f);
            synth_render_stereo(&s, &buffer);
            for (int i = 0; i < 64; ++i) {
                check(isfinite(left[i]) && isfinite(right[i]), "all routes with full polyphony stay finite");
            }
        }
        synth_uninit(&s);
    }
}

// runs the modulation contract and state-continuity checks; a nonzero exit reports failures
int main(void)
{
    test_catalog_and_evaluation();
    test_adsr_capture_and_manual_updates();
    test_every_destination_changes_audio_and_preserves_bases();
    test_partition_and_mono_equivalence();
    test_stateful_overrides();
    test_flanger_composition();
    test_unmodulated_filter_history();
    test_full_polyphony_at_sample_rates();
    if (failures) fprintf(stderr, "%d modulation checks failed\n", failures);
    return failures ? 1 : 0;
}
