# Panel controls in 0.2.205

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
