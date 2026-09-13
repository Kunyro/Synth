#ifndef SYNTH_PARAMETER_H
#define SYNTH_PARAMETER_H

#include <stddef.h>

struct synth;

// engine identities are independent of any controller or serialization format
typedef enum synth_parameter_id {
    SYNTH_PARAM_ATTACK,
    SYNTH_PARAM_DECAY,
    SYNTH_PARAM_SUSTAIN,
    SYNTH_PARAM_RELEASE,
    SYNTH_PARAM_MASTER_GAIN,
    SYNTH_PARAM_FILTER_CUTOFF,
    SYNTH_PARAM_FILTER_POLES,
    SYNTH_PARAM_OSCILLATOR_MORPH,
    SYNTH_PARAM_FIRST_OSCILLATOR_GAIN,
    SYNTH_PARAM_SECOND_OSCILLATOR_GAIN,
    SYNTH_PARAM_SECOND_OSCILLATOR_MORPH,
    SYNTH_PARAM_SECOND_OSCILLATOR_OCTAVE,
    SYNTH_PARAM_SECOND_OSCILLATOR_PITCH,
    SYNTH_PARAM_SECOND_OSCILLATOR_FINE_TUNE,
    SYNTH_PARAM_STEREO_SPREAD,
    SYNTH_PARAM_LFO_RATE,
    SYNTH_PARAM_LFO_SHAPE_MORPH,
    SYNTH_PARAM_LFO_DEPTH,
    SYNTH_PARAM_SATURATION_DRIVE,
    SYNTH_PARAM_SATURATION_MIX,
    SYNTH_PARAM_DISTORTION_DRIVE,
    SYNTH_PARAM_DISTORTION_MIX,
    SYNTH_PARAM_BITCRUSHER_SAMPLE_RATE,
    SYNTH_PARAM_BITCRUSHER_BITS,
    SYNTH_PARAM_BITCRUSHER_MIX,
    SYNTH_PARAM_FLANGER_RATE,
    SYNTH_PARAM_FLANGER_INTENSITY,
    SYNTH_PARAM_FLANGER_DEPTH,
    SYNTH_PARAM_FLANGER_FEEDBACK,
    SYNTH_PARAM_FLANGER_MIX,
    SYNTH_PARAM_FLANGER_MANUAL,
    SYNTH_PARAM_RING_MOD_FREQUENCY,
    SYNTH_PARAM_RING_MOD_RECTIFY,
    SYNTH_PARAM_RING_MOD_MIX,
    SYNTH_PARAM_CHORUS_RATE,
    SYNTH_PARAM_CHORUS_DEPTH,
    SYNTH_PARAM_CHORUS_MIX,
    SYNTH_PARAM_CHORUS_WIDTH,
    SYNTH_PARAM_CHORUS_DELAY,
    SYNTH_PARAM_CHORUS_FEEDBACK,
    SYNTH_PARAM_EQ_LOW,
    SYNTH_PARAM_EQ_MID,
    SYNTH_PARAM_EQ_HIGH,
    SYNTH_PARAM_DELAY_TIME,
    SYNTH_PARAM_DELAY_FEEDBACK,
    SYNTH_PARAM_DELAY_MIX,
    SYNTH_PARAM_PLATE_REVERB_DECAY,
    SYNTH_PARAM_PLATE_REVERB_DAMPING,
    SYNTH_PARAM_PLATE_REVERB_MIX,
    SYNTH_PARAM_PLATE_REVERB_PREDELAY,
    SYNTH_PARAM_COMPRESSOR_THRESHOLD,
    SYNTH_PARAM_COMPRESSOR_RATIO,
    SYNTH_PARAM_COMPRESSOR_MAKEUP_GAIN,
    SYNTH_PARAM_COMPRESSOR_ATTACK_SECONDS,
    SYNTH_PARAM_COMPRESSOR_RELEASE_SECONDS,
    SYNTH_PARAM_COUNT
} synth_parameter_id;

typedef enum synth_parameter_type {
    // continuous controls accept fractions; integer controls snap to whole steps
    SYNTH_PARAMETER_CONTINUOUS,
    SYNTH_PARAMETER_INTEGER
} synth_parameter_type;

typedef enum synth_parameter_domain {
    // linear adds native units; log2 scales by powers of two (octaves/doublings)
    SYNTH_DOMAIN_LINEAR,
    SYNTH_DOMAIN_LOG2
} synth_parameter_domain;

typedef struct synth_parameter_info {
    synth_parameter_id id;
    const char *name;
    const char *unit;
    synth_parameter_type type;
    synth_parameter_domain domain;
    // static bounds; cutoff and reduced sample rate also depend on the host rate
    float min_value;
    float max_value;
    // excursion at amount=1 and global depth=1, in native units or octaves
    float modulation_span;
    // zero excludes global lfo controls from being destinations themselves
    int modulatable;
} synth_parameter_info;

// immutable process-lifetime metadata; invalid ids/names return null
const synth_parameter_info *synth_parameter_info_at(synth_parameter_id id);
// finds the exact engine name; midi/config prefixes are handled by the adapter
const synth_parameter_info *synth_parameter_info_by_name(const char *name);
// reads the stored setting, even while its audible value is being modulated
float synth_get_parameter(const struct synth *s, synth_parameter_id id);
// rejects unknown ids and nonfinite values; bounds and quantizes valid values
int synth_set_parameter(struct synth *s, synth_parameter_id id, float value);
// applies static/sample-rate bounds and integer rounding without storing a value
float synth_parameter_clamp(synth_parameter_id id, float value, float sample_rate);

#endif
