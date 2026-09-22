> 0.2.169: existing Play Tools becomes Humanize / Tools with global Timing (0–30 ms), Velocity (0–30%) and Gate (0–30%), all default 0/off. Requires Movy hbclean.76 for playback processing. Recorded follower inputs receive early/late timing and gate variation; recorded conductor inputs receive velocity variation only, preserving conductor analysis. Chord attacks move together, offsets repeat across loops, and tracks vary independently. Live playing and recording are unchanged. Velocity varies recorded input attacks before the existing render-velocity gain. Timing uses the sequencer tick resolution, stays within the clip loop, and remains subject to explicit quantization and harmony-buffer rules. Play Reset and Bypass retain their previous per-track Follow Play meanings.

> 0.2.167: Chord Timing now shows the last confirmed transition position and the next learned transition position, plus their chord names. Positions are one-based bar:beat (four quarter notes per bar), relative to the conductor loop; e.g. `2:3.50` means bar 2, beat 3 plus half a beat. Lookahead does not shift these readouts. Grid Status reports Learning/Locked and the number of genuine changes, excluding a constant harmony or duplicate seed. Next stays blank until a complete cycle is learned. Observation works with lookahead off; predicting a future change requires the learned cycle. With multiple conductor clips, positions refer to their combined repeating cycle. Fixed Chord Grid/Anticipation settings are reflected in registered positions. Compatible with Movy hbclean.74; update HarmonyBus and restart to load the expanded panel.

> 0.2.152: correct the stale 0.2.137 module title and description shown in Schwung Manager. Display name is now simply Harmony Bus; the version field and DSP knob carry the release number. No musical changes from 0.2.151.

> 0.2.151: Arp Hold adds Single variants; previous Latch modes remain Overlap with their saved indices preserved. Toggle-off identifies raw input note and channel. Auto Chord replaces its duplicate Follower Scale knob with Clear on Harmony Change (default Off); committed chord changes remove released latched inputs and retain held pads. Movy hbclean.61 displays the exact retained input pads, independent of rendered pitches.

> 0.2.150: pending approaches advance once per incoming note gesture (simultaneous distinct notes within 25 ms count as one; a repeated key starts a new gesture). Buffered presses retain their own stages. Generated arp and strum notes inherit their source stage and cannot consume another stage; an arp repeats that source voicing until a new input replaces it. Touch Mode names distinguish Arm for consumable pitch triggers from Latch for continuous operations; legacy Toggle values retain their meaning. State/Punch shows Held, Latched, Off or the pending sequence. Pending/Reset shows the remaining enclosure order. Existing Movy hbclean.60 is compatible; no Movy update required.

> 0.2.149: shared ordered approach triggers across Movy knobs and step controls. Short taps arm/toggle; long holds gate immediately and release off. Above/Below tap order creates a three-gesture enclosure; re-tapping an unused modifier removes it. Global Hold Time defaults to 250 ms; operation slots offer Hold, Toggle and Tap/Hold. Pending triggers are runtime-only and reset on stop/teardown. Requires Movy hbclean.59 for timed hardware gestures.

> 0.2.148: Arp Rate adds Cycle durations to fit a complete pattern into the selected time. Ordinary rates remain per step. Up-Down counts the return path; Random counts as many independent draws as there are unique rendered notes. New Shuffle order plays each unique rendered note once per cycle, reshuffling at the next cycle or when the note pool changes. Octave range participates in the pool; shared pitches on the same channel are deduplicated. No new panels.

> 0.2.147: Latch with Off now uses normal Latch replacement: new notes replace a released gesture, overlapping held keys join it, and repeating a latched input key removes it individually. Latch Acc. with Off retains accumulation. Clear Arp occupies the former Strum Spread knob; Strum Spread moves to Auto Chord’s free slot. There is no timed grouping window: held-key overlap defines a gesture.

> 0.2.146: Arp Hold adds Latch with Off. New input keys accumulate; pressing a latched key again removes it. Releases keep the remaining pool latched. Toggle identity uses the input key and channel, before harmony mapping or transpose; a chord-producing key toggles its whole generated chord. Existing Latch behavior is unchanged.

> 0.2.145: Next Harm places the linked Follower Buffer control beside Lookahead Anti Buffer, replacing Reset Learn. Both buffer locations edit the same saved setting and display its effective value.

> 0.2.144: enabled lookahead with a nonzero anti-buffer automatically uses and displays Follower Buffer = 0 ms. The saved buffer returns when lookahead is off or the anti-buffer is zero. Applies during learning too.

> 0.2.143: direct follower MIDI owns its held-note display once input begins; auxiliary monitor snapshots cannot add silent notes or resurrect releases. Held notes refresh output-role context when the harmony changes without changing pitch. Pair with Movy hbclean.56. The reported pad G/A-sharp behavior remains unconfirmed on hardware.

> 0.2.142: Harmony Flow presents conductor notes, detected harmony, lookahead-selected harmony and transposed rendered harmony as one row. Pair with Movy hbclean.54 for simultaneous updates.

> 0.2.141: coherent follower rows, cached split mapping, None travel, clearer root readouts and a single Foll Notes page before Operations. Pair with Movy hbclean.53 for simultaneous follower display updates at up to 25 Hz.

> 0.2.140: Foll Map now includes Approach, Reset, Scale Next and Chrom Next. The separate Foll Mod page is removed; Diagnostics stays last. Movy hbclean.52 remains compatible.

> 0.2.139: group settings by musical workflow, followed by note analysis, pad appearance and diagnostics.

> 0.2.138: simplify note analysis to the global conductor page and two per-track follower path pages. Movy hbclean.52 remains compatible.

> 0.2.137: new followers default to Scale; input roles stay anchored to the follower root; transposition applies consistently across travel modes. Next Harm adds an independent Lookahead Anti Buffer (25 ms by default). Foll Trk has two note-path pages. Pair with Movy hbclean.52 for fresh-set defaults and running-transport/deleted-set lifecycle fixes. See [follower paths and timing](docs/follower-paths.md).

> 0.2.136 fixes unnecessary idle scheduler work introduced with Ratchet/Echo in 0.2.133. Empty repeat schedulers now return immediately, and empty note-owner tables are skipped. Keep Movy hbclean.50. This addresses a measured performance regression; the reported stock transport symptom still needs device confirmation.

> 0.2.136 extends Auto and cycle conditions to Clip Repeat, Reverse, Time Shift and Speed when paired with Movy hbclean.50. Manual holds override the automatic schedule. See [operation controls](docs/operations.md).

> Release 0.2.136 adds sixteen assignable operation slots per track and host-aware clip controls. Pair with Movy hbclean.50 for Settings → Step Row → STEPS / PERFORM. See [operations](docs/operations.md).










# Harmony Bus

Experimental Schwung MIDI FX for Ableton Move.

Operation controls: [sixteen lanes with shared Operation, Timing and Conditions editors](docs/operations.md).
The new operations use a global Steps / Perform step-row mode, not knob touch. The published version below remains unchanged.

Harmony Bus uses a **Conductor / Follower / Receiver** model:

- a Conductor instance infers harmony from incoming notes and publishes it to a shared in-process bus;
- Receiver instances deliver already-rendered notes on their selected channel to the next instrument, without remapping or rebroadcasting;
- Follower instances keep their own rhythm/register, but remap their pitches against the current harmony;
- follower content and travel settings control which tones are allowed and how input notes map to them;
- follower source-root policies: **Infer Input**, **Infer Notes**, and **Explicit**.

## Install with Schwung Manager

In Schwung Manager use:

**Custom → From GitHub URL → `douglasmason/harmonybus`**

Manager reads `release.json` and downloads the release asset.

## Timing and current release

HarmonyBus **0.2.129** makes Follower Buffer global with a **1/16-note** default for
fresh settings. Millisecond choices are 0, 25, 50, then 100 to 1000 in 50 ms steps. The same control also offers tempo-relative durations from 1/64
to 2 Bars. Existing saved values survive; the first restored legacy copy becomes
the shared value. Change it once and save to make all snapshots agree.

Already-on-grid notes remain on-grid even when the buffer spans a full interval.
Lookahead remains Off by default. Use Movy **0.34.1-hbclean.25** for the new explicit
Quantize + Fill Gaps clip operation and prepared 1/16-note defaults.

Read the canonical [timing guide](docs/timing-guide.md), with editable
[SVG diagrams](docs/timing). CI generates the PDF from those sources and attaches
it to each release. The PDF is an export, not a separately maintained document.
Native tests cover timing, mapping, state restoration and complete module loading;
physical Move behavior still needs device verification.

To rebuild the guide (Linux with `fonts-dejavu-core` installed):

```bash
python3 -m pip install -r scripts/docs-requirements.txt
python3 scripts/build_timing_guide.py
```

## Build

```bash
./scripts/build_harmonybus_move.sh
```

The output is `dist/harmonybus-v0.2.142-module.tar.gz`.

## License

MIT.

HB 0.2.104 adds negative lookahead (late harmony) with the capture window before the shifted boundary, and resolves recognized two-note Movy voicings before due followers. Lookahead stays Off by default; new sets use a 1/16-note global buffer. See the [timing guide](https://github.com/douglasmason/harmonybus/blob/main/docs/timing-guide.md).

### Auto Chord and Arp / Strum (0.2.129)

Per-track Auto Chord provides Off, Scale Degree and Conductor Chord, with selectable power, triad, seventh, ninth, add9, sixth, 6/9, eleventh, thirteenth and suspended forms, key-selected or explicit inversion, and Close / Root + Fifth Low / Alternate Up / Shell voicings. Arp / Strum provides Together, Repeat Arp and Once; Momentary/Latch; five orders; musical rates and gate lengths; and ms or musical strum spreads. Both generators are inactive by default. Use Movy hbclean.31 to record and replay rendered conductor chords.

Generation happens after follower-buffer release, before arpeggiation. Generated gestures keep their initial voicing until released or replaced. Chord Mode Off with an active arp uses raw input pitches, plus transpose; existing Content/Travel/Approach mapping applies when both generators are inactive. See the [canonical guide](docs/timing-guide.md) for inversion examples, control locations, latch and cancellation semantics.

Dominant Scale (Off / Harmonic Minor / Melodic Minor / Altered V) substitutes the output collection on functional V or raised-vii diminished harmony relative to the configured tonic. It applies to ordinary follower scale mapping and generated extensions, while preserving source-degree labels and recognized chord tones. The guide details triggers, altered-scale spelling and held-gesture behavior.

### Follower scale baseline (0.2.169)

An explicit Follower Scale now supplies the rendering baseline as well as the input layout. Actual chord tones replace conflicting scale degrees; unrelated notes keep the follower collection. Infer retains harmony-based parent selection. Grey pad backgrounds remain derived from actual rendered scale membership, so this corrects the rendering and its preview together. Travel None preserves pitches.

All melodic-minor modes are selectable: Melodic Minor, Dorian b2, Lydian Augmented, Lydian Dominant, Mixolydian b6, Locrian #2 and Altered. Existing scale IDs and saved presets retain their meanings. Update Movy to hbclean.77 for the added layouts and saved input roles.

Borrowed Scale fits in the existing Foll Root panel beside Dominant Scale. It is global, defaults to Minimal, and offers Aeolian, Dorian and Mixolydian b6 for major-third follower contexts encountering bIII major, iv minor, bVI major or bVII major. Actual chord tones always win. It changes the output collection, not the physical input layout. Dominant Scale retains priority, including Altered V. This release does not infer secondary ii-V progressions or add new Travel modes; the input scale remains shared.

Dominant Scale and Borrowed Scale are shared across all tracks, like Follower Scale. New presets restore one shared choice; legacy per-track presets seed it from the first non-default dominant setting. Later stale track copies cannot override an edited or restored global choice.
