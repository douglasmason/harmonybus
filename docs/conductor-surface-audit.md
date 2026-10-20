# Conductor, weaving and FM-1: first audited increment

## Baselines and capabilities

HB baseline: `2e8dfb7664a670ed27bfb4279e005f4006806b55` (0.2.281).
Movy integration baseline: `6a76a90` on `work/shared-core-fm1`, with upstream
Movy `5c01a4fe7745b730ee98111194fc64328ea2659b`. Movy's main branch advances
installer pointers; it does not contain the latest integration implementation.

| Capability | Existing implementation | This increment / remaining work |
| --- | --- | --- |
| One harmonic state | HB bus, shared context, source-scoped override owners | Retained; no second musical engine |
| Source arbitration | Conductor progression underneath latest committed eligible override; LIVE excludes recorded input; INPUT accepts both | Preserve existing priority and fallback; fix bass identity and expose actual resolution |
| Feedback prevention | Authority tracks resolve against conductor, not each other | Covered by source diagnostic regression |
| Actor and external input | Joint Play actors use tracks; FM-1/Launchpad use engine-owned source addresses | No new actor-specific harmonic store |
| Root versus bass | Core harmony has separate fields; override capture previously discarded MIDI register | Fixed for ordinary captures, overlapping live/recorded inputs and scheduled motif chords |
| Rhythm and note weaving | `rhythm_generator_material.h`, `rhythm_generator_motif.h`, `rhythm_generator_clip.h` already apply rhythm to note material and target-only fills | Extend regression coverage for per-event operations; no duplicate rhythm engine |
| Arbitrary pattern-to-pattern weaving | No generic cross-track rhythm donor selection found | Still pending; requires bounded source snapshots and clear edit/live semantics |
| FM-1 USB input | Stock note-on/off and poly pressure, opt-in channel, source fences, shared transport | Reject missing fields, invalid track and sparse MIDI data before batching |
| Remote/emulated diagnostics | Existing parameter transport and native processor architecture | New on-demand `harmony_source_snapshot`; visual integration remains pending |
| Firmware/display/knob mapping | No verified stock mapping | Remains hardware-gated; no firmware changes |

The simpler next step is to extend the existing rhythm material selector to accept
a bounded donor snapshot. A separate arrangement engine or another harmonic bus
would duplicate working logic. Keep latest-intent arbitration as the default;
explicit source priority selection is a separate user-facing feature, not a
silent change to existing sessions.

## Reference audit

Inspected source, not copied code:

- [LOOM-8](https://github.com/charlesvestal/fm1-loom/tree/4a29fcd0b68423eafeab0d2ae0cd37b4603e8c43):
  `docs/MANUAL.md` and `app/seq/seq.c` describe dedicated/track-derived conductor
  sources and exclusion of the source track from following itself. Weave rules
  include alternating bars/steps and exchanging rhythm versus note material.
- [Polly](https://github.com/charlesvestal/fm1-polly/tree/62cd07ea7c95c30021c31e957cea8be448415fa7):
  sections/progressions, style grooves, independent edited groove copies and
  chord-following arrangement are relevant future composition concepts.
- [CTL-1](https://github.com/charlesvestal/fm1-ctl/tree/e181b4a1ac1510ce18ebf6e271a143c2fe364ccd):
  a replacement controller firmware with notes, pressure and named controls.
  Its capabilities do not establish the stock FM-1's CC/display protocol.
- [Schwung](https://github.com/charlesvestal/schwung/tree/586aebd4367952384e1b447e8e3a2fd627974ec8):
  catalog lists Chord Finder and Impressive Chords; reuse host parameter/knob
  infrastructure instead of duplicating their user-interface services.

LOOM-8, Polly and CTL-1 identify GPL-3.0 application code; their platform and
third-party components carry separate licenses. No third-party implementation
is incorporated by this increment. Any later code reuse needs a file-level
license review before inclusion in HB's MIT module.

## Source diagnostic contract

Read `harmony_source_snapshot` through the existing HB parameter API (or the
hosted `midi_fx1:` prefix). A buffer of 512 bytes is sufficient for schema 1.
Undersized buffers return -1 and an empty string, never partial JSON.

```json
{"schema":1,"source":"override","owner":0,"track":4,"scope":"live","root":0,"bass":4,"observed_root":2,"observed_bass":2,"candidates":1,"pending":0}
```

`owner` is the zero-based instance index; `track` is zero-based Movy track or -1.
Roots/basses are pitch classes, or -1 when invalid. `root`/`bass` describe this
track's effective render harmony; observed fields describe the underlying bus.
An authority track itself reports conductor, preventing self-feedback. `scope`
is `live`, `input` or `none`. Candidate and pending counts describe the global
eligible override owners. These are read-only diagnostics, not another source
of harmonic truth. Request on demand; no periodic polling was added.

Ordinary note release retains the committed override until its operation is
released, stopped or cleared, as before. Releasing the operation falls back to
the next eligible authority or the continuing conductor. Revoicing held notes
does not reclaim latest-intent priority. Deferred motifs still honor generation
fences when an operation is released and rearmed.

## Validation and acceptance

The new C/E inversion regression fails on the baseline and passes after the fix.
Coverage includes lower recorded bass ownership, release followed by a new
gesture, motif inversions, source bypass/fallback, read-only diagnostics and
short-buffer rejection. Surface tests reject malformed input before it can
invalidate valid note/release events in the same batch. Rhythm regressions keep
per-event modifiers, secondary harmony, chord mode and operation data.

Desktop 16-track shared-work benchmark, three alternating 10,000-block runs:

| Build | Microseconds/block |
| --- | --- |
| Baseline | 32.58, 35.32, 34.61 |
| Candidate | 36.10, 34.82, 33.50 |

The overlapping timings show no clear idle regression; they do not establish
device latency or a crackle fix. The changed capture path uses fixed storage
and adds bass comparison to its existing bounded ownership scan. No allocation,
thread, timer or per-block polling was introduced.

Use `shared-core-hardware-test-plan.md` before deployment. FM-1 USB enumeration,
stock channel mapping, physical latency and audio acceptance remain unverified.
Development candidates must not replace the stable installer automatically.
