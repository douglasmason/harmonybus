# Operation lanes (HarmonyBus 0.2.132 / Movy hbclean.47)

Each HarmonyBus instance has sixteen independent slots. The Operation and
Timing / Trigger panels share one lane selector. Duplicate operations compose
in lane order. Existing assignments in slots 1–4 are retained. Unassigned slots 5–12 start Off; slots 13–16 default to the four approaches below. These defaults are inactive until pressed. The sixteen-slot performance row requires hbclean.47 or newer.

## Signal path and recording

Source clip/live notes → HarmonyBus mapping and chord/arp generation → output
operation lanes → local instrument and render-channel MIDI.

Source clips are never rewritten. The existing Movy conductor recording bridge
captures generated chord/arp reference voices before the new output operations.
It does not print their octave, rotation, velocity, skip, gate, or pan changes.
Recording downstream MIDI can print the transformed notes, subject to the
recorder's support for controller messages.

Harmony selection is the exception to the post-generation position: it chooses
the harmony used by mapping and chord generation. It does not edit source MIDI
or the shared harmony bus. A generated conductor voicing recorded by the existing
bridge can consequently reflect that selected harmony.

## Panels

| Knob | Operation | Timing / Trigger |
|---|---|---|
| 1 | Lane 1–16 | Lane 1–16 |
| 2 | Operation | Grid |
| 3 | Pattern | Cycle |
| 4 | Amount | Phase (grid steps) |
| 5 | Offset | Probability (%) |
| 6 | Auto On/Off | Group: Chord/Voice |
| 7 | Selected slot summary | Random: Repeat/Evolve |
| 8 | Punch status (read-only) | Punch status (read-only) |

Knob touch does not activate these operations. Choose **Settings → Step Row →
STEPS / PERFORM** (open Settings with Shift+Step 2; scroll with the wheel and
change the row using knob 1). This is a global UI preference stored in prefs,
not in a track or set. Default is STEPS. In PERFORM, the row targets HarmonyBus
in MIDI FX 1 on the active track even while editing its synth or another panel:

| Step | Action | Gesture |
|---|---|---|
| 1–12 | Assigned operation (Off unless configured or restored) | Hold |
| 13 | Chromatic semitone below target | Hold |
| 14 | Next scale tone above target | Hold |
| 15 | Scale above → chromatic below → target | Press to arm |
| 16 | Chromatic below → scale above → target | Press to arm |

Every button maps directly to its numbered slot. All sixteen assignments can be
changed, including the last four. Approaches/enclosures are ordinary operations
and can be duplicated or moved. Enclosure slots are Trigger; clip slots are Hold
only. Auto is disabled for those choices.

Opening or leaving an HB panel no longer changes the chosen mode. Loop/Session,
Shift shortcuts, track/mute selection, and dedicated step editing temporarily
retain normal behavior. The preference stays PERFORM, so the bank returns when
those contexts end. The footer shows **PERFORM Tn** for the active target track.
Without a compatible HB in MIDI FX 1, normal steps remain available and the
footer reads **STEPS / NO HB**. Switching the preference to STEPS clears holds
and enclosures immediately; old physical releases are still consumed. Releases
are captured by original track/lane and handled before modal dispatch; teardown
also resets holds and armed enclosures. LEDs: green for note operations (live and HB-routed playback), royal blue for
clip-only operations, unlit for Off. Idle assignments are dim; held, automatically
active or armed operations use their brighter category color. Press feedback is immediate; engine-status updates are bounded to 10 Hz.

Enclosures advance on the next three note/chord onsets; they do not generate
notes. Release of the trigger does not cancel the sequence. Retrigger starts
again. Distinct live pitches within 25 ms share a chord onset; a repeated pitch
always advances, and generated arpeggios use their output onset rather than this
live grouping window. Each target is calculated using the current rendering at
that onset, without changing source clip notes. After the target, the enclosure
disarms. Both momentary pitch buttons can be held: the latest wins, and releasing
it falls back to the still-held button. A pitch hold cancels a pending enclosure.
An enclosure armed during a pitch hold waits for the hold to end.

Runtime performance state is not saved. Holding a lane forces activation even
with Auto Off, probability zero, or motion bypass; pattern still determines its
value. Release restores automatic behavior. Notes already sounded keep their
pitch and matching note-off; releases do not retune sustained notes. Stop clears
all holds and enclosures.

## Values

The value is `offset + amount × pattern`. Choosing a different operation
initializes its amount and clears offset. Patterns: Constant, Alternate, Rise,
Fall, Triangle, Backbeat (quarter-note beats 2 and 4 in a four-beat cycle), Random.
Patterns are sampled on the grid. Repeat gives deterministic repeating randomness;
Evolve changes the seed each cycle. Probability gates each operation; only Skip
explicitly suppresses notes.

| Operation | Meaning | Initial amount |
|---|---|---|
| Velocity | Percentage change from incoming velocity, clamped to 1–127 | +25 |
| Pan | Offset from center: −100 left to +100 right | +50 |
| Octave | Rounded octave steps, limited to ±4 per lane | +1 |
| Rotate | Steps through the follower content collection, limited to ±24 | +1 |
| Gate | Maximum duration as a percentage of the grid; source release can end sooner | 50 |
| Skip | Suppress the note when the value is positive | 100 |
| Harmony | Below 50: current; 50 or above: lookahead | 100 |
| Transpose | Rounded semitone shift; emitted MIDI pitch stays within 0–127 | +1 |
| Chrom Below | Chromatic semitone below the rendered target | 1 |
| Scale Above | Next scale tone above the rendered target | 1 |
| Enclose Above Below | Above → below → target on three onsets | 1 |
| Enclose Below Above | Below → above → target on three onsets | 1 |

Lookahead uses the existing configured signed offset and learned model, falling
back to observed harmony when prediction is unavailable. Harmony choice is
chord-wide, not independently sampled per voice. With Lookahead Off, both choices
use current harmony.

Pan sends MIDI CC10 and restores the last incoming CC10 value (default center)
when it stops applying. This is channel-wide, not independent per-note pan. The
receiving instrument must support CC10. Independent voice panning would require
an instrument/voice control mechanism.

## Clip playback operations (Movy only)

Note operations work on live input and recorded notes that pass through HB.
Clip operations need an existing playing clip. Movy changes which stored notes
are emitted while its normal transport keeps moving; releasing the operation
returns playback to that transport position. They never rewrite the clip and
are bypassed on the track currently recording. Live pads are unaffected.

| Operation | Controls and behavior |
|---|---|
| Clip Repeat | Grid selects the repeated section length, anchored to the section containing the press. Starts at that section's beginning. |
| Clip Reverse | Mirrors note intervals within the clip loop, preserving their gates. |
| Clip Time Shift | Amount is a signed number of Grid steps ahead (+) or behind (−) normal playback. Wraps within the loop. |
| Clip Speed | Amount +2/+3/+4 means 2×/3×/4×; −2/−3/−4 means 1/2×, 1/3×, 1/4×. Zero and ±1 mean normal speed. Starts at the press position. |

Only Grid and Amount apply as described above. Clip operations do not use
Pattern, Offset, Cycle, Phase, Probability, Group or Random in this version.
With several clip operations held, the most recently pressed wins. Releasing it
restores an earlier held operation; its elapsed time continues in the background.
Stop, clip launch/change, set load, leaving Perform mode and teardown clear clip
gestures. Note-offs remain paired with emitted pitches; existing gates finish
normally on release. Speed scales new gates and pressure timing. Parameter
automation remains on the normal clip timeline.

Movy advertises clip support at runtime. Standalone HB in Schwung offers note
operations only. Loading a preset with a clip assignment preserves that slot,
shows **Requires Movy**, and bypasses it. Its selected operation stays visible so
the saved assignment can be understood or changed; other clip choices are hidden.
Host capability, physical holds and triggered enclosure progress are not saved.
The existing HB state format and first four assignments remain compatible.

## Validation and remaining release work

`tests/motion_test.c` exercises the production API: persistence, isolation,
stacked lanes, bypass and manual activation, note-off ownership, collisions,
gates with/without transport, pan restoration, stop, harmony choice, and recording
placement. Movy's real Schwung controller test verifies both panels, their shared
cursor, immediate dependent-value refresh, and inert knob-touch activation.

Release workflows build the ARM packages and run native and UI gates. This release still needs the on-device check after installation. Earlier eight-button approach/enclosure gestures were confirmed on Move; the expanded assignment row and clip operations are new.
