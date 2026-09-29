# Temporary chord and arp states

Assign **Chord/Arp State** to one of the sixteen operation lanes. Assignment copies the current track settings into that lane. The operation settings are shared, like other operation lanes; the editing destination and normal chord/arp settings belong to the selected track.

Use **Edit** on the Operations panel to open Chords for that lane, or choose **Edit Target → Lane N** on Chords or Arp / Strum. Both panels edit the same destination. Selecting an edit target never activates the operation. Choose **Track Settings** to edit normal playing again. **Copy Track** on Chord State replaces the selected lane snapshot with the selected track's settings.

The amount control becomes **State Preset**: Custom, Scale Degree Burst or Current Harmony Burst. Both bursts enable Repeat Arp, Shuffle order, Root + Fifth Low voicing, Top Note inversion, 1/32 rate, Momentary hold, Played / Pad start and First Note Free timing. Changing an individual setting marks the preset Custom.

Hold a lane to use its state temporarily. The existing lane latch gestures also work. When several state lanes are held, the most recently activated wins; releasing it reveals the earlier one. Ending the last state restores normal track settings. State changes release owned generated voices, preventing orphan notes. Editing Track Settings during an active state changes what returns afterward.

## Two-note input

On Chord State choose **Input → Root/Bass + Top**. Two overlapping notes on the same MIDI channel form one chord: the lowest note is the root in Scale Degree mode, or the bass in Current Harmony mode; the highest note is the exact top voice. Interior notes follow the chord/voicing settings. There is no timing window: one note alone waits silently for its partner. Hold the lower note and move the upper note to reshape the chord. Releasing down to one note stops the chord. Pair input uses physical overlap, irrespective of the arp latch setting; use a held or persistent operation lane for this gesture.

These are live performance state overlays. Their full setting bundles are not yet captured into recorded operation events or motif intent. To record the resulting burst as-is, record rendered MIDI.

## Arp timing controls

**Start Note** selects the first pitch; its new default is **Played / Pad**. **Start Timing** selects the clock behavior; its default is **First Note Free**. **On Grid** replaces the former Auto label. First Note Free plays the opening note immediately and places following attacks on the grid.

Note Phase was a sequence-index rotation, not a delay in milliseconds or beats. Its generic metadata range was ±256; the effective bound depends on chord-grid length divided by arp-note length. Explicit Start Note choices supersede that rotation. It has been removed from the main Arp panel to make room for Edit Target.
