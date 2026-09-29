# Anchored motifs — feature candidate

Motifs live in the HB Motifs panel and share a 16-slot library. Each track selects its own target, arrival grid and late policy. This candidate is implemented locally; Move DSP builds and on-device timing/LED checks are still pending.

## Entering and editing

1. Select a slot, then touch **Edit** to latch motif entry. Step buttons select phrase positions while editing, and slots while outside editing on the Motifs page.
2. Play notes at any pace. Overlapping held notes form one chord; releasing the final note advances. The selected duration controls musical time, not how long you hold a pad.
3. Right with notes held adds a tie; left removes a tie. Right without held notes inserts a rest/advances. Left moves back.
4. Select a note step and touch **Anchor**, or arm Anchor before entering the target note. Without an explicit anchor, the last note becomes the anchor. Start and middle anchors retain the following tail.
5. Touch **Done**, or press Record, to save and leave motif entry. Cancel discards the draft. Existing song clips are never the motif recording destination. Selecting an occupied step preserves it until replacement notes are entered, allowing anchor selection without erasing notes.

Editing a full bank can report **Motif bank full**. The saved library stays intact and the draft stays available. Switching tracks closes successful edits; a capacity-error draft remains on its original track and can be resumed with Edit.

## Playing

On the Motifs page, tap a populated slot or touch the slot/Arm control, then play a target pad. That pad is consumed as the phrase target. The phrase resolves against the harmony expected at its anchor time when HB has learned the future harmony.

Targets: Played, Root, Third, Fifth, Seventh, Sus2, Sus4. A missing chord member reports Target unavailable. Next Harmony requires known future harmony. Arrival grids include Now, Next Beat, Next Bar, 2 Bars and 4 Bars; bar lengths use HB's existing four-beat convention.

- **Trim:** omit attacks that have already passed; preserve the anchor and tail.
- **Fit:** compress the approach into available time, dropping approach steps below a 1/64-note spacing floor.
- **Defer:** use a later grid boundary that accommodates the approach. Next Harmony currently asks for a bar grid when deferral is needed.

Auto relationships use captured scale steps for in-scale material and semitone offsets for chromatic material. Scale and Chromatic modes override this. Harmonic approach intent and chord configuration reuse HB's capture and chord-approach resolver. Exact operation/provenance words are saved, but this version does not replay every nonharmonic effect or chord arpeggiation/strumming behavior.

## Feedback and limits

Cyan marks the anchor, purple marks operations, magenta marks ties, and dim grey marks rests. A dim white hardware pulse identifies the entry cursor. A short cyan confirmation wave spreads over nearby pitches and steps; it ends after roughly 420 ms, updates at most 20 times per second, and shares Movy's existing changed-LED cache and frame budget. Visual frames may be skipped; no musical event waits for the splash. Actual hardware smoothness and contention still require measurement on Move.

Limits: 32 events and eight input notes per event; 128 events across the library, additionally bounded by a 4 KB encoded library budget within the host's state limit. Expanded playback has a bounded 256-voice schedule per HB instance. Stop/rewind releases generated voices. This version has one anchor per motif and no automatic phrase-chain editor.

## Local validation

Native motif tests with UndefinedBehaviorSanitizer cover untimed entry, rest/tie gestures, anchors, transposition, harmonic modifiers, Trim/Fit/Defer, stop/cancel, exact operation snapshot persistence, truncated state, and bank capacity. Existing chord, cadence, operation-recording, input-recording, role-scale and stop-release suites pass. Runtime metadata validation passes.

Movy's integration applies to a clean pinned upstream checkout. TypeScript checks, browser and device-JavaScript builds, the original step-recorder suite, and motif edit-isolation/LED-budget checks pass. ARM DSP cross-compilation and physical-device verification remain pending. Candidate workflows build artifacts without publishing a release.

## Arp Start and pressure follow-up

Arp / Strum replaces Clear Arp with **Arp Start**: Order, Lowest / Pad, Highest / Pad, Lowest / Chord, Highest / Chord, Played / Pad, Played / Chord. Played selects the most recently played mapped pitch within the voiced pool (nearest voice if its pitch class is absent). Order preserves the previous behavior. Pad choices restart the pitch order on the next scheduled attack after an accepted input note; Chord choices restart it after the effective rendering harmony changes. Neither choice moves the running arp clock. Free and First Note Free retain their immediate opening attacks, while Auto retains its grid start. The selected extreme is drawn from the actual voiced/octave-expanded pool, and repeats at each order-cycle start. Shuffle visits every voice once, with the extreme first; Random excludes an immediate repeat of the forced opening note.

Clear Arp remains in **Humanize / Tools**, allowing deliberate latch clearing without stopping transport. Full Velocity also governs HB's conversion of live/replayed poly pressure into arp attack velocity; other pressure destinations keep raw pressure.

## Tap playback and built-in cadence motifs

Recording remains untimed step entry through Movy's existing step-recorder gestures. Playback is independent of recording:

- **Automatic** schedules the entire phrase around Arrival.
- **Tap Free** plays the next step at each press. The player determines when the anchor lands; Arrival does not impose a deadline.
- **Tap Guided** plays immediately on each press and shows the next step, stored anchor and beats remaining to Arrival.
- **Tap Grid** queues each pressed step on the next selected grid division. Rapid presses occupy successive divisions rather than bunching into one attack.

Arm once, then repeatedly play the same target pad. The target and harmonic context are fixed on the first tap for that phrase; another target pad restarts the phrase for that target. Notes already emitted retain their scheduled releases. Rests consume a tap; ties extend the previous note and do not require a new attack. Completion disarms the phrase. Cancel and transport stop cancel remaining taps and scheduled voices.

**Motif Play** contains Preset, Copy to Slot, Tap Grid, Completion, Late, Span, Arm and Status. The default Completion is **Manual**: passing the intended anchor does not complete the phrase. **Auto Finish**, applicable to Guided/Grid, schedules the remainder when the next expected step becomes due, using the existing late-arrival policy. Free mode always remains manual. The step row shows the next step and cyan anchor during tap playback; the header shows timing guidance.

Built-in presets include V–Target, ii–V–Target, backdoor iv–bVII–Target, tritone variants and all fourteen existing mixed cadence programs. These are end-anchored motif steps referencing the existing stable harmonic intent IDs. They render as melody or chords using the current chord mode. Existing cadence operation controls and recorded IDs remain compatible; new automatic/tap playback uses the same bounded motif renderer. **Copy to Slot** opens an editable draft in the selected library slot; Done commits it and Cancel retains the previous saved slot.
