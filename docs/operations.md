# Operation lanes (HarmonyBus 0.2.196 / Movy hbclean.104)

Each HarmonyBus instance has sixteen independent slots. The Operation,
Timing / Trigger, and Conditions panels share one lane selector. Duplicate operations compose
in lane order. Existing assignments in slots 1–4 are retained. Lane 5 selects Next Harmony; unassigned slots 6–12 start Off. Slots 13–16 default to the four approaches below. These defaults are inactive until pressed. The sixteen-slot performance row requires hbclean.47 or newer; the Conditions editor uses hbclean.50 or newer.


## Backdoor operations and chromatic destinations

Backdoor II and Backdoor V are choices in the existing Operation selector. Assign them to any free lanes (6–12 start Off). The factory 13–16 assignments and Follow Touch knob bindings stay unchanged. They use the same tap/hold/double-tap gestures and activation lights as other approaches. Touch order selects sequence order; the upper slot accepts Scale Above, Secondary II or Backdoor II, while the lower slot accepts Chrom Below, Secondary V or Backdoor V. Secondary VI can precede either pair.

| Operation | Note with Auto Chord off | Chord with Auto Chord on |
|---|---|---|
| Backdoor II | Perfect fourth above the resolved target | Minor triad/seventh (iv relative to the target) |
| Backdoor V | Whole step below the resolved target | Dominant triad/seventh (bVII relative to the target) |

Chord Form chooses triad, seventh, ninth, etc.; these operations never turn Auto Chord on. Backdoor II/V use the parallel borrowed family for extensions (Aeolian under Minimal; otherwise the selected Borrowed Scale). Backdoor V can apply Dominant Scale relative to its functional destination, bIII of the main target.

The order is **pad/travel target → relative operation → chord voicing**. A chromatic approach pad is a usable destination in its own right. In C major with None travel and Chromatic On, the Eb pad approaches E. Arm Secondary II then Secondary V and play that Eb pad repeatedly: Fm7 → Bb7 → Ebdim7 with Seventh form and the default chromatic quality. Then play the regular E pad to resolve. The Eb arrival retains the chromatic pad's configured quality; it does not silently become Ebmaj7. With Auto Chord off the same inputs produce F → Bb → Eb, then E.

An in-scale destination retains the effective parent collection. Out-of-scale bIII/bVI/bVII destinations use the selected parallel borrowed family; Minimal uses Aeolian to establish that missing destination. Other out-of-scale roots use a local major destination as the fallback. This is destination context, separate from the quality played on the chromatic landing. Existing tap-order sequences remain sequences; pressing Chrom Below alongside a secondary operation does not create an arbitrary nested operation tree.

**Dominant Scale colors the output collection.** It leaves the input scale and actual chord-defining tones intact. Harmonic Minor and Melodic Minor refer to the destination tonic; Altered V uses the melodic-minor collection a semitone above the dominant root. Leading-tone chords retain the existing harmonic-minor interpretation of Altered V. This also applies to generated secondary dominants and leading-tone chords: G7 keeps G–B–D–F while a Ninth form can use Ab under Harmonic Minor/Altered V. It does not globally switch the destination chord to minor.

## Chord-family coverage and symmetric scales

The classifier now has exact-evidence names for 7#5, maj7#5, 9#5, maj9#5, 7b5, 9b5, minMaj9, 7sus4, 9sus4, 7sus2, maj7b5, min9b5, 7b9, 7#9, 7#5b9 and 7#5#9. Their masks, display names and Auto voicings preserve those tones. These new families require the full pitch-class evidence during free recognition; an incomplete third/seventh shell continues to imply the ordinary fifth. Root-established recognition handles inversions, but pitch-set ambiguity still exists: a symmetric or enharmonically identical set cannot uniquely identify its intended root without context.

MinMaj7, AugMaj7 and Dom7b5 are additional Quality choices in the existing Chords panel. Auto remains scale-derived. Aug retains its earlier meaning (augmented triad, flat seventh when Seventh form is requested); AugMaj7 explicitly selects the major seventh.

Whole Tone (0,2,4,6,8,10) and Augmented (0,3,4,7,8,11) are explicit Follower Scale choices and synchronize with Movy's keyboard selector. Existing scale IDs are preserved. Infer retains its existing seven-note candidate families rather than guessing a symmetric parent from ambiguous partial evidence.

Whole Tone builds augmented triads and augmented dominant sevenths. Augmented builds augmented triads; where a major seventh is present, Seventh form adds it. On the other three roots that six-note collection contains neither a minor nor a major seventh, so Auto omits the unavailable seventh instead of inventing an out-of-scale tone. A forced Quality can supply one deliberately. C melodic minor's third degree produces Eb–G–B or Eb–G–B–D (Ebaug / Ebmaj7#5).

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
| 5 | Offset (Echo: Decay %) | Probability (%) |
| 6 | Auto On/Off | Group: Chord/Voice |
| 7 | Touch Hold threshold (global) | Random: Repeat/Evolve |
| 8 | Punch status (read-only) | Advance: Clock / Note / Chord |

Knob touch does not activate these operations. Choose **Settings → Step Row →
STEPS / PERFORM** (open Settings with Shift+Step 2; scroll with the wheel and
change the row using knob 1). This is a global UI preference stored in prefs,
not in a track or set. Default is STEPS. In PERFORM, the row targets HarmonyBus
in MIDI FX 1 on the active track even while editing its synth or another panel:

| Step | Action | Gesture |
|---|---|---|
| 1–12 | Assigned operation (lane 5: Next Harmony; 6–12: Off) | Tap / hold / double-tap |
| 13 | Secondary VI | Tap to arm; hold momentarily; double-tap persistently |
| 14 | Next scale tone above target | Tap to arm; hold momentarily; double-tap persistently |
| 15 | Secondary V | Tap to arm; hold momentarily; double-tap persistently |
| 16 | Chromatic semitone below target | Tap to arm; hold momentarily; double-tap persistently |

Every button maps directly to its numbered slot. All sixteen assignments can be
changed, including the last four. Approaches/enclosures are ordinary operations
and can be duplicated or moved. Enclosure slots are Trigger and have Auto disabled. Clip slots support Auto
on Movy hbclean.50 or newer, alongside their direct manual holds.

Opening or leaving an HB panel no longer changes the chosen mode. Loop/Session,
Shift shortcuts, track/mute selection, and dedicated step editing temporarily
retain normal behavior. The preference stays PERFORM, so the bank returns when
those contexts end. The footer shows **PERFORM Tn** for the active target track.
Without a compatible HB in MIDI FX 1, normal steps remain available and the
footer reads **STEPS / NO HB**. Switching the preference to STEPS clears holds
and enclosures immediately; old physical releases are still consumed. Releases
are captured by original track/lane and handled before modal dispatch; teardown
also resets holds and armed enclosures. LEDs: green for note operations (live and HB-routed playback), royal blue for
clip-only operations, unlit for Off. Inactive lanes are unlit. Armed triggers and momentary holds are solid; persistent double-tap latches pulse smoothly. Press feedback is immediate; modern engine-status reads are cached for 50 ms.

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
| Ratchet | Total attacks (1–16) within one Grid duration | 4 |
| MIDI Echo | Additional repeats (0–16), spaced by Grid; Decay reduces velocity per repeat | 3 |
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

## Cycle conditions

The **Conditions** page automates Auto-capable note operations on selected
transport cycles. Each lane has its own condition; duplicate operations can
use different ranges. Cycle counting starts at transport beat zero. Seeking or
restarting the transport updates the cycle position immediately; punching a
button never restarts it. Lanes with the same Cycle length stay aligned even
when their instances were created at different times.

| Knob | Control | Meaning |
|---|---|---|
| 1 | Lane | Shared slot selector across all three editors |
| 2 | Every | Phrase length in cycles, 1–16 |
| 3 | From | First eligible cycle, inclusive |
| 4 | Through | Last eligible cycle, inclusive |
| 5 | Cycle | Length of one cycle; same control as Timing / Trigger |
| 6 | Auto | Enable automatic operation; same control as Operation |
| 7 | Cycle Range | Read-only summary, such as `4 of 4` or `7-8 of 8` |
| 8 | Cycle Status | Read-only position and state, such as `3/4 Waiting` |

**Every 4, From 4, Through 4, Cycle 1 Bar, Auto On** applies the selected
operation during the last bar of each four-bar phrase. **Every 8, From 7,
Through 8** covers the last two cycles. Every 1 is the default and preserves
unrestricted operation. Increasing From past Through moves Through with it;
decreasing Through below From moves From with it. Reducing Every clamps both
endpoints. Movy refreshes the native option lists immediately. Standalone
Schwung keeps the full 1–16 lists available; endpoint writes still clamp to Every.

Conditions are checked before Probability and Pattern. Pattern Phase and
Clock/Note/Chord advancement do not shift the condition window: conditions
always use transport time. A ratchet or echo begins only from notes emitted in
an eligible cycle. Buffered follower notes use their output time for this test;
their captured input pattern position remains unchanged.

Holding a step overrides the condition, probability, Auto Off and bypass.
Release returns to automatic scheduling and cancels that press's generated
repeats. The cycle readout shows Held during the override. Idle assignments are
dim green and eligible automatic lanes brighten; Ready means eligible, not that
a note or probability trial has fired. Auto Off, Bypassed, Prob 0 and Waiting are
shown explicitly. No new polling loop is added; LEDs use the existing 10 Hz status
read. Notes retain their matching note-offs across a window boundary; automatic
echo/ratchet tails already started can finish. Pan returns to its base value when
its condition window closes. Stop clears scheduled repeats and holds.

While the transport is stopped, conditional lanes (Every greater than 1) wait
and show Stopped; a manual hold still works. Every 1 preserves ordinary live
operation without transport. Conditions are saved with each lane. Older states
default to Every 1, and old readers can ignore the new state suffix.

Clip operations support the same Auto and cycle conditions with Movy
hbclean.50 or newer. Enclosures remain **Trigger**. Standalone Schwung continues
to show Requires Movy for preserved clip slots; older Movy hosts show Update Movy
and keep manual clip gestures available.

## Pattern advancement and generated repeats

**Advance** lives on Timing / Trigger knob 8 and opens the native option list.
Clock samples the transport grid (or the running internal clock while stopped).
Note advances once per accepted incoming note-on. Chord advances once per group
of distinct incoming pitches within 25 ms; a repeated pitch starts a new group.
The first event uses step zero. Grid and Cycle determine the number of steps;
Phase offsets the position. These counters advance even when a slot is inactive,
and reset on Stop, state load, performance reset or changing its Advance mode.
Generated chord voices, arpeggio output, ratchets and echoes do not increment the
input counter. Use Clock when an arpeggio should change the pattern over time.

Ratchet and MIDI Echo work on live input and recorded MIDI passing through HB,
in standalone Schwung as well as Movy. Their step buttons are green. Choose the
operation, set Auto Off for punch-in use, then hold its assigned step while playing.
Ratchet's Amount includes the original attack; Echo's Amount counts extra attacks.
Repeats use the final mapped pitch and velocity. Each repeat has a gate of half
its spacing. Ratchet also shortens the original gate to half that spacing;
Echo leaves the original gate alone. Ratchet uses a fixed Grid window because
the eventual duration of a live held key is not known in advance.

For Echo, knob 5 becomes **Decay %** (default 25): 25 removes a quarter of the
remaining velocity on each repeat; 0 keeps the velocity unchanged and 100 silences
all repeats. Its pattern controls repeat count; Decay is not added to that count.
Multiple repeat lanes add independent copies of the original transformed note;
they never feed one another or harmony detection. The existing conductor
recording bridge does not record these copies; recording downstream MIDI can.

Source note-off may precede the repeats. Releasing a punch button cancels that
press's queued repeats and releases its generated voices, even if Auto is On.
Changing the lane's settings, bypassing automatic operation, Stop, state load
and performance reset also cancel pending repeats. Other held notes retain their
own note-offs. Scheduling uses a monotonic DSP clock so a transport seek cannot
strand a repeat. Each output route allows 128 pending bursts; excess bursts are
omitted while original notes still play. Missed attacks after a late callback are
dropped rather than emitted as a catch-up flurry.

## Clip playback operations (Movy only)

Note operations work on live input and recorded notes that pass through HB.
Clip operations need an existing playing clip. Movy changes which stored notes
are emitted while its normal transport keeps moving. Leaving an automatic
condition window, or releasing the last manual hold when no automatic operation
is eligible, returns playback to that transport position. They never rewrite the clip and
are bypassed on the track currently recording. Live pads are unaffected.

| Operation | Controls and behavior |
|---|---|
| Clip Repeat | Grid selects the repeated section length, anchored to the section containing the press. Starts at that section's beginning. |
| Clip Reverse | Mirrors note intervals within the clip loop, preserving their gates. |
| Clip Time Shift | Amount is a signed number of Grid steps ahead (+) or behind (−) normal playback. Wraps within the loop. |
| Clip Speed | Amount +2/+3/+4 means 2×/3×/4×; −2/−3/−4 means 1/2×, 1/3×, 1/4×. Zero and ±1 mean normal speed. Starts at the press position. |

Grid and Amount control the transformation as described above. **Auto On** enables
automatic activation, with **Cycle / Every / From / Through** defining its windows.
For a last-bar reverse, choose Clip Reverse, Cycle 1 Bar, Every 4, From 4,
Through 4 and Auto On. No editor needs to be open. Auto Off preserves hold-only
operation. The settings remain stored in HB alongside the other lanes.

An automatic gesture captures the current playing clip and position when its
eligible window begins. It continues through an inclusive multi-cycle range,
then releases. Full-range conditions at probability 100 run continuously across
cycle boundaries, allowing effects you leave on. A new partial-range window
starts a fresh gesture. A backwards transport seek re-anchors it; a clip change
clears runtime gestures and reapplies the schedule to the new playing clip.

Probability is sampled once per phrase window, rather than once per note. The
Repeat random mode repeats the same deterministic decision for that lane/track;
Evolve varies it between phrases. Holding a clip step bypasses this decision and
cycle eligibility. Pattern, Offset, Phase, Group and Advance do not alter the
clip reader; they retain their note-operation meanings.

Manual gestures take priority over all automatic gestures. With several clip
operations held, the most recently pressed wins. Releasing it restores an earlier
held operation, or the eligible automatic operation; elapsed reader time continues
in the background. If several automatic clip lanes qualify, the highest-numbered
lane wins. They do not compose conflicting reader transformations. Blue LEDs show
cycle eligibility, not the result of a probability draw or which competing lane
wins. Existing note gates finish normally when a window closes.
Stop, clip launch/change, set load, leaving Perform mode and teardown clear
manual clip gestures. Auto schedules remain independent of Step Row mode.
Recording bypasses clip operations on the recording track. No source clips are
rewritten; parameter and set changes refresh the sequencer configuration without
per-frame parameter polling. Note-offs remain paired with emitted pitches; existing gates finish
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
cursor, the Conditions page, immediate dependent-value refresh, and inert knob-touch activation.

Release workflows build the ARM packages and run native and UI gates. This release still needs the on-device check after installation. Earlier eight-button approach/enclosure gestures were confirmed on Move; the expanded assignment row and clip operations are new.

For scale-aware chord families, ordered combinations and Auto Off behavior, see [Secondary cadences and linked approaches](timing-guide.md#secondary-cadences-and-linked-approaches). Untouched modern defaults adopt the new assignments on load. Explicit saved assignments remain intact.

Operation-lane controls now share single-tap automatic completion, momentary hold and double-tap persistent activation. Persistent lanes pulse smoothly; armed and held lanes stay solid, then turn off on deactivation. See [operation gestures and activation lights](timing-guide.md#operation-gestures-and-activation-lights). The separate Next Latch control is removed; lane 5 supplies both Next Harmony behaviors and lane 6 is available.


## Named performance panels (0.2.197)

The sixteen **Step Seq** slots always correspond one-to-one to physical steps
1–16 and the knobs on **Ops 1–8** / **Ops 9–16**. Their mapping is fixed. The
operation assigned to each slot remains editable; its name labels the knob.
Turning these knobs adjusts the operation Amount (or Form), never its slot.

Named controls have independent settings and do not consume Step Seq slots.
The Operation, Timing / Trigger and Conditions editors select them as
**Pitch Play N: name**, **Chord Play N: name**, or **Harmony Play N: name**.
Their operation identities are fixed; the other operation settings remain editable.

| Knob | Pitch Play | Pitch Cadences |
| --- | --- | --- |
| 1 | Chrom Below | Backdoor II |
| 2 | Scale Above | Backdoor V |
| 3 | Chrom Above | Tritone II |
| 4 | Secondary II | II-V-Target |
| 5 | Secondary V | Backdoor II-V-Target |
| 6 | Secondary VI | Tritone II-V-Target |
| 7 | Chord Form (Chord Play 1) | Enclose Above Below |
| 8 | Auto Chord Repeat (Chord Play 2) | Enclose Below Above |

**Foll Map knob 6** is fixed to **Next Harmony** (Harmony Play 1).
Existing tap/hold/double-tap and solid/pulsing LED semantics apply throughout.
The full cadence operations consume three source gestures, not three generated
chord voices or arpeggiator steps. A chord played as one gesture advances once.

Relative to C, the cadence families are Dm7–G7–C, Fm7–Bb7–C, and
Abm7–Db7–C. Chrom Above moves a melody by one semitone; with chords enabled it
constructs a dominant on bII. Tritone II constructs the preceding minor chord
on bVI. These offsets are relative to the rendered destination, including
chromatic approach destinations. The tritone dominant uses Lydian-dominant
extensions and its ii uses Dorian; both retain their chord quality.

Fresh states have empty Step Seq slots. Existing saved slots are retained,
including their former defaults, and named controls are added separately.
New action recordings carry all named-control outcomes; old 16-slot action
recordings remain readable. Updating both HB and Movy is required for the
expanded recording format.
