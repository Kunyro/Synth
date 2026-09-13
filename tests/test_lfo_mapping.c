#include "midi/midi_mapping.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

// collects failures so one run can report multiple mapping problems
static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

// feeds a channel-1 control-change packet directly to the adapter, without midi hardware
static int send_cc(midi_mapping *mapping, synth *s, int control, int value,
                   midi_mapping_apply_result *result)
{
    const unsigned char bytes[] = {0xb0, (unsigned char)control, (unsigned char)value};
    return midi_mapping_apply_short_message(mapping, bytes, sizeof(bytes), s, result);
}

// finds the cc assigned to a particular base or amount property in this test mapping
static int control_for(const midi_mapping *mapping, synth_parameter_id id, midi_mapping_target_kind kind)
{
    for (size_t i = 0; i < mapping->binding_count; ++i) {
        if (mapping->bindings[i].parameter == id && mapping->bindings[i].target_kind == kind) {
            return mapping->bindings[i].control;
        }
    }
    return -1;
}

// builds every base/amount binding, saves and reloads it, then checks pickup, exact
// signed endpoints, and that config loading never enables modulation
static void test_all_bindings_and_roundtrip(const char *path)
{
    midi_mapping original, loaded;
    midi_mapping_parameter_info first, second;
    char error[MIDI_MAPPING_ERROR_LENGTH];
    synth s;
    midi_mapping_init(&original);
    check(midi_mapping_parameter_count() == 107, "complete base/global/amount surface");
    for (size_t i = 0; i < midi_mapping_parameter_count(); ++i) {
        midi_mapping_parameter_info info;
        midi_mapping_binding *binding = &original.bindings[i];
        check(midi_mapping_parameter_info_at(i, &info), "enumerates control metadata");
        binding->parameter = info.parameter;
        binding->target_kind = info.target_kind;
        binding->source_type = MIDI_MAPPING_SOURCE_CC;
        binding->channel = 1;
        binding->control = (int)i;
        binding->scale = info.default_scale;
        binding->min_value = info.default_min_value;
        binding->max_value = info.default_max_value;
        ++original.binding_count;
    }
    // float bounds must survive serialization without a six-digit precision loss
    original.bindings[SYNTH_PARAM_ATTACK].min_value = 0.012345678f;
    original.bindings[SYNTH_PARAM_ATTACK].max_value = 1.2345678f;
    // include navigation and chords to ensure round trips retain adapter controls too
    original.effect_banks[0].selector = (midi_mapping_control_binding){1, MIDI_MAPPING_SOURCE_CC, 2, 10};
    original.effect_banks[1].macros[2] = (midi_mapping_control_binding){1, MIDI_MAPPING_SOURCE_CC, 2, 11};
    original.chord_bindings[MIDI_CHORD_MODE_PAD_MAJOR] =
        (midi_mapping_chord_binding){1, MIDI_CHORD_MODE_PAD_MAJOR, MIDI_MAPPING_SOURCE_CC, 2, 12};
    check(midi_mapping_save(&original, path, error, sizeof(error)), "saves complete mapping");
    check(midi_mapping_load(&loaded, path, error, sizeof(error)), "loads more than 64 bindings");
    check(loaded.binding_count == original.binding_count, "round trip keeps all 107 bindings");
    check(loaded.effect_banks[0].selector.control == 10 &&
          loaded.effect_banks[1].macros[2].control == 11 &&
          loaded.chord_bindings[MIDI_CHORD_MODE_PAD_MAJOR].control == 12, "round trip keeps macro/chord controls");
    for (size_t i = 0; i < loaded.binding_count; ++i) {
        const midi_mapping_binding *a = &original.bindings[i], *b = &loaded.bindings[i];
        check(a->parameter == b->parameter && a->target_kind == b->target_kind &&
              a->control == b->control && a->scale == b->scale &&
              a->min_value == b->min_value && a->max_value == b->max_value,
              "binding identity/range survives serialization exactly");
    }
    check(midi_mapping_parameter_info_by_name("lfo_amount.delay_mix", &first), "canonical amount name resolves");
    check(midi_mapping_parameter_info_by_name("filter_cutoff", &second), "another metadata lookup resolves");
    check(strcmp(first.name, "lfo_amount.delay_mix") == 0, "metadata does not share mutable scratch storage");
    synth_init(&s, 8000);
    check(synth_get_lfo_depth(&s) == 0, "global LFO is initially off");
    for (size_t i = 0; i < loaded.binding_count; ++i) {
        midi_mapping_binding *binding = &loaded.bindings[i];
        midi_mapping_apply_result result;
        if (binding->target_kind != MIDI_MAPPING_TARGET_LFO_AMOUNT) continue;
        check(synth_get_lfo_amount(&s, binding->parameter) == 0, "binding does not initialize route amount");
        check(!send_cc(&loaded, &s, binding->control, 110, &result), "amount waits for soft takeover");
        check(send_cc(&loaded, &s, binding->control, 64, &result), "center picks up zero amount");
        check(send_cc(&loaded, &s, binding->control, 127, &result), "every amount can reach full positive depth");
        check(result.parameter == binding->parameter && result.target_kind == MIDI_MAPPING_TARGET_LFO_AMOUNT,
              "apply result distinguishes route amount from base value");
        check(synth_get_lfo_amount(&s, binding->parameter) == 1, "positive endpoint exact");
        check(send_cc(&loaded, &s, binding->control, 0, &result), "every amount can reach negative depth");
        check(synth_get_lfo_amount(&s, binding->parameter) == -1, "negative endpoint exact");
        send_cc(&loaded, &s, binding->control, 63, &result);
        check(synth_get_lfo_amount(&s, binding->parameter) == 0, "CC 63 is exact zero");
        send_cc(&loaded, &s, binding->control, 64, &result);
        check(synth_get_lfo_amount(&s, binding->parameter) == 0, "CC 64 is exact zero");
    }
    {
        const int base_cc = control_for(&loaded, SYNTH_PARAM_DELAY_MIX, MIDI_MAPPING_TARGET_BASE);
        const int amount_cc = control_for(&loaded, SYNTH_PARAM_DELAY_MIX, MIDI_MAPPING_TARGET_LFO_AMOUNT);
        midi_mapping_apply_result result;
        float samples[64];
        synth_set_delay_mix(&s, 0.5f);
        synth_set_lfo_depth(&s, 1);
        send_cc(&loaded, &s, amount_cc, 127, &result);
        synth_lfo_reset(&s.lfo, 0.25f);
        synth_render_mono(&s, samples, 64);
        check(!send_cc(&loaded, &s, base_cc, 120, &result), "base pickup ignores moving effective mix");
        check(send_cc(&loaded, &s, base_cc, 64, &result), "base pickup follows stored knob value");
        check(result.target_kind == MIDI_MAPPING_TARGET_BASE, "base edit reports base property");
        check(synth_get_lfo_amount(&s, SYNTH_PARAM_DELAY_MIX) == 1, "base edit leaves route amount intact");
        check(midi_mapping_save(&loaded, path, error, sizeof(error)), "saving after runtime edits succeeds");
        check(midi_mapping_load(&original, path, error, sizeof(error)), "runtime save reloads");
        check(!original.bindings[base_cc].pickup.picked_up, "pickup state is not persisted");
    }
    synth_uninit(&s);
}

// checks that excluded targets and malformed route declarations fail with their line number
static void test_invalid_declarations(const char *path)
{
    static const char *invalid[] = {
        "lfo_amount.lfo_rate=cc:1:1:linear:-1:1\n",
        "lfo_amount.lfo_depth=cc:1:1:linear:-1:1\n",
        "lfo_amount.lfo_shape_morph=cc:1:1:linear:-1:1\n",
        "lfo_amount.lfo_amount.delay_mix=cc:1:1:linear:-1:1\n",
        "lfo_amount.effect_selector_1=cc:1:1:linear:-1:1\n",
        "lfo_amount.chord_major=cc:1:1:linear:-1:1\n",
        "lfo_filter_amount=cc:1:1:linear:0:1\n",
        "lfo_amount.unknown=cc:1:1:linear:0:1\n",
        "lfo_amount.delay_mix=0.5\n",
        "lfo_amount.delay_mix=cc:1:1:linear:nan:1\n",
        "lfo_amount.delay_mix=cc:1:1:linear:-1:inf\n",
        "lfo_amount.delay_mix=cc:1:1:linear:-2:1\n",
        "lfo_amount.delay_mix=cc:1:1:log:-1:1\n",
        "lfo_amount.delay_mix=cc:1:1:linear:1:1\n",
        "lfo_amount.delay_mix=cc:1:1:linear:-1:1:extra\n",
        "lfo_amount.delay_mix=cc::1:1:linear:-1:1\n"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        midi_mapping mapping;
        char error[MIDI_MAPPING_ERROR_LENGTH];
        FILE *file = fopen(path, "w");
        check(file != NULL, "opens invalid fixture");
        if (file == NULL) return;
        fputs(invalid[i], file);
        fclose(file);
        check(!midi_mapping_load(&mapping, path, error, sizeof(error)), "rejects invalid route declaration");
        check(strncmp(error, "line 1:", 7) == 0, "invalid declaration reports line number");
    }
}

// runs mapping checks using a temporary config file and removes that fixture afterward
int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "build/lfo_mapping_test.conf";
    test_all_bindings_and_roundtrip(path);
    test_invalid_declarations(path);
    remove(path);
    return failures ? 1 : 0;
}
