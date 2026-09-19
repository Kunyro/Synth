#include "synth/synth.h"
#include "../src/internal/render_parameters.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int condition, const char *message)
{
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
static void near(float actual, float expected, float tolerance, const char *message)
{
    check(isfinite(actual) && fabsf(actual - expected) <= tolerance, message);
}
static void render(synth *s, size_t frames)
{
    float scratch[64];
    while (frames) {
        const size_t n = frames < 64 ? frames : 64;
        synth_render_mono(s, scratch, n);
        frames -= n;
    }
}
static void init(synth *s)
{
    synth_init(s, 8000);
    check(synth_is_ready(s), "all voice resources initialized");
    synth_set_adsr(s, (synth_adsr){0, 0, 1, 0.5f});
}

// explicit exceptions protect eligibility independently of the routing implementation
static void test_routes(void)
{
    synth s;
    init(&s);
    int count = 0;
    near(synth_get_mod_envelope_depth(&s), 0, 0, "depth starts disabled");
    for (int i = 0; i < SYNTH_PARAM_COUNT; ++i) {
        const synth_parameter_id id = (synth_parameter_id)i;
        const synth_parameter_info *info = synth_parameter_info_at(id);
        const int expected = i > SYNTH_PARAM_RELEASE &&
            i != SYNTH_PARAM_LFO_RATE && i != SYNTH_PARAM_LFO_SHAPE_MORPH &&
            i != SYNTH_PARAM_LFO_DEPTH && i < SYNTH_PARAM_MOD_ENVELOPE_ATTACK;
        check(synth_modulation_supports(SYNTH_MOD_SOURCE_ENVELOPE, id) == expected, "exact envelope inventory");
        near(synth_get_envelope_amount(&s, id), 0, 0, "every amount initially zero");
        check(synth_set_envelope_amount(&s, id, 1) == expected, "excluded controls rejected");
        if (!expected) continue;
        ++count;
        float low = info->min_value, high = info->max_value;
        if (id == SYNTH_PARAM_FILTER_CUTOFF) high = 4000;
        if (id == SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE) high = 8000;
        const float base = synth_parameter_clamp(id, low + (high - low) * 0.3f, 8000);
        near(synth_modulate_sources(&s, id, base, 0, 1), base, 0, "zero depth is exact identity");
        synth_set_mod_envelope_depth(&s, 1);
        near(synth_modulate_sources(&s, id, base, 0, 0), base, 0, "zero source is exact identity");
        near(synth_modulate_sources(&s, id, base, 0, 1), high, 0, "positive peak reaches exact maximum");
        synth_set_envelope_amount(&s, id, -1);
        near(synth_modulate_sources(&s, id, base, 0, 1), low, 0, "negative peak reaches exact minimum");
        for (int sign = -1; sign <= 1; sign += 2) {
            const float endpoint = sign < 0 ? low : high;
            synth_set_envelope_amount(&s, id, (float)sign);
            float midpoint = info->domain == SYNTH_DOMAIN_LOG2 ? sqrtf(base * endpoint) : (base + endpoint) * 0.5f;
            if (info->type == SYNTH_PARAMETER_INTEGER) midpoint = roundf(midpoint);
            near(synth_modulate_sources(&s, id, base, 0, 0.5f), midpoint,
                 fmaxf(1e-5f, fabsf(midpoint) * 2e-6f), "fractional excursion follows parameter domain");
        }
        synth_set_mod_envelope_depth(&s, 0);
    }
    check(count == 48, "48 envelope destinations");
    synth_set_mod_envelope_depth(&s, 1);
    synth_reset_envelope_amounts(&s);
    synth_set_envelope_amount(&s, SYNTH_PARAM_DELAY_MIX, 1);
    synth_set_lfo_amount(&s, SYNTH_PARAM_DELAY_MIX, -1);
    synth_set_lfo_depth(&s, 1);
    near(synth_modulate_sources(&s, SYNTH_PARAM_DELAY_MIX, 0.3f, 1, 1), 0.5f, 1e-6f, "sources add before clamping");
    synth_set_envelope_amount(&s, SYNTH_PARAM_FILTER_CUTOFF, 1);
    synth_set_lfo_amount(&s, SYNTH_PARAM_FILTER_CUTOFF, -0.2f);
    near(synth_modulate_sources(&s, SYNTH_PARAM_FILTER_CUTOFF, 1000, 1, 1), 2000, 0.001f, "logarithmic contributions add in octaves");
    synth_reset_lfo_amounts(&s);
    near(synth_get_envelope_amount(&s, SYNTH_PARAM_FILTER_CUTOFF), 1, 0, "lfo reset leaves envelope routes intact");
    synth_set_lfo_amount(&s, SYNTH_PARAM_FILTER_CUTOFF, 0.2f);
    synth_reset_envelope_amounts(&s);
    near(synth_get_lfo_amount(&s, SYNTH_PARAM_FILTER_CUTOFF), 0.2f, 0, "envelope reset leaves lfo routes intact");
    check(!synth_set_envelope_amount(&s, SYNTH_PARAM_DELAY_MIX, NAN), "nan route rejected");
    check(!synth_set_envelope_amount(NULL, SYNTH_PARAM_DELAY_MIX, 1), "null route rejected");
    check(!synth_set_envelope_amount(&s, SYNTH_PARAM_COUNT, 1), "invalid target rejected");
    synth_set_mod_envelope_depth(&s, NAN);
    near(synth_get_mod_envelope_depth(&s), 1, 0, "nonfinite depth rejected");
    synth_uninit(&s);
}

static void test_release_and_edits(void)
{
    synth_envelope env;
    synth_adsr adsr = {0, 0, 0.25f, 0.1f};
    synth_envelope_init(&env, adsr);
    env.full_duration_release = 1;
    synth_envelope_note_on(&env);
    synth_envelope_advance(&env, 1000);
    synth_envelope_advance(&env, 1000);
    synth_envelope_note_off(&env);
    for (int i = 0; i < 50; ++i) synth_envelope_advance(&env, 1000);
    near(env.level, 0.125f, 1e-6f, "half duration is half the release starting level");
    synth_envelope_note_off(&env);
    adsr.attack_seconds = 1;
    synth_envelope_set_adsr(&env, adsr);
    for (int i = 0; i < 50; ++i) synth_envelope_advance(&env, 1000);
    check(env.stage == SYNTH_ENV_OFF, "repeated note-off and unrelated edits do not restart release");
    adsr.attack_seconds = 0;
    synth_envelope_set_adsr(&env, adsr);
    synth_envelope_note_on(&env);
    synth_envelope_advance(&env, 1000);
    synth_envelope_advance(&env, 1000);
    synth_envelope_note_off(&env);
    for (int i = 0; i < 50; ++i) synth_envelope_advance(&env, 1000);
    adsr.release_seconds = 0.2f;
    synth_envelope_set_adsr(&env, adsr);
    near(env.level, 0.125f, 1e-6f, "release edit preserves current level");
    for (int i = 0; i < 100; ++i) synth_envelope_advance(&env, 1000);
    near(env.level, 0.0625f, 1e-6f, "new release duration starts from preserved level");
    for (int i = 0; i < 100; ++i) synth_envelope_advance(&env, 1000);
    check(env.stage == SYNTH_ENV_OFF, "new release duration completes");
}

static void test_independent_voices(void)
{
    synth s;
    init(&s);
    synth_set_mod_envelope_adsr(&s, (synth_adsr){0.1f, 0.1f, 0.5f, 1});
    synth_set_mod_envelope_depth(&s, 1);
    synth_set_envelope_amount(&s, SYNTH_PARAM_FILTER_CUTOFF, 1);
    synth_set_filter_cutoff(&s, 100);
    synth_note_on(&s, 60, 1);
    render(&s, 400);
    near(s.voices[0].mod_envelope.level, 0.5f, 1e-5f, "first voice halfway through attack");
    const float phase = s.lfo.phase;
    synth_note_on(&s, 64, 0.1f);
    near(s.lfo.phase, phase, 0, "new note does not restart global lfo");
    near(s.voices[1].mod_envelope.level, 0, 0, "new voice starts its own envelope");
    render(&s, 100);
    near(s.voices[1].mod_envelope.level, 0.125f, 1e-5f, "source is independent of note velocity");
    check(s.voices[0].filter.state[0] != s.voices[1].filter.state[0], "filter histories are per voice");
    check(s.voices[0].effects.delay.voices[0].line.left != s.voices[1].effects.delay.voices[0].line.left,
          "effect buffers have independent ownership");
    const float level = s.voices[0].mod_envelope.level;
    const synth_envelope_stage stage = s.voices[0].mod_envelope.stage;
    synth_set_parameter(&s, SYNTH_PARAM_MOD_ENVELOPE_SUSTAIN, 0.3f);
    near(s.voices[0].mod_envelope.level, level, 0, "manual source edit preserves level");
    check(s.voices[0].mod_envelope.stage == stage, "manual source edit preserves stage");
    near(s.voices[1].mod_envelope.adsr.sustain_level, 0.3f, 0, "manual source edit updates every voice");
    synth_set_adsr(&s, (synth_adsr){0, 0, 1, 0});
    synth_note_off(&s, 60);
    render(&s, 1);
    check(s.voices[0].mod_envelope.stage == SYNTH_ENV_OFF, "modulation retires with volume envelope");
    check(s.voices[1].active, "other held note remains active");
    near(synth_get_filter_cutoff(&s), 100, 0, "render does not modify base cutoff");
    synth_uninit(&s);
}

// two separate instruments form an independent reference for per-voice nonlinear processing
static void test_voice_sum_and_partition(void)
{
    synth combined, first, second, chunks, mono;
    synth *all[] = {&combined, &first, &second, &chunks, &mono};
    for (int i = 0; i < 5; ++i) {
        init(all[i]);
        synth_set_distortion_drive(all[i], 12);
        synth_set_distortion_mix(all[i], 0.7f);
        synth_set_compressor_threshold(all[i], -20);
        synth_set_compressor_ratio(all[i], 4);
        synth_set_mod_envelope_depth(all[i], 1);
        synth_set_mod_envelope_adsr(all[i], (synth_adsr){0.02f, 0.05f, 0.4f, 0.1f});
        synth_set_envelope_amount(all[i], SYNTH_PARAM_DISTORTION_DRIVE, 0.4f);
        synth_set_envelope_amount(all[i], SYNTH_PARAM_FILTER_CUTOFF, -0.5f);
        synth_set_lfo_depth(all[i], 0.5f);
        synth_set_lfo_amount(all[i], SYNTH_PARAM_FILTER_CUTOFF, 0.2f);
    }
    for (int i = 0; i < 5; ++i) {
        if (i != 2) synth_note_on(all[i], 60, 0.8f);
        if (i != 1) synth_note_on(all[i], 67, 0.7f);
    }
    float left[257], right[257], a[257], b[257], cl[257], cr[257], m[257];
    synth_audio_buffer buffer = {left, right, 257};
    synth_render_stereo(&combined, &buffer);
    synth_render_mono(&first, a, 257);
    synth_render_mono(&second, b, 257);
    synth_render_mono(&mono, m, 257);
    for (size_t start = 0; start < 257;) {
        const size_t n = start + 17 < 257 ? 17 : 257 - start;
        synth_audio_buffer part = {cl + start, cr + start, n};
        synth_render_stereo(&chunks, &part);
        start += n;
    }
    check(memcmp(left, cl, sizeof(left)) == 0 && memcmp(right, cr, sizeof(right)) == 0,
          "two-source rendering is buffer-partition invariant");
    for (int i = 0; i < 257; ++i) {
        near((left[i] + right[i]) * 0.5f, a[i] + b[i], 1e-6f, "nonlinear processing happens before summing voices");
        near(m[i], (left[i] + right[i]) * 0.5f, 0, "mono shares the stereo source clock");
    }
    for (int i = 0; i < 5; ++i) synth_uninit(all[i]);
}

static void test_tails_and_stealing(void)
{
    synth s;
    init(&s);
    synth_set_adsr(&s, (synth_adsr){0, 0, 1, 0});
    synth_set_delay_time(&s, 0.2f);
    synth_set_delay_mix(&s, 1);
    synth_set_delay_feedback(&s, 0);
    synth_note_on(&s, 60, 1);
    render(&s, 80);
    synth_note_off(&s, 60);
    render(&s, 1000);
    check(!s.voices[0].active && s.voices[0].tail_active, "silent gap before echo does not retire tail");
    float echo[1000];
    synth_render_mono(&s, echo, 1000);
    float energy = 0;
    for (int i = 0; i < 1000; ++i) energy += fabsf(echo[i]);
    check(energy > 0.01f, "delay echo continues after both envelopes retire");
    render(&s, 24000);
    check(!s.voices[0].tail_active, "drained histories eventually retire");
    synth_set_delay_mix(&s, 0);
    for (int i = 0; i < SYNTH_MAX_VOICES; ++i) synth_note_on(&s, 48 + i, 1);
    render(&s, 32);
    synth_note_on(&s, 90, 1);
    synth_voice *stolen = NULL;
    for (int i = 0; i < SYNTH_MAX_VOICES; ++i) if (s.voices[i].pending.active) stolen = &s.voices[i];
    check(stolen != NULL, "full polyphony creates a bounded pending replacement");
    if (stolen) {
        float *storage = stolen->effects.delay.voices[0].line.left;
        const size_t fade = stolen->steal_remaining;
        render(&s, fade);
        check(stolen->note_number == 90 && stolen->active, "replacement starts after fade");
        check(storage == stolen->effects.delay.voices[0].line.left, "voice steal reuses allocated storage");
        check(!synth_effect_chain_has_tail(&stolen->effects), "replacement history is clean before its first frame");
        near(stolen->mod_envelope.level, 0, 0, "replacement source starts at zero");
    }
    synth_note_on(&s, 91, 1);
    synth_note_off(&s, 91);
    render(&s, 64);
    for (int i = 0; i < SYNTH_MAX_VOICES; ++i)
        check(!(s.voices[i].gate && s.voices[i].note_number == 91), "note-off cancels an unstarted replacement");
    synth_note_on(&s, 92, 1);
    synth_all_notes_off(&s);
    render(&s, 64);
    for (int i = 0; i < SYNTH_MAX_VOICES; ++i)
        check(!s.voices[i].gate && !s.voices[i].pending.active, "all-notes-off cancels pending gates");
    synth_uninit(&s);
    synth_uninit(&s);
}

static void test_fade_and_disabled_identity(void)
{
    synth faded, reference;
    init(&faded);
    init(&reference);
    synth_set_adsr(&faded, (synth_adsr){0, 0, 1, 0});
    synth_set_adsr(&reference, (synth_adsr){0, 0, 1, 0});
    // zero depth and zero routes are independent ways to disable contribution
    synth_set_mod_envelope_depth(&reference, 1);
    for (int id = 0; id < SYNTH_PARAM_COUNT; ++id)
        synth_set_envelope_amount(&faded, (synth_parameter_id)id, 1);
    for (int i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_note_on(&faded, 48 + i, i == 0 ? 1 : 0);
        synth_note_on(&reference, 48 + i, i == 0 ? 1 : 0);
    }
    float a[64], b[64];
    synth_render_mono(&faded, a, 64);
    synth_render_mono(&reference, b, 64);
    check(memcmp(a, b, sizeof(a)) == 0, "disabled envelope routes produce identical audio");
    synth_note_on(&faded, 90, 1);
    const size_t frames = faded.voices[0].steal_remaining;
    check(frames > 1 && frames <= 64, "steal has a short bounded output fade");
    if (frames > 1 && frames <= 64) {
        synth_render_mono(&faded, a, frames);
        synth_render_mono(&reference, b, frames);
        float energy = 0;
        for (size_t i = 0; i < frames; ++i) {
            const float gain = (float)(frames - i - 1) / (float)frames;
            near(a[i], b[i] * gain, 1e-6f, "steal fades the old output even with zero volume release");
            energy += fabsf(a[i]);
        }
        check(energy > 0.01f, "steal does not cut the old voice off immediately");
        near(a[frames - 1], 0, 0, "last retired sample is silent before history reset");
    }
    synth_uninit(&faded);
    synth_uninit(&reference);
}

int main(void)
{
    test_routes();
    test_release_and_edits();
    test_independent_voices();
    test_voice_sum_and_partition();
    test_tails_and_stealing();
    test_fade_and_disabled_identity();
    if (failures) { fprintf(stderr, "%d modulation envelope failures\n", failures); return 1; }
    puts("All modulation envelope tests passed.");
    return 0;
}
