#ifndef SYNTH_DELAY_H
#define SYNTH_DELAY_H

#include <stddef.h>

#include "synth/audio_types.h"

// default max delay frames at the project's 48 khz default sample rate
// initialized delays expose their actual per-sample-rate limit in max_delay_frames
#define SYNTH_DELAY_MAX_FRAMES 96000
#define SYNTH_DELAY_MIN_TIME_SECONDS 0.001f
#define SYNTH_DELAY_MAX_TIME_SECONDS 2.0f
#define SYNTH_DELAY_DEFAULT_TIME_SECONDS 0.25f
#define SYNTH_DELAY_MAX_FEEDBACK 0.95f
#define SYNTH_DELAY_TIME_CROSSFADE_SECONDS 0.030f
#define SYNTH_DELAY_TIME_SETTLE_SECONDS 0.050f
#define SYNTH_DELAY_VOICE_COUNT 4
#define SYNTH_DELAY_TAIL_SILENCE_THRESHOLD 0.0001f

// effective controls, separate from persistent dsp history
typedef struct synth_delay_params {
    float time_seconds;
    float feedback;
    float mix;
} synth_delay_params;

typedef enum synth_delay_voice_state {
    SYNTH_DELAY_VOICE_INACTIVE = 0,
    SYNTH_DELAY_VOICE_MAIN,
    SYNTH_DELAY_VOICE_TAIL
} synth_delay_voice_state;

typedef struct synth_delay_line {
    float *left;
    float *right;
    size_t write_index;
    size_t capacity_frames;
    float delay_frames;
} synth_delay_line;

typedef struct synth_delay_voice {
    synth_delay_line line;
    synth_delay_voice_state state;
    size_t quiet_frames;
    float output_gain;
    float last_level;
} synth_delay_voice;

typedef struct synth_delay {
    synth_delay_voice voices[SYNTH_DELAY_VOICE_COUNT];
    size_t main_voice_index;
    float pending_delay_frames;
    size_t crossfade_position;
    size_t crossfade_frames;
    size_t settle_frames;
    size_t settle_frames_remaining;
    int crossfading;
    int has_pending_time_change;
    int has_processed;
    float sample_rate;
    size_t max_delay_frames;
    float time_seconds;
    float feedback;
    float mix;
} synth_delay;

void synth_delay_init(synth_delay *delay, float sample_rate);
void synth_delay_uninit(synth_delay *delay);
void synth_delay_set_sample_rate(synth_delay *delay, float sample_rate);
void synth_delay_set_time(synth_delay *delay, float seconds);
void synth_delay_set_feedback(synth_delay *delay, float feedback);
void synth_delay_set_mix(synth_delay *delay, float mix);
float synth_delay_get_time(const synth_delay *delay);
float synth_delay_get_feedback(const synth_delay *delay);
float synth_delay_get_mix(const synth_delay *delay);
synth_stereo_sample synth_delay_process(
    synth_delay *delay,
    synth_stereo_sample input);

// returns a copy of stored controls; processing overrides never change those bases
synth_delay_params synth_delay_get_params(const synth_delay *effect);
// effective controls must be finite and within the module bounds
// advances dsp history without storing controls or calling parameter setters
synth_stereo_sample synth_delay_process_with_params(
    synth_delay *effect,
    synth_stereo_sample input,
    const synth_delay_params *params);

#endif
