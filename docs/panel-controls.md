**Connector controls:** Pitch Play is now Approach Harmony. Connector Below (CCB) and Connector Above (CCA) use Connector Harmony. Leading Tone (LT) and Upper Dim keep diminished triads/sevenths; Tritone Sub (TTS) keeps dominant quality. Leading Tone and Upper Dim are available in the Ops lane and Harm Perform bank pickers. Legacy saved connector assignments retain their behavior.

**Fixed lane editing:** Ops 1–8 and Ops 9–16 always control lanes 1–16. Shift + turn changes the operation on that fixed lane; named performance controls retain their function and use Shift + turn for their amount or mode. Editing preserves an existing latch and does not change the lane-editor selection.

**Chord Forms** brings Conductor Form, Follower Form, Pad Color Form, and the live Detected Harmony Form together above This Track Form, Quality, Inversion, and Next Pulse. Voicing remains on Chords. Next Pulse defaults to None and selects tones of the displayed preview harmony using Pad Color Form: 3, 7, 3+7, 1, 5, 1+5, 9, 11, 13, 9+11+13, or All. The selection follows the displayed preview: Effective uses effective harmony/color, Next uses available next harmony/color, and Full Lookahead uses full-next harmony/color. Current-only stays unpulsed. Selected pads use that single color without blending it with current or overlap colors. Selected resting pads breathe smoothly between 50% and 100% requested brightness at the global Pulse Rate (Off disables it), even when the general Pulse Shape is None. Playback highlights remain solid. No displayed preview harmony means no pulse. This Track Form can inherit Follow Role or override it. Fresh conductor defaults use Triad; fresh follower and pad-color defaults use Follow Detected. In Scale Degree mode, Follow Detected transfers the detected degree pattern to the played target; in Harmony mode it follows the detected chord. A missing detected harmony falls back to a triad for Scale Degree. Existing saved choices are preserved. Rootless 7 is 3–7, and Rootless 9 is 3–7–9. The separate Pad Harmony panel and redundant Chords/Role Defaults form controls have been removed. Harm Perform uses framed cells, short names, Hold/Armed/Latch captions, and a Chord + Arp On/Off caption; Shift-turn retains the full assignment peek.

**HarmonyBus 0.2.232 / Movy hbclean.137:** Shift + turn edits a Harm Perform operation with a list peek. Outside the two approach-row layouts, tap arms one use, hold is momentary, and ordinary turns latch on/off with explicit feedback and a pulsing white LED. In approach-row layouts, touches select persistent row assignments, ordinary turns do nothing, and entering the layout clears permanent approach-bank latches. Step buttons trigger independently without changing the rows. The first panel defaults to Secondary V, Secondary II, CCB, CCA, Leading Tone, Tritone Sub and Backdoor V; the second contains sequences. Knob 8 on both panels controls Chord + Arp. Chromatic approach pads stay black with Auto Chord enabled, and a 75 ms visual onset flash catches short notes without changing MIDI gates.

**Approach routing fix (HB 0.2.217 / Movy hbclean.126):** Dedicated Approach and Triple Approach layouts enable follower approach pads independently of chromatic mapping and travel. Sound and play-color previews use the same eligibility rule. Operation knob LEDs own their indicators; generic parameter-value lights cannot overwrite them.

**FIFO Approach / Perform 2 (HarmonyBus 0.2.217, Movy hbclean.125):** Copy cycles Steps, Perform 1 and Perform 2. Perform 2 knobs and step buttons share the same 16-slot operation/motif bank on every layout. Each new touch enters a persistent three-item FIFO, without requiring overlapping touches. Triple Approach plays the queue from the top row down: row 3 (oldest), row 2, row 1 (newest), then the scale target. A fourth touch evicts the oldest assignment. Each row advances its own assigned motif; single Approach uses the latest touch. The display mirrors rows 3–2–1, highlights the newest assignment, and retains a compact slot summary. Tap/hold/latch performance and overlapping performance sequences remain independent of the row FIFO. Existing bank assignments are preserved; the eight triple motifs remain optional library choices.

> **0.2.217 / Movy hbclean.124:** Secondary II consolidates Scale Above. Secondary LT consolidates Chromatic Below and remains a semitone below the target; Auto Chord uses the Chromatic Chord setting. Secondary VII follows the effective scale’s seventh degree and can differ from LT. Legacy saved assignments and recorded note intent remain compatible. Pitch Play and Secondary panels now access the same Secondary II lane.

# Panel controls in 0.2.217

## Approach mode (Movy hbclean.124)

Copy tap cycles Steps, Perform, Approach. Approach Rows presents eight knobs:
turn to assign a pitch/harmony transformation or Motif 1–16; touch to select.
Overlapping touches append in touch-down order. Releasing the knobs retains
that selection; a new touch with no other knob held replaces it. Successive
approach-pad presses advance through the selected transformations and motif
steps, wrapping after the sequence. Ties extend the preceding motif event;
rests occupy a silent press. Each press targets its paired scale pad under
the currently selected rendering harmony, including Next Harmony.

Step buttons 1–16 arm a motif from the separate per-track bank. The next
scale/input pad supplies its target and launches the complete motif, using
the existing motif event durations and chord rendering. Motif Bank 1–8 and
9–16 pages assign existing stock or User motifs; they do not copy a second
library or replace Perform's operation lanes. Knobs initially reference
bank slots 1–8. Before the first selection, approach pads retain Chromatic
Below. Use the Approach keyboard layout for two complete approach rows;
the existing Piano gap pads work too when chromatic approach mapping is on.

Single transformations retain normal note-off and arp handling. Motif choices
use the motif renderer's durations. Recorded inputs retain the selected
transformation or motif reference/step so later knob choices do not replace
the recorded intent. User-motif references continue to use that shared motif.
Assignments and selected sequences save per track; physical touch and held
note state do not. Lookahead/pad-preview reads never advance the sequence.


Operation knobs always control their assigned lane: clockwise sets the permanent latch, counterclockwise clears it, tap arms one use, and hold acts momentarily. Repeated knob taps do not promote to a permanent latch. A pre-existing permanent latch survives a tap or hold. Step-button double-tap behavior is unchanged.

With Movy hbclean.121, sounding output alone uses a faint blend of Play Color and the normal pad color. A live or recorded input whose corresponding note is also sounding uses solid Play Color. Input alone preserves the background. Play Color Off disables both highlights.

Chords and Arp / Strum remain near the start. Foll Notes and Pads Global are last in Movy because they are diagnostics and infrequent setup controls.

## Role Defaults

One advanced panel replaces the four conductor/follower chord/scale defaults panels. Its first knob selects Conductor Chords, Follower Chords, Conductor Scales or Follower Scales. The remaining labels and values refresh together.

Chord defaults cover form, quality, inversion, voicing, spread and chromatic quality. Scale defaults cover gap filling, context scope, dominant scale, borrowed scale and local palette. The final scale field identifies the shared scope.

These settings affect all tracks of that role which inherit the corresponding setting. A track override remains in effect until its control is returned to Role Default. Chords and Arp Edit Target is independent: Track Settings edits the selected track, while Lane N edits a temporary Chord/Arp State operation.

## Local Palette

Track Scales combines the three binary local-scale choices into one eight-option palette, plus Role Default. The label order is major / minor / half-diminished:

- Ion or Lyd: Ionian or Lydian.
- Dor or Aeo: Dorian or Aeolian.
- Loc or Loc#2: Locrian or Locrian with raised second.

Selecting a palette sets all three choices; Role Default restores inheritance for all three. These are the local recipes used by Strict Local and by Auto Local where context does not select another collection.

## Harmony Target

Foll Map's dedicated Harmony Target operation selects Current or Next. Turning it chooses the target; the existing touch, hold and latch gesture activates the operation. It does not occupy a user-assignable lane. Next still requires known upcoming harmony during playback.

## Startup and LEDs

With compatible Movy, a stopped session previews the first note group in each selected conductor clip, including auto-chord settings and per-note recorded intent. It does not start transport, emit MIDI, consume live gestures or teach the full-loop harmony model. Live conductor notes take precedence. This is an opening-content preview, not a simulation of future probabilistic triggers, evolving operations or a full performance.

In Steps mode, an empty playing clip uses the four groups of four step buttons as a beat indicator. A populated clip uses sequence steps and its playhead. Perform owns those buttons for operation activity; motif editing owns them for motif steps. The fast beat update respects both owners.

Pad changes reserve the entire changed group within the existing MIDI budget, avoiding a partial chord-color update when the budget is exhausted. Beat updates also run on cached touch-display frames. Hardware still transmits LED messages sequentially; these changes reduce software staggering rather than claiming simultaneous physical updates.

Arp Order offers **Shuffle** and **Shuffle Cycle Pin**. Both honor Arp Start when the arp starts or its selected pad/chord reanchor occurs. Shuffle lets later cycles begin randomly; Shuffle Cycle Pin starts every cycle at the selected anchor. Both visit each available note once per cycle. With Arp Start set to Order, neither pins a note.

In single Approach layout, overlapping Harm Perform knob touches compose the row in touch order. Approach-pad presses advance the composition and wrap after its last entry; target pads do not consume it. A fresh non-overlapping touch replaces the composition. Triple Approach keeps its three independent row assignments. Movy local MIDI input remains active with HarmonyBus Receive Off.

Single approach-row sequences advance on repeated presses of the same approach pad. A target pad or a different approach pad resets the sequence to its first entry; note release alone does not reset it. Recorded approach tokens retain their captured step.

Harm Perform shows each operation status directly beneath its knob: Off, Armed, Hold, or Latch. In approach layouts it shows the assigned row, or Seq with the next sequence position on the current member. Chord + Arp uses cyan-blue LEDs with the same solid armed/held and pulsing permanent-latch distinction, and an explicit state caption.

The Perform footer expands the motif preset dictionary into a live sequence strip. The highlighted entry is the next step to play; long sequences scroll around that position. T remains an explicitly played resolution in approach layouts. User motif entries show their stored step numbers.

Single approach-row sequence members show their touch-order position beneath every knob (1, 2, 3). The dictionary strip highlights the next event. Fresh Perform 1 defaults contain one Leading Tone assignment, on knob 5; knob 1 is Secondary V. Saved assignments remain unchanged.

Chord + Arp one-shot remains active through approach pads and the full hold of the first live target pad played after arming. Releasing that target consumes it, even if other pads remain held. Recorded playback and unrelated releases do not consume it. Momentary holds and permanent latches retain their existing behavior.

With Retrigger Held on, held approach notes retain the operation captured at their original press when harmony changes. Revoicing uses that operation against the new harmony and does not advance the approach sequence or resolve to the target.

Knob 7 on both Harm Perform panels is Motif Latch. Turn right to repeat the currently armed whole motif/sequence; turn left to stop the latch. Its LED pulses white while latched. Selecting a new motif clears this latch. Approach layouts display Rows here and keep target pads independent; their approach pads already provide persistent access. Former knob-7 operations remain available on their step slots.

Chord Forms uses shared Conductor Form and Follower Form values for the current set. This Track Form offers Follow Role or a local override, independent of any legacy Chord State editor. Auto is no longer offered in form menus; saved legacy Auto values remain supported internally. Role Defaults and Chord State panels are removed. New sets reset conductor form to Triad, follower/pad forms to Follow Detected, and track form to Follow Role.

Detected quality and detected form are separate. Recorded scale-degree gestures use the current conductor form while retaining richer known quality: a Min7-quality gesture rendered as Power reports 1–5, and switching its form to Seventh exposes 1–3–5–7. Power, shell and rootless forms are not filled out merely to match the inferred chord name. Baked recordings retain their recorded voices. Opening preview uses the same quality/form distinction; learned lookahead is invalidated when rendering settings change. Next Pulse can select extensions added by an explicit Pad Color Form; Follow Detected adds no missing tones.
