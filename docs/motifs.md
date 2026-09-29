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

## Global motif rhythm

**Global → Motif Rhythm Global** (also reachable from Motifs) contains **Global Motif Rhythm** and **Global Motif Span**. Defaults are As Entered and As Entered (×1). Span offers Half and Double. The controls affect motifs across all HB tracks, including built-in cadence presets; ordinary clips and arps keep their own timing.

Rhythm choices are As Entered, Even, Long-Short, Short-Long, Accelerate and Decelerate. Alternating patterns use 3:1 or 1:3 durations. Accelerate progressively shortens steps; Decelerate lengthens them. Each side of the anchor is normalized independently to its saved total duration, then Span scales it. The anchor therefore remains at the requested arrival, and its stored position within the phrase stays meaningful. Rests participate in the timing pattern; ties remain extensions of the preceding attack. Saved motif events are never rewritten by these controls.

Automatic playback schedules the transformed rhythm. Guided tapping uses it for expected spacing and Auto Finish; Free and Grid taps retain the player's/requested-grid attack times, while using transformed note lengths. A tap phrase snapshots the global controls at its first tap; later knob changes apply to the next phrase. The first valid restored snapshot initializes these globals; loading stale settings from another track cannot undo a live global edit. Older per-track Span snapshots migrate using that first valid snapshot.

Custom rhythm capture / Tap Rhythm is not part of this version.

## Shared Render Rhythm

The former Motif Rhythm Global page is now **Render Rhythm**, reachable from
Global, Motifs, and the root menu. Global Rhythm supplies the default for all
HB conductor/follower render paths and Movy clip playback. Track Rhythm selects
Inherit, Off, or Override; Override uses Track Pattern and Track Window. Global
Window and Track Window are Beat or Bar (four beats). Motif Span remains specific
to motif playback; motifs retain their own phrase spans and fixed anchors.

For ordinary tracks, the pattern continuously warps four equal subdivisions of
the selected window. Long-Short uses 3:1:3:1 weights; Short-Long uses 1:3:1:3;
Accelerate uses 4:3:2:1; Decelerate uses 1:2:3:4. As Entered and Even leave an
ordinary track's existing timing unchanged. Even still equalizes motif steps.
Window endpoints stay fixed. Notes at the same source time share an onset.
These controls do not rewrite notes, rests, ties, intent, or recorded pressure.

Live output cannot anticipate an unknown input: negative timing offsets clamp
to zero. Positive offsets enter a bounded output queue. Each note release and
poly-pressure event follows its attack's captured delay, including after a
setting change. Conductor sensing remains immediate. Stop, rewind and overflow
cancel pending attacks and release sounding output. Receiver tracks consume the
already retimed source output without applying rhythm a second time.

The accompanying Movy integration applies the full earlier/later transform to
known clip onsets after quantize/swing and before playback performance windows.
Gates and pressure offsets retain their lengths, and transformed same-pitch
retriggers close the previous gate before opening a new one. Effective rhythm
settings are cached on edits/loads and take effect at the next clip cycle (or
clip launch), so changing a knob does not move an unfired note behind the
playhead mid-cycle. Active recording bypasses clip rhythm. The host handshake
marks clip-origin attacks as already retimed; they do not receive the live delay
again. Generated motion repeats still follow their own runtime schedule.

This version exposes Beat and Bar windows. Learned, variable-length chord-span
windows need a versioned timeline bridge and are not exposed yet. Motif arrival
at a known next chord and motif anchors continue to work. Custom rhythm capture
and physical-device latency/LED validation remain outstanding.
