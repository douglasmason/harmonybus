# Key center operations — 0.2.265 / hbclean.185

Update both HarmonyBus and HarmonyBus Movy for recorded modulation sequences.

| Control | Behavior | Recorded? |
| --- | --- | --- |
| Set Live Key: Tonic / Scale / Apply | Choose an exact sounding key without a note; applies a live key center | No |
| Live Key Center | Arm, then play the destination target; stays live until Off/reset | No |
| Recordable Key Center | Arm, then play the destination target; stores an absolute tonic and scale | On a recording conductor |
| Relative Key Center | Each activation moves the sequence by its signed semitone amount | On a recording conductor |
| Key Step Back | Restore the previous sequence tonic **and scale** | On a recording conductor |
| Key Return to Start | Restore the sequence's original tonic **and scale**, clearing its steps | On a recording conductor |

The existing Harm Play Settings key-center knob is now labeled **Live Key Center**. Assign the other operations to any of the sixteen existing operation slots. Relative Key Center's Amount is **Shift (semitones)**, from -12 to +12; Offset becomes **Return After**, from 0 (disabled) to 64. For +1 and Return After 4, the sequence is C, C#, D, Eb, E, C: the fifth activation returns, allowing the fourth shifted section to sound first. Use a negative amount to descend; use Step Back to retrace actual previous destinations, including changes of scale.

Recordable operations audition while not recording, but those earlier activations are not added to the next take. They record only while recording a conductor, without requiring a note for relative/back/return operations. Each recorded event advances once per pass through its clip, including loops. A held control, note retrigger, display read or repeated sequencer tick does not add steps. Their Auto control is disabled: clip events provide the recurrence.

Each track's sequence keeps its starting key plus the most recent 64 steps. Return restores the start even after that history limit. Step Back cannot go farther than the retained history. Live Key Center overrides the recorded result while its underlying sequence continues; turn Live Key Center Off to hear the current recorded destination. Parallel Scale and parent-scale operations retain their independent precedence.

Recording and clip Undo never take ownership of Live Key Center. Undo that leaves a sequence unchanged preserves its modulation progress; edits to that sequence reconstruct it from its anchor, loop count and playhead. Stop releases the recorded contribution; restarting rebuilds from the start. Attaching mid-song reconstructs the appropriate loop and position without replaying old notes. Performance Reset clears both live and sequence state.

Old absolute key-center events remain playable. Their operation ID now selects Live Key Center for new performances; choose Recordable Key Center explicitly when you want new automation. Source notes remain editable input intent and use the existing conductor/follower travel settings.

## Explicit entry and approach protection

Open **Set Live Key** in Steps, Perform, or Harm Play. Turn **Tonic** and **Scale** to stage the destination, then touch **Apply Key**. For example: F# / Natural Minor / Apply. Touching the selectors only displays their choices; turning Apply does nothing. The chosen tonic is the sounding pitch class, including when Master Transpose is active. The selectors are saved per track; restoring them does not apply a key change.

Apply replaces pending pad-selected arrivals and disarms pad selection. It uses the existing live key operation and conductor/follower travel policies, remains live during recording and clip Undo, and does not emit a note. Independent Parallel Scale operations retain their existing precedence. Use Live Key Center Off to remove the live key.

Approach-role steps must not select the destination. Tapped motifs now wait for their actual anchor, including on the first tap or when changing the tapped input. Automatic motifs retain their scheduled arrival. Deliberate chromatic targets remain valid destinations.
