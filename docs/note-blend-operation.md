# Note Blend as an operation — HarmonyBus 0.2.324

Assign **Note Blend** to any of the sixteen operation slots. Choose its output
with the slot's control or Operation Amount: **Steps**, **Bend 2**, **Bend 12**,
**Bend 24**, or **Bend 48**. The default is Steps. It appears in the Pitch group
in Movy's shared operation picker and Remote UI.

This uses the existing operation gestures:

| Gesture | Behavior |
| --- | --- |
| Tap the assigned touch knob | Arm one overlapping-note phrase |
| Hold the control | Momentary operation while held |
| Existing latch-on gesture/turn | Stay enabled for successive phrases |
| Existing latch-off/cancel | Disable and restore the underlying blend setting |

After arming, press a second note while another note remains held on the same
input channel. That overlap starts the pressure-weighted blend. Releasing one
note leaves it active; another overlapping press continues the same phrase.
When all live notes are released, a one-shot finishes. Playing separate single
notes without overlap does not consume the arm. A persistent latch stays enabled
and waits for the next overlapping phrase; a held control does likewise until
its release. Move, Launchpad and Remote UI use their established gesture routes.

Gesture activation and musical input are separate. The operation catalog declares
an overlapping-note input requirement. The engine reuses the existing physical
note map and generic gesture state; it does not introduce another tap detector,
contact map or latch mode. Generated chord voices, motif output and clip playback
do not masquerade as two live fingers. The blend still processes the track's
output voices once activated, with the existing local/Render To behavior.

The original `note_blend` setting remains as **Base Note Blend** for saved-session
compatibility and an always-on baseline. Operation overrides never overwrite
that baseline or save a transient arm into a preset. Releasing the operation
restores the current baseline, including edits made while the operation was active.
Leave the baseline Off for normal tap-to-arm use.

Steps works with stock Move sounds. Continuous Bend needs a compatible synth,
a matching bend range and a dedicated output channel. For browser NuSaw, use
Bend 12 and set the synth's Bend Range to 1.0. This remains monophonic channel
bend, not per-finger MPE output. See [note-blend.md](note-blend.md) for output
smoothing, reanchoring and shared-pitch behavior.

Gesture permissions now use explicit catalog declarations instead of treating
all operation IDs after Relative Key Center as immediate actions. This restores
normal latch support for Note Repeat, Repeat Divisions and Smart Approaches,
while preserving the three immediate key-history commands' existing behavior.

Native regressions cover arm/overlap/last-release boundaries, continued overlap,
same-channel requirements, momentary and persistent controls, playback/chord
isolation, baseline/preset restoration and catalog permissions. Production-chain
and compiled browser tests exercise actual gesture commands and note packets.
