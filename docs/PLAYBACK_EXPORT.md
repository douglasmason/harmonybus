# Playback and export architecture

## Non-destructive source model

Harmony Bus treats the authored clip as **Source**. The musical result produced by
Harmony Bus is **Rendered**. Source is never intentionally discarded.

For export/transfer the supported representations are:

- **Source** — authored notes.
- **Rendered** — ordinary MIDI notes after the Conductor timeline and Follower mapping.
- **Both** — default; preserves the authored material and the audible arrangement.

Rendered MIDI therefore has no runtime dependency on Harmony Bus.

## Native Move playback boundary

Move's native sequencer notes are observable by Schwung through Move's MIDI_OUT
echo. This is sufficient for a Conductor to infer harmony from a stored native
Move clip.

It is not, by itself, sufficient for transparent replacement of a native
Follower note. The MIDI_OUT echo occurs after Move has already consumed the
sequencer event for its native instrument. A chain MIDI FX can transform that
echo and Schw+Move/Pre mode can inject transformed MIDI back into Move, but that
is additive: it cannot unsound the original native note.

Therefore Harmony Bus must not claim that ordinary chain MIDI FX provides
non-destructive native-note substitution.

## Execution targets

The implementation separates three cases:

1. **Native Conductor** — observe Move's stored clip via MIDI_OUT echo; publish
   inferred harmony. This is supported by the MIDI-FX module.
2. **Schwung-hosted/virtual Follower** — transform source MIDI before its sound
   generator. This is true playback-time substitution and remains
   non-destructive.
3. **Native Move Follower** — requires a rendered playback layer rather than
   plain MIDI-FX substitution. Source remains preserved; Rendered notes are what
   must be printed/cached for reliable native playback and for export.

The TrackBank/export layer is the canonical place for Source/Rendered/Both and
for printing Rendered MIDI when leaving the Harmony Bus environment.
