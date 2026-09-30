**FIFO Approach / Perform 2 (HarmonyBus 0.2.216, Movy hbclean.125):** Copy cycles Steps, Perform 1 and Perform 2. Perform 2 knobs and step buttons share the same 16-slot operation/motif bank on every layout. Each new touch enters a persistent three-item FIFO, without requiring overlapping touches. Triple Approach plays the queue from the top row down: row 3 (oldest), row 2, row 1 (newest), then the scale target. A fourth touch evicts the oldest assignment. Each row advances its own assigned motif; single Approach uses the latest touch. The display mirrors rows 3–2–1, highlights the newest assignment, and retains a compact slot summary. Tap/hold/latch performance and overlapping performance sequences remain independent of the row FIFO. Existing bank assignments are preserved; the eight triple motifs remain optional library choices.

> **0.2.214 / Movy hbclean.123:** Secondary II consolidates Scale Above. Secondary LT consolidates Chromatic Below and remains a semitone below the target; Auto Chord uses the Chromatic Chord setting. Secondary VII follows the effective scale’s seventh degree and can differ from LT. Legacy saved assignments and recorded note intent remain compatible. Pitch Play and Secondary panels now access the same Secondary II lane.

> **0.2.213 / Movy hbclean.122:** Copy cycles Steps → Perform → Approach. Approach has a separate motif bank on steps 1–16 and ordered transformation/motif selection on knobs 1–8. Turn knobs to assign, overlap touches to sequence, then play approach pads to advance.

> **0.2.212 / Movy hbclean.121:** Fixed operation knobs: clockwise latches on, counterclockwise off, tap arms one use, hold is momentary. Rendered-only notes blend a faint play color into the pad background; matching source and output use the solid play color.

> **0.2.210:** Track source input and sounding output separately. Pad playback now follows final emitted notes from live/recorded Auto Chord, arps, motifs and motion echoes, mapped back through each pad’s single-note rendering. Requires Movy hbclean.120 for output highlights; held pads retain immediate feedback. Includes the 0.2.209 processing optimization.

> **0.2.209:** Reduce synchronous pad-preview and note-render work by reading operation tags without repeatedly copying full motion settings. Keep recorded overrides and active motion behavior intact. Pair with Movy hbclean.119 or newer for the separate pad LED delivery fix.

> **0.2.208:** Inversion now calls Top Note **Played Top Note**, clarifying that the played melody anchors the top of the generated voicing. Includes the follower pad-color fix and Shuffle / Shuffle Cycle Pin choices.

> **0.2.207:** Arp Order offers Shuffle (honor Arp Start on launch) and Shuffle Cycle Pin (also pin every cycle). Both follow Arp Start and visit each available note once per cycle.

> **0.2.206:** Follower Auto Chord pad colors reflect the single note rendered with Auto Chord off, including conductor-harmony mode and Top Note. Expanded voicings still determine equivalent-output groups.

> **0.2.205 / Movy hbclean.116:** Stopped startup harmony preview, coherent pad updates and protected Perform lights. Four defaults panels become one Role Defaults editor; Track Scales gains Local Palette and Foll Map gains Current / Next Harmony Target. See [panel controls](docs/panel-controls.md).

> **0.2.204 / Movy hbclean.115:** Shared Track / Lane Edit Target on Chords and Arp / Strum, temporary Chord/Arp State operations, burst presets and explicit Root/Bass + Top input. Start Note defaults to Played / Pad; Start Timing defaults to First Note Free. See [state editing](docs/chord-states.md).

> **0.2.203:** Adds complete-page live snapshots for Next Harm, Chord Timing and Follower Root so Movy can refresh position and harmony fields together. Includes the three context scopes introduced in 0.2.201. Pair with Movy hbclean.113 for the display refresh fix.

> **0.2.201:** Context Scope now offers Current Harm, Current + Next, and Full Loop on Track Scales and both role-default scale panels. Current Harm uses chord-local recipes without progression inference; Current + Next recognizes adjacent-pair resolutions; Full Loop additionally uses preceding/following chords. Explicit recorded intent remains authoritative at every scope. Full Loop remains the factory default.

> 0.2.199: Mixed cadence panels, conductor/follower chord defaults, visible track overrides, strict/automatic local gap scales and recorded harmonic intent. See [operations](docs/operations.md).

> **0.2.198 / Movy hbclean.106:** Complete Secondary II–VII controls; Parent Scale, Simple Chord, and Simple Scale policies on II/III/IV/VI/VII; separate Chrom Above connector and Tritone V. Parent Scale II follows the actual next scale degree. Older recorded approaches retain their original behavior.

> 0.2.197 / Movy hbclean.105: Pitch Play and Pitch Cadences have dedicated named controls; Ops 1–8 and 9–16 are fixed to the sixteen Step Seq slots. Added three-press ii–V–target cadences, Chrom Above (tritone-sub dominant in chord mode), and Tritone II. Install both modules for named-control recording.

> 0.2.196 / Movy hbclean.104: Backdoor II/V operations, relative cadences over chromatic approach pads, destination-relative dominant colors, Whole Tone/Augmented input scales, and expanded altered/extended chord recognition. Existing lane defaults are unchanged.

> 0.2.195: Tap to arm, hold momentarily, double-tap for persistence on operation knobs and steps. Persistent activation pulses smoothly; armed/held lanes stay solid. One Next Harmony control replaces Next Once / Next Latch; lane 6 is available.

> 0.2.190: Chromatic approach pads generate the selected chromatic chord family on their rendered root, even when that root is diatonic. Fresh instances default to Dim / Dim7; saved settings remain intact.

> 0.2.189: Inversion → Top Note anchors a generated chord to the rendered played melody, keeping Chord Mode and spacing independent without another knob.

> 0.2.188: Bulk operation-editor snapshots remove repeated synchronous page reads in Movy hbclean.96, preserving old-lane pending edits and coherent dependent menus.

> 0.2.187: Auto Chord Repeat operation temporarily enables repeated chord playback using existing panel settings; tap latches, hold is momentary. Turning it off restores the saved mode. Tonal/parallel-harmony tests and guide updates are included.

> 0.2.186: Piano approach gaps report their actual scale/current/next harmony membership for Movy hbclean.94 coloring. Remove redundant Direct travel from the selector; saved Direct sounds are preserved as None.

> 0.2.179: supports Movy hbclean.87 piano-gap approaches in Closest Split Chromatic. Each gap approaches the effective mapped output of its lower pad, with independent note ownership and recorded identity. Other travel modes leave gaps silent. Pad coloring rules are unchanged. Update both modules.

> 0.2.185: Lookahead now groups Off, Immediate, After, Before and Late in the existing selector. After measures from the current chord start and resets at each chord change; old saved offsets retain their meanings. Both Full Lookahead keeps current/next pad interpretations visible independently of the rendering switch. The timing guide explains this look-back reference. Compatible with Movy hbclean.91.

> 0.2.184: Lookahead now includes Immediate. It always renders against the next known harmony and advances at each actual chord boundary, including loop wrap. Anti Buffer does not delay Immediate. Off remains the default; saved timed choices keep their meaning. Works with Movy hbclean.91.

> 0.2.183: Follow Map keeps the assignable lane-5 touch knob and adds a plain lane-6 Off/On toggle. Movy hbclean.91 labels these Next Once / Next Latch; touching the toggle only shows its description. Descriptions follow operation edits. Older assignments remain intact.

> 0.2.181: Follow Map adds an assignable Map Touch knob, defaulting to lane 5 (Harmony → Next, Auto Off). Lane 5 advances all follower pads until the next chord change; lane 6 stays latched across changes. Auto Off on the existing Conditions panel selects Normal, Chord Change or Manual for harmony/approach operations. Colors follow the selected mapping. All nine operation-knob assignments are global across tracks. Pair with Movy hbclean.89.

> 0.2.180: Chromatic is an independent follower setting, On by default, supporting Relative, Closest and the other Travel modes. Follow Touch has eight assignable lane selectors (1, 2, 3, 4, 13, 14, 15, 16): turn to select, tap to trigger, hold for momentary operation. New factory lanes 15/16 are Scale Above/Chrom Below; touch order determines the pending enclosure. Saved lane assignments and legacy travel sounds are preserved. Pair with Movy hbclean.88.

> 0.2.178: Current and Full Lookahead colors advance together on the learned boundary, including Lookahead Off while live chord detection settles. Current colors use their own follower mapping instead of the early effective mapping. MIDI timing and saved settings are unchanged; no Movy update required.

> 0.2.177: Lookahead pad colors now preview the upcoming chord’s follower mapping, so highlighted input keys land on chord tones when it arrives. Defaults: Both Full Lookahead, pulse rate 1/4, pulse shape None, Current Yellow, Play Green, Lookahead Red, Both Orange, Input Tonic Grey. Existing saved settings are preserved. Works with Movy hbclean.85; no Movy update required.

> 0.2.176: Pads Global adds Play Color for live, recorded and retained input highlights, including Off. Input Tonic defaults to Grey; Play Color defaults to Track. Effective is the default; redundant Standard is removed. Effective shares Current Color. Requires Movy hbclean.82 for configurable playback LEDs.

> 0.2.175: complete a learned conductor-clip traversal only after the wrap boundary's MIDI batch has been processed. A restart that misses the already-sounding opening chord no longer locks an incomplete schedule, invalidates it at the wrap, and waits through a second pass; deterministic clips lock after their first complete traversal.

> 0.2.174: learned next-harmony knowledge is independent of the Lookahead time. At Lookahead Off, Full Lookahead pad modes, Next Harmony, chord-transition diagnostics and explicit Next harmony operations remain available; ordinary effective harmony and note rendering stay current and are not shifted early.

> 0.2.173: Chords adds Shell 7, Shell 9, Shell 6/9, Rootless 7 and Rootless 9. Voicing omissions retain semantic chord tones for followers and pads. Form, quality and voicing edits apply at the next input onset. Chord Form joins existing Operations, including cycle conditions and recorded outcomes (Movy hbclean.79). Learned conductor timelines are now retained independently and recomposed at actual launch, including full deterministic form-operation cycles. New clips/settings still need observation; unplayed source scanning and whole-event reshaping of old baked captures are not included.

> 0.2.172: restarting the same cached clip resets its launch boundary, including clips that begin with silence.

> 0.2.171: Pause/Stop preserves raw/baked note-offs before clearing pitch ownership, preventing stuck local and routed voices. retains up to 32 learned clip/playing-conductor configurations across launches, with phase alignment and eviction on confirmed divergence, missing transitions, or edited clip contents. Requires Movy hbclean.78 for explicit slot identity. Cache is session-local and learns a new or changed rendering configuration on its first full pass. Simultaneous conductor combinations are retained as combinations; queued future launches are not predicted before activation. Tempo is available in Humanize / Tools, using the shared Schwung tempo request protocol (20–300 BPM). External Link peers can own tempo. Movy .78 additionally records raw Auto Chord gestures and marks the selected track button independently of note activity.

> 0.2.170: existing Play Tools becomes Humanize / Tools with global Timing (0–30 ms), Velocity (0–30%) and Gate (0–30%), all default 0/off. Requires Movy hbclean.76 for playback processing. Recorded follower inputs receive early/late timing and gate variation; recorded conductor inputs receive velocity variation only, preserving conductor analysis. Chord attacks move together, offsets repeat across loops, and tracks vary independently. Live playing and recording are unchanged. Velocity varies recorded input attacks before the existing render-velocity gain. Timing uses the sequencer tick resolution, stays within the clip loop, and remains subject to explicit quantization and harmony-buffer rules. Play Reset and Bypass retain their previous per-track Follow Play meanings.

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

### Follower scale baseline (0.2.170)

An explicit Follower Scale now supplies the rendering baseline as well as the input layout. Actual chord tones replace conflicting scale degrees; unrelated notes keep the follower collection. Infer retains harmony-based parent selection. Grey pad backgrounds remain derived from actual rendered scale membership, so this corrects the rendering and its preview together. Travel None preserves pitches.

All melodic-minor modes are selectable: Melodic Minor, Dorian b2, Lydian Augmented, Lydian Dominant, Mixolydian b6, Locrian #2 and Altered. Existing scale IDs and saved presets retain their meanings. Update Movy to hbclean.77 for the added layouts and saved input roles.

Borrowed Scale fits in the existing Foll Root panel beside Dominant Scale. It is global, defaults to Minimal, and offers Aeolian, Dorian and Mixolydian b6 for major-third follower contexts encountering bIII major, iv minor, bVI major or bVII major. Actual chord tones always win. It changes the output collection, not the physical input layout. Dominant Scale retains priority, including Altered V. This release does not infer secondary ii-V progressions or add new Travel modes; the input scale remains shared.

Dominant Scale and Borrowed Scale are shared across all tracks, like Follower Scale. New presets restore one shared choice; legacy per-track presets seed it from the first non-default dominant setting. Later stale track copies cannot override an edited or restored global choice.

### Progression-based Infer (0.2.170)

Infer now uses confirmed conductor transitions during learning and the full registered loop once learned. Each harmony contributes equally to scale membership scoring across all fifteen scales around the existing follower reference root. Ties retain the previous best choice; no evidence defaults to Major. Input layout, rendering and Used Scale share this baseline, independently of lookahead position. Explicit scales still take precedence, and chord alterations plus global borrowing/dominant overrides still apply to rendering.

The existing Foll Root panel's final slot is now Used Scale. A `?` marks equally scoring alternatives; `(default)` means no harmony evidence yet. `--` means the follower reference root is unresolved. Inferred Root and Used Root keep their root-policy meanings. This estimates a collection, not tonal function or modulation: it does not infer the tonic from a progression, inspect unplayed raw clip notes, or automatically identify secondary ii-Vs. The learned progression is available after playback observation, even with the lookahead offset off.

### Parallel-harmony regression coverage

`tests/parallel_harmony_test.c` compares Dm11–G9–Cmaj9–Am11 with parallel minor-11 and major-9 loops. It checks chord preservation, changing output collections, learned-loop lookahead, pad membership and chromatic approach mapping through the production mapper. The [timing guide](docs/timing-guide.md) explains the results and the distinction between parent-scale accommodation and independent per-chord modes. These tests do not add automatic per-chord mode selection.

0.2.203: Play Motif uses the existing performance lanes; user library edits are shared, stock edits create copies, and the lane editor allocates recordings automatically. Fixes the guide version gate for the pending display snapshot release.
