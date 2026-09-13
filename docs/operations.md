# Operation lanes (HarmonyBus 0.2.130 / Movy hbclean.44)

Each HarmonyBus instance has four independent lanes. The Operation and
Timing / Trigger panels share one lane selector. Duplicate operations compose
in lane order. Existing sets load with all operations Off. The Movy performance row requires hbclean.44 or newer.

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
| 1 | Lane 1–4 | Lane 1–4 |
| 2 | Operation | Grid |
| 3 | Pattern | Cycle |
| 4 | Amount | Phase (grid steps) |
| 5 | Offset | Probability (%) |
| 6 | Auto On/Off | Group: Chord/Voice |
| 7 | Four-lane overview (`*` means active) | Random: Repeat/Evolve |
| 8 | Punch status (read-only) | Punch status (read-only) |

Knob touch does not activate these operations. Choose **Settings → Step Row →
STEPS / PERFORM** (open Settings with Shift+Step 2; scroll with the wheel and
change the row using knob 1). This is a global UI preference stored in prefs,
not in a track or set. Default is STEPS. In PERFORM, the row targets HarmonyBus
in MIDI FX 1 on the active track even while editing its synth or another panel:

| Step | Action | Gesture |
|---|---|---|
| 1–4 | Force operation lanes 1–4 | Hold |
| 5 | Chromatic semitone below target | Hold |
| 6 | Next scale tone above target | Hold |
| 7 | Scale above → chromatic below → target | Press to arm |
| 8 | Chromatic below → scale above → target | Press to arm |
| 9–16 | Unassigned | No action |

Opening or leaving an HB panel no longer changes the chosen mode. Loop/Session,
Shift shortcuts, track/mute selection, and dedicated step editing temporarily
retain normal behavior. The preference stays PERFORM, so the bank returns when
those contexts end. The footer shows **PERFORM Tn** for the active target track.
Without a compatible HB in MIDI FX 1, normal steps remain available and the
footer reads **STEPS / NO HB**. Switching the preference to STEPS clears holds
and enclosures immediately; old physical releases are still consumed. Releases
are captured by original track/lane and handled before modal dispatch; teardown
also resets holds and armed enclosures. LEDs: dim available, white physically held, green active or
armed. Press feedback is immediate; engine-status updates are bounded to 10 Hz.

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

Lookahead uses the existing configured signed offset and learned model, falling
back to observed harmony when prediction is unavailable. Harmony choice is
chord-wide, not independently sampled per voice. With Lookahead Off, both choices
use current harmony.

Pan sends MIDI CC10 and restores the last incoming CC10 value (default center)
when it stops applying. This is channel-wide, not independent per-note pan. The
receiving instrument must support CC10. Independent voice panning would require
an instrument/voice control mechanism.

## Validation and remaining release work

`tests/motion_test.c` exercises the production API: persistence, isolation,
stacked lanes, bypass and manual activation, note-off ownership, collisions,
gates with/without transport, pan restoration, stop, harmony choice, and recording
placement. Movy's real Schwung controller test verifies both panels, their shared
cursor, immediate dependent-value refresh, and inert knob-touch activation.

Release workflows build the ARM packages and run native and UI gates. Physical step gestures and audio on Move have not yet been verified for this release.
