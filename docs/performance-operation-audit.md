# Performance actions and shared operations

The operation system should own activation, assignment, feedback and completion.
Surfaces deliver gestures and musical inputs. Musical engines retain their own
voice ownership, clocks and data structures.

## Current correction

Note Blend is now an assignable operation. Its overlapping-note requirement is
declared in the operation catalog; normal one-shot/latch/hold handling is shared.
Immediate-action permissions also come from explicit declarations, replacing an
ID-range condition that accidentally restricted newer operations' gestures.

## Candidates, in priority order

1. **Field overrides.** `src/field_params.h` has separate `field_gesture`,
   `field_release`, output-finish and latch handling. Expose rate/zoom/output
   overrides through operation activation, retaining input leases for device
   disconnect safety. A timed input lease and an intentional musical latch are
   different responsibilities.
2. **Cloud performance.** Movy's cloud surface and generator have separate
   emit/contact and capture/hold commands. Operations could invoke those actions
   from any surface. Preserve the distinction between capturing a cloud's contents
   and latching an operation's activation. Keep 14-bit X/Y and contact ownership
   in the existing input/engine paths.
3. **Playhead manipulation.** Clip Speed, Reverse and Time Shift already exist.
   Generalize their target where useful, then expose position/scrub and freeze as
   operations. The existing continuous playhead, LFO/Perlin drivers and two
   sequence-position inputs remain reusable engines and signal sources.
4. **Temporary rhythm overrides.** Pattern, density and rotation are useful
   performance operations. Apply an overlay to the current rhythm state and
   restore the current base on completion. Filling or baking a pattern into a
   clip is an edit command with undo, not a reversible held override.
5. **Chord state coverage.** Chord Form, Chord/Arp State, Chord Spread and Voicing
   Rotate already are operations. Extend the existing state operation for missing
   range/strum settings instead of adding another collection of special toggles.

## Common contract

- Activation: momentary, armed one-shot or persistent latch, from existing gestures.
- Inputs: held notes, overlap, ordered note/operation events, pressure or X/Y.
- Target: track, voice group, playhead, cloud or parameter, with clear ownership.
- Completion: the operation reports when its musical work or phrase is finished.
- Effective state: resolve active overrides over saved base settings; avoid saving
  temporary state or restoring an obsolete snapshot over a later user edit.

Pressure curves, direction/range mapping and contact geometry belong to shared
input mapping. LFOs and Perlin noise are signal sources. Routing, bend sensitivity,
scale choices and interpolation defaults remain configuration. Destructive edits
retain explicit command/undo semantics. Not every setting needs a new operation.

The safe migration is one adapter at a time around the existing engines, backed
by cross-surface tests. Replacing those engines with a general-purpose framework
would add complexity without improving musical behavior.
