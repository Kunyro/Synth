#include "synth/synth.h"

#include <math.h>
#include <string.h>

#include "internal/synth_internal.h"
#include "internal/render_parameters.h"

// converts a semitone offset into a frequency multiplier
static float pitch_bend_ratio(float semitones)
{
    return powf(2.0f, semitones / 12.0f);
}

// applies the current synth bend to a base frequency
static float bend_frequency(const synth *s, float base_frequency)
{
    return base_frequency * pitch_bend_ratio(s->pitch_bend_semitones);
}

// totals the second oscillator pitch offset in semitones
static float second_oscillator_semitones(const synth *s)
{
    return (float)(s->second_oscillator_octave * 12) +
           (float)s->second_oscillator_pitch_semitones +
           (s->second_oscillator_fine_tune_cents / 100.0f);
}

// applies the current second oscillator tuning to a primary frequency
static float second_oscillator_frequency(const synth *s, float primary_frequency)
{
    return primary_frequency * pitch_bend_ratio(second_oscillator_semitones(s));
}

// free slots precede tails, then quiet musical voices; pending replacements stay bounded
static synth_voice *find_available_voice(synth *s)
{
    synth_voice *best = NULL;
    float best_level = 0;
    int best_rank = 4;
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *v = &s->voices[i];
        const int rank = v->pending.active ? 3 : (!v->active && !v->tail_active ? 0 : (!v->active ? 1 : 2));
        const float level = v->active ? v->envelope.level : v->output_level;
        if (best == NULL || rank < best_rank || (rank == best_rank && level < best_level)) {
            best = v;
            best_rank = rank;
            best_level = level;
        }
    }
    return best;
}

// retunes one voice without changing its original note or phase
static void retune_voice(synth *s, synth_voice *voice)
{
    const float primary_frequency = bend_frequency(s, voice->base_frequency);
    const float secondary_frequency = second_oscillator_frequency(s, primary_frequency);

    synth_voice_set_frequencies(voice, primary_frequency, secondary_frequency);
}

// retunes active voices without changing their original note or phase
static void retune_active_voices(synth *s)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *voice = &s->voices[i];

        if (voice->active) {
            retune_voice(s, voice);
        }
    }
}

// starts a captured replacement only after the old output has faded to zero
void synth_start_pending_voice(synth *s, synth_voice *voice)
{
    const synth_pending_note note = voice->pending;
    voice->pending.active = 0;
    voice->active = 0;
    voice->gate = 0;
    voice->envelope.stage = SYNTH_ENV_OFF;
    voice->envelope.level = 0;
    voice->mod_envelope.stage = SYNTH_ENV_OFF;
    voice->mod_envelope.level = 0;
    voice->steal_remaining = 0;
    if (!note.active) {
        synth_voice_reset_processing(voice);
        return;
    }
    synth_envelope_set_adsr(&voice->mod_envelope, s->mod_envelope);
    synth_voice_note_on(voice, note.note_number, note.frequency, note.velocity,
                        s->waveform, note.adsr);
    retune_voice(s, voice);
    synth_voice_set_oscillator_morph(voice, s->oscillator_morph);
    synth_voice_set_second_oscillator_morph(voice, s->second_oscillator_morph);
}

static void request_note(synth *s, int note_number, float frequency, float velocity)
{
    if (!s->ready || !isfinite(frequency) || frequency <= 0 || !isfinite(velocity)) return;
    synth_voice *voice = find_available_voice(s);
    voice->pending = (synth_pending_note){1, note_number, frequency, velocity, synth_capture_modulated_adsr(s)};
    if (!voice->active && !voice->tail_active) {
        synth_start_pending_voice(s, voice);
    } else if (voice->steal_remaining == 0) {
        // a two millisecond fade bounds the onset delay without allocating a tail pool
        voice->steal_frames = (size_t)fmaxf(1.0f, s->sample_rate * SYNTH_VOICE_STEAL_SECONDS);
        voice->steal_remaining = voice->steal_frames;
        // the output ramp owns this retirement; a zero release must not cut it short
        voice->gate = 0;
    }
}

// sets up the synth with defaults
void synth_init(synth *s, float sample_rate)
{
    const synth_adsr default_envelope = {0.01f, 0.08f, 0.75f, 0.16f};

    memset(s, 0, sizeof(*s));
    s->sample_rate = isfinite(sample_rate) && sample_rate > 0 ? sample_rate : SYNTH_DEFAULT_SAMPLE_RATE;
    sample_rate = s->sample_rate;
    s->master_gain = SYNTH_DEFAULT_MASTER_GAIN;
    s->pitch_bend = 0.0f;
    s->pitch_bend_semitones = 0.0f;
    s->waveform = SYNTH_WAVEFORM_SINE;
    s->oscillator_morph = synth_waveform_to_morph(s->waveform);
    s->first_oscillator_gain = 1.0f;
    s->second_oscillator_gain = 0.0f;
    s->stereo_spread = 0.0f;
    s->second_oscillator_morph = synth_waveform_to_morph(SYNTH_WAVEFORM_SQUARE);
    s->second_oscillator_octave = 0;
    s->second_oscillator_pitch_semitones = 0;
    s->second_oscillator_fine_tune_cents = 0.0f;
    synth_lfo_init(&s->lfo, SYNTH_DEFAULT_LFO_RATE_HZ);
    s->lfo_depth = 0.0f;
    s->envelope = synth_sanitize_adsr(default_envelope);
    s->mod_envelope = default_envelope;
    s->filter = (synth_filter_params){sample_rate * 0.5f, SYNTH_FILTER_DEFAULT_POLES};
    s->ready = 1;
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_init(&s->voices[i], s->envelope);
        if (!synth_voice_prepare(&s->voices[i], sample_rate)) s->ready = 0;
    }
    s->effects = synth_effect_chain_get_params(&s->voices[0].effects);
    if (!s->ready) {
        for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) synth_voice_uninit(&s->voices[i]);
    }
}

void synth_uninit(synth *s)
{
    if (s == 0) {
        return;
    }

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) synth_voice_uninit(&s->voices[i]);
    s->ready = 0;
}

int synth_is_ready(const synth *s)
{
    return s != NULL && s->ready;
}

void synth_note_on(synth *s, int note_number, float velocity)
{
    request_note(s, note_number, synth_note_to_frequency(note_number), velocity);
}

void synth_note_on_frequency(synth *s, float frequency, float velocity)
{
    request_note(s, -1, frequency, velocity);
}

void synth_note_off(synth *s, int note_number)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice *v = &s->voices[i];
        if (v->gate && v->note_number == note_number) {
            synth_voice_note_off(v);
            return;
        }
        if (v->pending.active && v->pending.note_number == note_number) {
            v->pending.active = 0;
            return;
        }
    }
}

void synth_all_notes_off(synth *s)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        s->voices[i].pending.active = 0;
        synth_voice_note_off(&s->voices[i]);
    }
}

void synth_set_mod_envelope_adsr(synth *s, synth_adsr adsr)
{
    if (s == NULL || !isfinite(adsr.attack_seconds) || !isfinite(adsr.decay_seconds) ||
        !isfinite(adsr.sustain_level) || !isfinite(adsr.release_seconds)) return;
    s->mod_envelope = synth_sanitize_adsr(adsr);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_envelope_set_adsr(&s->voices[i].mod_envelope, s->mod_envelope);
}

synth_adsr synth_get_mod_envelope_adsr(const synth *s)
{
    return s != NULL ? s->mod_envelope : (synth_adsr){0, 0, 0, 0};
}

void synth_set_mod_envelope_depth(synth *s, float depth)
{
    if (s != NULL && isfinite(depth)) s->mod_envelope_depth = synth_clampf(depth, 0, 1);
}

float synth_get_mod_envelope_depth(const synth *s)
{
    return s != NULL ? s->mod_envelope_depth : 0;
}

void synth_set_pitch_bend(synth *s, float pitch_bend)
{
    s->pitch_bend = synth_clampf(pitch_bend, -1.0f, 1.0f);
    s->pitch_bend_semitones = s->pitch_bend * SYNTH_PITCH_BEND_MAX_SEMITONES;
    retune_active_voices(s);
}

void synth_set_master_gain(synth *s, float gain)
{
    s->master_gain = synth_clampf(gain, 0.0f, 1.0f);
}

void synth_set_adsr(synth *s, synth_adsr envelope)
{
    s->envelope = synth_sanitize_adsr(envelope);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_envelope_set_adsr(&s->voices[i].envelope, s->envelope);
        if (s->voices[i].pending.active) s->voices[i].pending.adsr = s->envelope;
    }
}

synth_adsr synth_get_adsr(const synth *s)
{
    return s->envelope;
}

void synth_set_waveform(synth *s, synth_waveform waveform)
{
    s->waveform = waveform;
    s->oscillator_morph = synth_waveform_to_morph(waveform);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_waveform(&s->voices[i], waveform);
    }
}

void synth_set_oscillator_morph(synth *s, float morph)
{
    s->oscillator_morph = synth_clampf(morph, 0.0f, 1.0f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_oscillator_morph(&s->voices[i], s->oscillator_morph);
    }
}

void synth_set_second_oscillator_morph(synth *s, float morph)
{
    s->second_oscillator_morph = synth_clampf(morph, 0.0f, 1.0f);

    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_set_second_oscillator_morph(&s->voices[i], s->second_oscillator_morph);
    }
}

void synth_set_first_oscillator_gain(synth *s, float gain)
{
    s->first_oscillator_gain = synth_clampf(gain, 0.0f, 1.0f);
}

void synth_set_second_oscillator_gain(synth *s, float gain)
{
    s->second_oscillator_gain = synth_clampf(gain, 0.0f, 1.0f);
}

void synth_set_stereo_spread(synth *s, float spread)
{
    s->stereo_spread = synth_clampf(spread, 0.0f, 1.0f);
}

void synth_set_second_oscillator_octave(synth *s, int octave)
{
    s->second_oscillator_octave = synth_clampi(octave, -1, 1);
    retune_active_voices(s);
}

void synth_set_second_oscillator_pitch(synth *s, int semitones)
{
    s->second_oscillator_pitch_semitones = synth_clampi(semitones, -6, 6);
    retune_active_voices(s);
}

void synth_set_second_oscillator_fine_tune(synth *s, float cents)
{
    s->second_oscillator_fine_tune_cents = synth_clampf(cents, -50.0f, 50.0f);
    retune_active_voices(s);
}

void synth_set_filter_cutoff(synth *s, float cutoff_hz)
{
    if (!isfinite(cutoff_hz)) return;
    s->filter.cutoff_hz = synth_parameter_clamp(SYNTH_PARAM_FILTER_CUTOFF, cutoff_hz, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_filter_set_cutoff(&s->voices[i].filter, s->filter.cutoff_hz);
        synth_filter_set_cutoff(&s->voices[i].right_filter, s->filter.cutoff_hz);
    }
}

void synth_set_filter_poles(synth *s, int pole_count)
{
    s->filter.pole_count = synth_clampi(pole_count, 1, SYNTH_FILTER_MAX_POLES);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_filter_set_poles(&s->voices[i].filter, s->filter.pole_count);
        synth_filter_set_poles(&s->voices[i].right_filter, s->filter.pole_count);
    }
}

void synth_set_lfo_rate(synth *s, float frequency_hz)
{
    synth_lfo_set_frequency(&s->lfo, frequency_hz);
}

void synth_set_lfo_shape_morph(synth *s, float morph)
{
    synth_lfo_set_morph(&s->lfo, morph);
}

void synth_set_lfo_depth(synth *s, float depth)
{
    s->lfo_depth = synth_clampf(depth, 0.0f, 1.0f);
}

void synth_set_saturation_drive(synth *s, float drive)
{
    if (!isfinite((float)drive)) return;
    s->effects.saturation.drive = (float)synth_parameter_clamp(SYNTH_PARAM_SATURATION_DRIVE, (float)drive, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_saturation_set_drive(&s->voices[i].effects.saturation, s->effects.saturation.drive);
}

void synth_set_saturation_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.saturation.mix = (float)synth_parameter_clamp(SYNTH_PARAM_SATURATION_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_saturation_set_mix(&s->voices[i].effects.saturation, s->effects.saturation.mix);
}

void synth_set_distortion_drive(synth *s, float drive)
{
    if (!isfinite((float)drive)) return;
    s->effects.distortion.drive = (float)synth_parameter_clamp(SYNTH_PARAM_DISTORTION_DRIVE, (float)drive, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_distortion_set_drive(&s->voices[i].effects.distortion, s->effects.distortion.drive);
}

void synth_set_distortion_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.distortion.mix = (float)synth_parameter_clamp(SYNTH_PARAM_DISTORTION_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_distortion_set_mix(&s->voices[i].effects.distortion, s->effects.distortion.mix);
}

void synth_set_bitcrusher_sample_rate(synth *s, float sample_rate)
{
    if (!isfinite((float)sample_rate)) return;
    s->effects.bitcrusher.sample_rate = (float)synth_parameter_clamp(SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE, (float)sample_rate, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_bitcrusher_set_sample_rate(&s->voices[i].effects.bitcrusher, s->effects.bitcrusher.sample_rate);
}

void synth_set_bitcrusher_bits(synth *s, int bits)
{
    if (!isfinite((float)bits)) return;
    s->effects.bitcrusher.bits = (int)synth_parameter_clamp(SYNTH_PARAM_BITCRUSHER_BITS, (float)bits, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_bitcrusher_set_bits(&s->voices[i].effects.bitcrusher, s->effects.bitcrusher.bits);
}

void synth_set_bitcrusher_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.bitcrusher.mix = (float)synth_parameter_clamp(SYNTH_PARAM_BITCRUSHER_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_bitcrusher_set_mix(&s->voices[i].effects.bitcrusher, s->effects.bitcrusher.mix);
}

void synth_set_flanger_rate(synth *s, float hz)
{
    if (!isfinite((float)hz)) return;
    s->effects.flanger.rate_hz = (float)synth_parameter_clamp(SYNTH_PARAM_FLANGER_RATE, (float)hz, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_flanger_set_rate(&s->voices[i].effects.flanger, s->effects.flanger.rate_hz);
}

void synth_set_flanger_intensity(synth *s, float intensity)
{
    if (!isfinite((float)intensity)) return;
    s->effects.flanger.intensity = (float)synth_parameter_clamp(SYNTH_PARAM_FLANGER_INTENSITY, (float)intensity, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_flanger_set_intensity(&s->voices[i].effects.flanger, s->effects.flanger.intensity);
    // the module owns the intensity curve; retain its resolved manual components
    s->effects.flanger.depth = s->voices[0].effects.flanger.depth;
    s->effects.flanger.feedback = s->voices[0].effects.flanger.feedback;
}

void synth_set_flanger_depth(synth *s, float depth)
{
    if (!isfinite((float)depth)) return;
    s->effects.flanger.depth = (float)synth_parameter_clamp(SYNTH_PARAM_FLANGER_DEPTH, (float)depth, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_flanger_set_depth(&s->voices[i].effects.flanger, s->effects.flanger.depth);
}

void synth_set_flanger_feedback(synth *s, float feedback)
{
    if (!isfinite((float)feedback)) return;
    s->effects.flanger.feedback = (float)synth_parameter_clamp(SYNTH_PARAM_FLANGER_FEEDBACK, (float)feedback, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_flanger_set_feedback(&s->voices[i].effects.flanger, s->effects.flanger.feedback);
}

void synth_set_flanger_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.flanger.mix = (float)synth_parameter_clamp(SYNTH_PARAM_FLANGER_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_flanger_set_mix(&s->voices[i].effects.flanger, s->effects.flanger.mix);
}

void synth_set_flanger_manual(synth *s, float seconds)
{
    if (!isfinite((float)seconds)) return;
    s->effects.flanger.manual_delay_seconds = (float)synth_parameter_clamp(SYNTH_PARAM_FLANGER_MANUAL, (float)seconds, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_flanger_set_manual(&s->voices[i].effects.flanger, s->effects.flanger.manual_delay_seconds);
}

void synth_set_ring_mod_frequency(synth *s, float hz)
{
    if (!isfinite((float)hz)) return;
    s->effects.ring_mod.frequency_hz = (float)synth_parameter_clamp(SYNTH_PARAM_RING_MOD_FREQUENCY, (float)hz, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_ring_mod_set_frequency(&s->voices[i].effects.ring_mod, s->effects.ring_mod.frequency_hz);
}

void synth_set_ring_mod_rectify(synth *s, float rectify)
{
    if (!isfinite((float)rectify)) return;
    s->effects.ring_mod.rectify = (float)synth_parameter_clamp(SYNTH_PARAM_RING_MOD_RECTIFY, (float)rectify, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_ring_mod_set_rectify(&s->voices[i].effects.ring_mod, s->effects.ring_mod.rectify);
}

void synth_set_ring_mod_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.ring_mod.mix = (float)synth_parameter_clamp(SYNTH_PARAM_RING_MOD_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_ring_mod_set_mix(&s->voices[i].effects.ring_mod, s->effects.ring_mod.mix);
}

void synth_set_chorus_rate(synth *s, float hz)
{
    if (!isfinite((float)hz)) return;
    s->effects.chorus.rate_hz = (float)synth_parameter_clamp(SYNTH_PARAM_CHORUS_RATE, (float)hz, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_chorus_set_rate(&s->voices[i].effects.chorus, s->effects.chorus.rate_hz);
}

void synth_set_chorus_depth(synth *s, float depth)
{
    if (!isfinite((float)depth)) return;
    s->effects.chorus.depth = (float)synth_parameter_clamp(SYNTH_PARAM_CHORUS_DEPTH, (float)depth, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_chorus_set_depth(&s->voices[i].effects.chorus, s->effects.chorus.depth);
}

void synth_set_chorus_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.chorus.mix = (float)synth_parameter_clamp(SYNTH_PARAM_CHORUS_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_chorus_set_mix(&s->voices[i].effects.chorus, s->effects.chorus.mix);
}

void synth_set_chorus_width(synth *s, float width)
{
    if (!isfinite((float)width)) return;
    s->effects.chorus.width = (float)synth_parameter_clamp(SYNTH_PARAM_CHORUS_WIDTH, (float)width, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_chorus_set_width(&s->voices[i].effects.chorus, s->effects.chorus.width);
}

void synth_set_chorus_delay(synth *s, float seconds)
{
    if (!isfinite((float)seconds)) return;
    s->effects.chorus.delay_seconds = (float)synth_parameter_clamp(SYNTH_PARAM_CHORUS_DELAY, (float)seconds, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_chorus_set_delay(&s->voices[i].effects.chorus, s->effects.chorus.delay_seconds);
}

void synth_set_chorus_feedback(synth *s, float feedback)
{
    if (!isfinite((float)feedback)) return;
    s->effects.chorus.feedback = (float)synth_parameter_clamp(SYNTH_PARAM_CHORUS_FEEDBACK, (float)feedback, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_chorus_set_feedback(&s->voices[i].effects.chorus, s->effects.chorus.feedback);
}

void synth_set_eq_low(synth *s, float gain_db)
{
    if (!isfinite((float)gain_db)) return;
    s->effects.eq.low_gain_db = (float)synth_parameter_clamp(SYNTH_PARAM_EQ_LOW, (float)gain_db, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_eq_set_low(&s->voices[i].effects.eq, s->effects.eq.low_gain_db);
}

void synth_set_eq_mid(synth *s, float gain_db)
{
    if (!isfinite((float)gain_db)) return;
    s->effects.eq.mid_gain_db = (float)synth_parameter_clamp(SYNTH_PARAM_EQ_MID, (float)gain_db, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_eq_set_mid(&s->voices[i].effects.eq, s->effects.eq.mid_gain_db);
}

void synth_set_eq_high(synth *s, float gain_db)
{
    if (!isfinite((float)gain_db)) return;
    s->effects.eq.high_gain_db = (float)synth_parameter_clamp(SYNTH_PARAM_EQ_HIGH, (float)gain_db, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_eq_set_high(&s->voices[i].effects.eq, s->effects.eq.high_gain_db);
}

void synth_set_delay_time(synth *s, float seconds)
{
    if (!isfinite((float)seconds)) return;
    s->effects.delay.time_seconds = (float)synth_parameter_clamp(SYNTH_PARAM_DELAY_TIME, (float)seconds, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_delay_set_time(&s->voices[i].effects.delay, s->effects.delay.time_seconds);
}

void synth_set_delay_feedback(synth *s, float feedback)
{
    if (!isfinite((float)feedback)) return;
    s->effects.delay.feedback = (float)synth_parameter_clamp(SYNTH_PARAM_DELAY_FEEDBACK, (float)feedback, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_delay_set_feedback(&s->voices[i].effects.delay, s->effects.delay.feedback);
}

void synth_set_delay_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.delay.mix = (float)synth_parameter_clamp(SYNTH_PARAM_DELAY_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_delay_set_mix(&s->voices[i].effects.delay, s->effects.delay.mix);
}

void synth_set_plate_reverb_decay(synth *s, float seconds)
{
    if (!isfinite((float)seconds)) return;
    s->effects.plate_reverb.decay_seconds = (float)synth_parameter_clamp(SYNTH_PARAM_PLATE_REVERB_DECAY, (float)seconds, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_plate_reverb_set_decay(&s->voices[i].effects.plate_reverb, s->effects.plate_reverb.decay_seconds);
}

void synth_set_plate_reverb_damping(synth *s, float damping)
{
    if (!isfinite((float)damping)) return;
    s->effects.plate_reverb.damping = (float)synth_parameter_clamp(SYNTH_PARAM_PLATE_REVERB_DAMPING, (float)damping, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_plate_reverb_set_damping(&s->voices[i].effects.plate_reverb, s->effects.plate_reverb.damping);
}

void synth_set_plate_reverb_mix(synth *s, float mix)
{
    if (!isfinite((float)mix)) return;
    s->effects.plate_reverb.mix = (float)synth_parameter_clamp(SYNTH_PARAM_PLATE_REVERB_MIX, (float)mix, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_plate_reverb_set_mix(&s->voices[i].effects.plate_reverb, s->effects.plate_reverb.mix);
}

void synth_set_plate_reverb_predelay(synth *s, float seconds)
{
    if (!isfinite((float)seconds)) return;
    s->effects.plate_reverb.predelay_seconds = (float)synth_parameter_clamp(SYNTH_PARAM_PLATE_REVERB_PREDELAY, (float)seconds, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_plate_reverb_set_predelay(&s->voices[i].effects.plate_reverb, s->effects.plate_reverb.predelay_seconds);
}

void synth_set_compressor_threshold(synth *s, float threshold_db)
{
    if (!isfinite((float)threshold_db)) return;
    s->effects.compressor.threshold_db = (float)synth_parameter_clamp(SYNTH_PARAM_COMPRESSOR_THRESHOLD, (float)threshold_db, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_compressor_set_threshold(&s->voices[i].effects.compressor, s->effects.compressor.threshold_db);
}

void synth_set_compressor_ratio(synth *s, float ratio)
{
    if (!isfinite((float)ratio)) return;
    s->effects.compressor.ratio = (float)synth_parameter_clamp(SYNTH_PARAM_COMPRESSOR_RATIO, (float)ratio, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_compressor_set_ratio(&s->voices[i].effects.compressor, s->effects.compressor.ratio);
}

void synth_set_compressor_makeup_gain(synth *s, float makeup_gain_db)
{
    if (!isfinite((float)makeup_gain_db)) return;
    s->effects.compressor.makeup_gain_db = (float)synth_parameter_clamp(SYNTH_PARAM_COMPRESSOR_MAKEUP_GAIN, (float)makeup_gain_db, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_compressor_set_makeup_gain(&s->voices[i].effects.compressor, s->effects.compressor.makeup_gain_db);
}

void synth_set_compressor_attack_seconds(synth *s, float seconds)
{
    if (!isfinite((float)seconds)) return;
    s->effects.compressor.attack_seconds = (float)synth_parameter_clamp(SYNTH_PARAM_COMPRESSOR_ATTACK_SECONDS, (float)seconds, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_compressor_set_attack(&s->voices[i].effects.compressor, s->effects.compressor.attack_seconds);
}

void synth_set_compressor_release_seconds(synth *s, float seconds)
{
    if (!isfinite((float)seconds)) return;
    s->effects.compressor.release_seconds = (float)synth_parameter_clamp(SYNTH_PARAM_COMPRESSOR_RELEASE_SECONDS, (float)seconds, s->sample_rate);
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i)
        synth_compressor_set_release(&s->voices[i].effects.compressor, s->effects.compressor.release_seconds);
}

float synth_get_master_gain(const synth *s)
{
    return s->master_gain;
}

float synth_get_oscillator_morph(const synth *s)
{
    return s->oscillator_morph;
}

float synth_get_first_oscillator_gain(const synth *s)
{
    return s->first_oscillator_gain;
}

float synth_get_second_oscillator_gain(const synth *s)
{
    return s->second_oscillator_gain;
}

float synth_get_stereo_spread(const synth *s)
{
    return s->stereo_spread;
}

float synth_get_second_oscillator_morph(const synth *s)
{
    return s->second_oscillator_morph;
}

int synth_get_second_oscillator_octave(const synth *s)
{
    return s->second_oscillator_octave;
}

int synth_get_second_oscillator_pitch(const synth *s)
{
    return s->second_oscillator_pitch_semitones;
}

float synth_get_second_oscillator_fine_tune(const synth *s)
{
    return s->second_oscillator_fine_tune_cents;
}

float synth_get_filter_cutoff(const synth *s)
{
    return s->filter.cutoff_hz;
}

int synth_get_filter_poles(const synth *s)
{
    return s->filter.pole_count;
}

float synth_get_lfo_rate(const synth *s)
{
    return s->lfo.frequency_hz;
}

float synth_get_lfo_shape_morph(const synth *s)
{
    return s->lfo.morph;
}

float synth_get_lfo_depth(const synth *s)
{
    return s->lfo_depth;
}

float synth_get_saturation_drive(const synth *s)
{
    return s->effects.saturation.drive;
}

float synth_get_saturation_mix(const synth *s)
{
    return s->effects.saturation.mix;
}

float synth_get_distortion_drive(const synth *s)
{
    return s->effects.distortion.drive;
}

float synth_get_distortion_mix(const synth *s)
{
    return s->effects.distortion.mix;
}

float synth_get_bitcrusher_sample_rate(const synth *s)
{
    return s->effects.bitcrusher.sample_rate;
}

int synth_get_bitcrusher_bits(const synth *s)
{
    return s->effects.bitcrusher.bits;
}

float synth_get_bitcrusher_mix(const synth *s)
{
    return s->effects.bitcrusher.mix;
}

float synth_get_flanger_rate(const synth *s)
{
    return s->effects.flanger.rate_hz;
}

float synth_get_flanger_intensity(const synth *s)
{
    return s->effects.flanger.intensity;
}

float synth_get_flanger_depth(const synth *s)
{
    return s->effects.flanger.depth;
}

float synth_get_flanger_feedback(const synth *s)
{
    return s->effects.flanger.feedback;
}

float synth_get_flanger_mix(const synth *s)
{
    return s->effects.flanger.mix;
}

float synth_get_flanger_manual(const synth *s)
{
    return s->effects.flanger.manual_delay_seconds;
}

float synth_get_ring_mod_frequency(const synth *s)
{
    return s->effects.ring_mod.frequency_hz;
}

float synth_get_ring_mod_rectify(const synth *s)
{
    return s->effects.ring_mod.rectify;
}

float synth_get_ring_mod_mix(const synth *s)
{
    return s->effects.ring_mod.mix;
}

float synth_get_chorus_rate(const synth *s)
{
    return s->effects.chorus.rate_hz;
}

float synth_get_chorus_depth(const synth *s)
{
    return s->effects.chorus.depth;
}

float synth_get_chorus_mix(const synth *s)
{
    return s->effects.chorus.mix;
}

float synth_get_chorus_width(const synth *s)
{
    return s->effects.chorus.width;
}

float synth_get_chorus_delay(const synth *s)
{
    return s->effects.chorus.delay_seconds;
}

float synth_get_chorus_feedback(const synth *s)
{
    return s->effects.chorus.feedback;
}

float synth_get_eq_low(const synth *s)
{
    return s->effects.eq.low_gain_db;
}

float synth_get_eq_mid(const synth *s)
{
    return s->effects.eq.mid_gain_db;
}

float synth_get_eq_high(const synth *s)
{
    return s->effects.eq.high_gain_db;
}

float synth_get_delay_time(const synth *s)
{
    return s->effects.delay.time_seconds;
}

float synth_get_delay_feedback(const synth *s)
{
    return s->effects.delay.feedback;
}

float synth_get_delay_mix(const synth *s)
{
    return s->effects.delay.mix;
}

float synth_get_plate_reverb_decay(const synth *s)
{
    return s->effects.plate_reverb.decay_seconds;
}

float synth_get_plate_reverb_damping(const synth *s)
{
    return s->effects.plate_reverb.damping;
}

float synth_get_plate_reverb_mix(const synth *s)
{
    return s->effects.plate_reverb.mix;
}

float synth_get_plate_reverb_predelay(const synth *s)
{
    return s->effects.plate_reverb.predelay_seconds;
}

float synth_get_compressor_threshold(const synth *s)
{
    return s->effects.compressor.threshold_db;
}

float synth_get_compressor_ratio(const synth *s)
{
    return s->effects.compressor.ratio;
}

float synth_get_compressor_makeup_gain(const synth *s)
{
    return s->effects.compressor.makeup_gain_db;
}

float synth_get_compressor_attack_seconds(const synth *s)
{
    return s->effects.compressor.attack_seconds;
}

float synth_get_compressor_release_seconds(const synth *s)
{
    return s->effects.compressor.release_seconds;
}
