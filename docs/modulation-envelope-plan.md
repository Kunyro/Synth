# Per-voice modulation envelope implementation plan

Status: implemented and validated. This document retains the agreed contract,
pre-implementation review, and implementation sequence. Completed validation and
desktop measurements are recorded below. See [the API contract](modulation.md)
for current behavior and usage.

## Confirmed requirements

- An independent modulation envelope for each voice, triggered for each note.
- Held ADSR behavior.
- All audio processing, including filters, effects, and destination gain, is
  per voice before the voice outputs are summed.
- The modulation envelope can target the LFO's destinations except volume
  attack, decay, sustain, and release: 48 destinations. Its own source controls
  are excluded too. Existing LFO destination eligibility is retained.
- Additive LFO and envelope contributions.
- Envelope route amounts range from -1 to +1. Their sign controls modulation
  direction; effective parameter values remain inside each destination's bounds.
- For all 48 envelope destinations, at full envelope depth and the source peak,
  amount +1 reaches the legal maximum and amount -1 reaches the legal minimum
  when no other modulation counteracts it. Amount 0 contributes nothing. This
  uses endpoint-relative envelope scaling; the LFO retains its fixed excursion.
- Volume ADSR note-on capture remains an existing LFO behavior; modulation
  envelope routes to those parameters are rejected, removing the zero-capture
  ambiguity.
- The modulation envelope uses full-duration release from its current level.
- MIDI mapping with the LFO's binding, pickup, learn, and save/load capabilities.
- Teensy 4.1 remains a future target. Embedded optimization and memory work are
  deferred; they do not constrain the current per-voice processing decision.
- Retire the modulation envelope when its volume envelope finishes. This means
  the volume envelope reaches its off stage, not merely that its level is zero
  during a held note with zero sustain.
- Let the voice's effect tails finish after the volume/modulation envelopes end.
- When stealing a voice, fade its output and reset its effect history before
  using that processing state for the new note.
- Keep one shared set of knob settings and route amounts and one global,
  free-running LFO. Each voice owns its audio/effect histories and modulation
  envelope progression.
- The modulation envelope is fully off by default, with zero audible
  contribution within the new per-voice architecture. Preserving the former
  shared-effects audio response is not required.
- Manual modulation ADSR edits follow the existing volume ADSR setter: replace
  all four settings on existing instances while preserving stage and level,
  and use the updated settings for future notes.
- Prioritize modularity, readability, and avoiding technical debt without using
  production cost to restrict the design.
- All new or modified code comments must be lowercase.

## Source settings and defaults

The source conventions are linear ADSR stages, reset from zero on each
note, 0–1 output without velocity scaling, and a MIDI-mappable overall depth
control. Shape defaults are 10 ms attack, 80 ms decay, 75% sustain, and 160 ms
release; depth and route amounts start at zero. Route polarity is signed -1–1;
source output remains unipolar 0–1. Full-duration release applies to the
modulation envelope; existing volume-envelope timing remains as implemented.

Disabling a route or overall depth changes its contribution, not note ownership
or effect history. Source progression follows note lifetime so enabling a route
during a held note uses that note's envelope position. The source ends when the
volume envelope reaches its off stage, even if its own release would last longer.

## Behavior examples

With no competing LFO contribution and overall envelope depth 1:

| Destination and stored base | Amount | Value at envelope peak |
| --- | --- | --- |
| Filter cutoff, 1000 Hz at 48 kHz sample rate | +1 | 24000 Hz |
| Filter cutoff, 1000 Hz at 48 kHz sample rate | -1 | 10 Hz |
| Effect mix, 0.3 | +1 | 1.0 |
| Effect mix, 0.3 | -1 | 0.0 |
| Effect mix, 0.3 | +0.5 | 0.65 |
| Effect mix, 0.3 | -0.5 | 0.15 |

With envelope level 0 or depth 0, all of those routes contribute zero. Parameter
getters always return stored bases. Legal endpoints come from the engine, not
the configured MIDI knob travel. A second note starts its own envelope without
retriggering the first note's envelope or the global LFO.

## Deferred Teensy feasibility review

The Teensy 4.1 has a 600 MHz Cortex-M7 with floating-point hardware and 1024 KiB
of on-chip RAM, plus optional external memory support
([PJRC specifications](https://www.pjrc.com/store/teensy41.html)).

Static allocation analysis at the engine's current 48 kHz default:

- One delay allocates four stereo history slots, each holding two seconds of
  32-bit float samples: `4 * 2 * 48000 * 2 * 4 = 3072000` bytes, or 2.93 MiB.
- Twelve copies require 35.16 MiB for delay sample buffers alone, excluding
  reverb, other effects, allocator overhead, and the engine itself.
- These buffers are allocated at initialization even when delay mix is zero.
- The shared wavetable sample arrays occupy another 369000 bytes, plus metadata.
  They are currently mutable static storage initialized at runtime.

Sources: `include/synth/delay.h`, `src/effects/delay.c`, and `src/wavetable.c`.
These are calculated storage requirements, not measurements of Teensy CPU use.

The selected architecture uses per-voice processing despite these costs.
Per-voice compression and distortion change how simultaneous notes interact,
independently of performance. Existing polyphonic audio cannot be required to
remain byte-identical to the former shared-effects signal path.

Before claiming the current engine fits Teensy, plan buffer sizing and memory
placement, determine whether external PSRAM is available, choose the embedded
sample rate, and benchmark on hardware. Normal `calloc` calls do not explicitly
select external PSRAM. Consider configurable buffer capacities or an explicit
host-provided allocator/storage boundary. Any reduced maximum delay time or
changed tail behavior requires a separate product decision.

## Code review findings

| Location | Finding | Planned treatment |
| --- | --- | --- |
| `src/modulation.c` | Generic-looking evaluation reads LFO amounts and depth directly. LFO reset clears the entire modulation struct. | Separate source routes, share destination evaluation, and reset only the selected source. |
| `include/synth/parameter.h`, `src/parameter.c` | One boolean marks destination eligibility, which now differs between sources. | Represent source-specific eligibility centrally: 52 existing LFO destinations and 48 envelope destinations. Use it for engine validation, MIDI enumeration, and config validation. |
| `src/modulation.c`, `src/parameter.c` | Fixed modulation spans do not guarantee the envelope reaches a destination endpoint. Cutoff and bitcrusher bounds depend on sample rate. | Share destination-domain arithmetic and expose resolved legal bounds; distinguish the LFO's fixed-span contribution from the envelope's endpoint-relative contribution. |
| `src/parameter.c`, `src/internal/render_parameters.h` | One LFO sample resolves one frame shared by every voice and effect. | Resolve a complete frame for each voice using explicit source samples. Keep one destination catalog. |
| `src/synth.c` | Note-off eligibility is inferred from the volume envelope's stage; allocation uses voice activity and volume level. | Make note gate ownership explicit and integrate both envelope lifecycles with allocation and release. |
| `src/voice.c` | A voice becomes inactive when its volume envelope ends. | Retire its modulation envelope at that boundary; keep effect-tail activity separate. |
| `src/envelope.c` | Linear stages, zero-level retrigger, and fixed-slope release are existing behavior. | Reuse the ADSR mechanism where semantics agree; make any required timing distinction explicit and test it without silently changing existing volume behavior. |
| `src/parameter.c` | Volume ADSR capture peeks at the LFO at note-on; manual volume ADSR edits overwrite captures. | Preserve that LFO behavior and reject modulation-envelope routes to the four volume ADSR controls. |
| `src/effects/`, `src/synth.c` | One effects instance currently owns both editable bases and DSP history. Resource-owning modules expose initialization/destruction but no common allocation-free reset contract. | Separate authoritative patch controls from per-voice history, preserve manual-setter side effects, and provide lifecycle/reset operations for the selected tail policy. |
| Desktop MIDI registry, runtime, validation, host, and monitor | Route naming and access repeatedly special-case the LFO. | Centralize route metadata, naming, and get/set dispatch for both sources. |
| `tests/test_lfo_mapping.c` | Full-catalog fixture assigns every control to channel 1 with its index as the CC number. | Allocate fixtures across channels when the expanded catalog exceeds 128 controls; update lookup and message helpers to retain channel identity. |

## Intended module responsibilities

- `envelope.c`: ADSR progression and stage timing. No routing or MIDI knowledge.
- `voice.c`: per-note oscillator, filter, effect, and envelope state, gate/tail
  lifecycle, and rendering from resolved voice controls.
- `modulation.c`: independent route banks, source contribution combination, and
  destination-domain evaluation.
- `parameter.c`: authoritative immutable parameter metadata, base access, and
  dispatch into typed effective controls and source-specific route eligibility.
- `synth_render.c`: sample clock ownership, per-voice resolution, and summing
  finished voice outputs. Each source advances exactly once per applicable sample.
- `synth.c`: synth-level controls and note operations integrating these modules.
- Desktop MIDI adapter: controller syntax, scaling, pickup, validation, and
  serialization. Engine code remains independent of this adapter.

## Implementation sequence

### 1. Establish contract tests and lifecycle boundaries

Translate the agreed behavior and examples above into independent expectations
for source timing, destination eligibility, endpoint scaling, and note lifecycle.
Preserve the existing global LFO ADSR capture ordering. There is no envelope
aggregation or cross-note capture.

Distinguish held/releasing notes, tail-only voices, free slots, and slots fading
for replacement. Centralize the voice-steal fade duration as an internal constant
and keep pending-note storage bounded. Cover note-off and all-notes-off arriving
during that fade so a delayed note cannot start with a stuck gate. Prefer a free
slot, then a tail-only slot, before interrupting a held/releasing note. Retain
the existing quietest-volume-envelope ranking for competing musical voices;
use output activity for comparing tail-only voices.

### 2. Refactor modulation and destination eligibility

Introduce source-specific route storage and shared route validation, preserving
existing LFO entry points and behavior. Generalize pure destination evaluation
to accept combined source contributions independently of source advancement.
Encode source-specific eligibility once and test the two destination inventories
independently, including rejection of envelope-to-volume-ADSR routes.

Evaluation retains existing destination domains and the LFO's fixed
spans, while envelope excursion depends on the stored base and chosen endpoint.
Use this rule for every eligible envelope destination:

```text
d(x) = x for linear parameters, log2(x) for logarithmic parameters
b = d(stored_base)
endpoint = legal_maximum if envelope_amount >= 0, otherwise legal_minimum

lfo_offset = lfo_sample * lfo_depth * lfo_amount * lfo_span
envelope_offset = envelope_sample * envelope_depth * abs(envelope_amount)
                * (d(endpoint) - b)

effective = inverse_d(b + lfo_offset + envelope_offset)
```

Clamp and round the combined result once. Preserve exact base values for zero
contribution. Preserve the existing LFO-only arithmetic path where necessary to
avoid logarithmic round-trip drift. With LFO contribution zero, implement exact
endpoint returns at full envelope excursion before the final bounds/rounding
step. Obtain dynamic limits from the same helper used for clamping, not MIDI
knob ranges. Handle degenerate ranges before evaluating logarithms.

For example, cutoff's maximum at 48 kHz is 24 kHz. With base 100 Hz, the LFO's
five-octave positive excursion reaches 3200 Hz, whereas a full positive envelope
excursion must reach 24 kHz. Fractional envelope amounts interpolate toward the
endpoint in the destination's domain, so frequency sweeps remain logarithmic.

Contributions are calculated from authoritative stored bases, not another
source's already-modulated result. A simultaneous negative LFO contribution may
pull a destination below its maximum even at a full positive envelope peak;
that follows the confirmed additive rule. Do not limit the sum to a single
source's range.

Preserve flanger intensity composition before final component clamping. Compute
direct depth/feedback contributions against their stored bases, then add the
intensity-induced deltas. This prevents endpoint distances from changing with
source evaluation order. Endpoint guarantees concern isolated routes; other
routes affecting the same compound control can contribute too.

### 3. Implement per-voice envelope lifecycle

Add independent modulation envelope state to each voice. Ownership is one
synth-level set of editable shape/routing controls with independently
progressing voice instances. Integrate numbered notes, direct-frequency notes,
repeated-note release, all-notes-off, and voice stealing.

Retire modulation when the volume envelope reaches its off stage. Reset the
modulation state on voice reuse, and apply manual ADSR edits to existing instances
without restarting their stage or changing their current level immediately.
Use explicit finite-value validation at new public control boundaries.

For full-duration release, capture the release starting level and track progress
so the ramp reaches zero after the configured duration. Repeated note-off must
not restart a release. Implement release-time edits through the same centralized
ADSR update path, preserving current level and stage; unchanged release settings
must not reset release progress. Add coverage for mid-release edits. Reuse the
envelope module with explicit timing policy/state rather than maintaining a
copied ADSR algorithm.

### 4. Move the complete processing chain into voices

Each voice resolves its oscillator, filter, effects, and gain controls from that
voice's envelope and its applicable LFO sample. Preserve the existing order
inside each voice: oscillators, volume envelope/velocity, filter, effects, and
destination master gain; sum those completed outputs. The volume envelope
precedes effects, so the selected effect-tail policy can be implemented without
extending the volume envelope.

Maintain one authoritative patch control set and independent DSP histories.
Refactor module control access as needed so getters, generic parameter edits,
and manual effect side effects remain coherent.
Do not leave twelve independently editable copies of synth-wide bases. Preserve
standalone module APIs where practical through explicit control wrappers.

Initialize and allocate each voice's resources outside audio rendering, handle
partial allocation failure, and release all resources at synth destruction.
Implement history reset without allocation. Continue processing a tail-only
voice with zero oscillator input after its envelopes finish; modulation-envelope
contribution is zero, while the global LFO still applies. Use module-aware tail
retirement so a quiet gap between echoes is not mistaken for finished history.
Account for effects with frozen history at zero mix. A reused slot fades its old
output and resets history before starting the new note; no separate pool of
stolen effect tails is requested.

Audit all voice initialization, note-on, destruction, and copying call sites,
including standalone voice APIs and tests. Note-on must not reallocate or leak
effect buffers, and copying resource-owning voice state must not duplicate
ownership of buffer pointers. Update tests that inspect the old synth-level
filter/effect state to inspect the appropriate voice or authoritative controls.

Compute temporary controls from stored bases each frame. Preserve oscillator
phase, effect history, and sample timing. LFO note-on capture must not advance
its source. Continue the global LFO clock during silence. Ensure mono/stereo use
the same source progression.

### 5. Expose engine controls and MIDI mapping

Add modulation-envelope settings to the parameter catalog and independent
amount get/set/reset APIs. Append new parameter IDs to preserve existing IDs.
Use this API naming plan:

- Shape: `synth_set_mod_envelope_adsr()` / `synth_get_mod_envelope_adsr()`.
- Depth: `synth_set_mod_envelope_depth()` / `synth_get_mod_envelope_depth()`.
- Routes: `synth_set_envelope_amount()` / `synth_get_envelope_amount()` /
  `synth_reset_envelope_amounts()` with existing destination IDs.
- Config shape/depth: `mod_envelope_attack`, `mod_envelope_decay`,
  `mod_envelope_sustain`, `mod_envelope_release`, and `mod_envelope_depth`.
- Config routes: `envelope_amount.<destination>`.

Five new source controls yield 60 base controls, 52 LFO amount controls, and
48 envelope amount controls: 160 direct control identities. Keep source controls
excluded as destinations for both sources. The current 256-binding capacity
accommodates this catalog; tests must also check its explicit capacity limit.

Use source-specific destination eligibility, with source controls excluded
from routing and envelope routes to volume ADSR rejected. Share amount scaling
and pickup between sources. Preserve exact zero at CC 63 and 64 for full signed
linear bindings; continue supporting explicitly configured positive-only knob
ranges, with zero at the endpoint. Update parsing, enumeration, learn,
bind/unbind, map-all, diagnostics, and save/load together. Preserve existing
config names and the rule that binding files do not initialize synth values.

### 6. Validate, document, and review

- Preserve LFO source timing, route math, getters, and note-on ADSR capture.
- Verify initialization produces zero modulation-envelope contribution for
  every eligible destination, including notes and effect tails.
- Test the new per-voice signal path against independently processed voice
  references. Changed polyphonic interactions through nonlinear effects are
  intentional; old shared-effects output is not a golden reference for them.
- Independent overlapping voices have independent source progression.
- Both sources reach every agreed destination with unchanged base getters.
- Combined positive/negative contributions cancel correctly and clamp/quantize
  only after addition, including logarithmic destinations and flanger controls.
- Verify cutoff reaches the sample-rate-dependent maximum at full positive
  envelope excursion from both low and high bases. Cover both endpoints and
  fractional amounts on all 48
  destinations, including integer rounding and zero contribution identity.
- Volume ADSR captures remain LFO-only; envelope routes to those destinations
  are rejected by both the engine and MIDI adapter.
- Release timing, held sustain, zero times, manual edits, repeated note-ons,
  all-notes-off, silent voices, and voice stealing match the contract.
- Effect tails and internal histories survive modulation.
- Verify tail-only voices continue after envelope retirement, are not cut off
  during silent gaps between echoes, and fade/reset cleanly when stolen.
- Rendering is invariant to buffer partitioning and mono/stereo selection.
- Full MIDI catalog round trips across valid channel/CC pairs; invalid routes,
  nonfinite values, independent resets, and pickup are covered.
- Run desktop and core-only suites, sanitizers, and full-polyphony render timing
  at supported sample rates. Audit sample processing for allocation and locking.
- Record desktop CPU and memory use for the new architecture. Teensy memory
  placement, profiling, and optimization are deferred to the embedded phase.
- Register any new sources/tests in CMake and Makefile and update the modulation
  contract, engine notes, MIDI documentation, and test documentation.
- Review new and modified code comments for lowercase style.

## Completed validation

The initial baseline had 21 passing tests. After implementation:

- CMake desktop: all 23 tests pass; app and MIDI monitor build successfully.
- Core-only release build: all 19 tests pass.
- Makefile: all 23 tests pass.
- AddressSanitizer and UndefinedBehaviorSanitizer: all 23 desktop tests pass.
- Allocation injection: all 300 effect-buffer allocation sites fail cleanly,
  including partial late-voice initialization. Rendering, note events, steals,
  and parameter edits perform no buffer allocations.
- Both source inventories have audible-destination coverage. Additional tests
  verify endpoint math, full-duration release and edits, independent voices,
  nonlinear per-voice reference audio, default-off identity, retained delay
  tails, exact steal fading, cancellation, and MIDI capacity/round trips.

The tail checks exposed a pre-existing bitcrusher behavior that quantized zero
to a nonzero DC value. Silence now stays zero, with a regression test across
all bit depths. Other nonzero quantization behavior is unchanged outside the
1e-7 silence threshold.

### Desktop measurements

An offline arm64 macOS run with AppleClang 21, `-O3`, and 64-frame stereo blocks
measured 12 voices, both source depths at 0.6, all eligible routes at +0.3,
all effect mixes at 0.5, distortion/saturation drive 8, low EQ +3 dB, and
compressor ratio 4. Each sample rate rendered one second, followed by a burst
replacing all 12 notes. Times are wall time around render calls; initialization
and note-request handling are excluded. Buffer bytes were counted at allocation,
excluding allocator overhead and shared static wavetables.

| Sample rate | Effect buffer bytes | One-second render | Largest normal block | Largest replacement block |
| --- | ---: | ---: | ---: | ---: |
| 44.1 kHz | 36,121,296 | 0.604 s | 1.076 ms | 1.949 ms |
| 48 kHz | 39,315,696 | 0.655 s | 1.002 ms | 1.288 ms |
| 96 kHz | 78,630,816 | 1.309 s | 1.057 ms | 1.807 ms |

The synth struct occupies another 19,464 bytes on this build. This is an offline
stress measurement, not a real-time guarantee: 96 kHz full modulation exceeded
real time, and simultaneous history resets can exceed a 64-frame block budget.
Resetting buffers allocates nothing but still has a measurable memory-clear
cost. No CPU or memory optimization was substituted for the agreed per-voice
architecture. Teensy profiling/placement, Windows/Linux execution, and live MIDI
and audio-hardware audition remain unverified.
