# Two-dimensional MIDI head surface

HarmonyBus's existing **Field Outputs** Remote UI now includes a shared
**MIDI event heads · X / Y** touch surface. It is an alternate controller for
two existing Field output channels, not a new generator, sequencer, synthesizer
or MIDI routing protocol. The identical canonical `field-web.js` component is
composed into the HarmonyBus–Movy browser studio and Schwung Remote UI.

## First performance

1. On Movy hbclean.362 or newer, open Joint Play → Sources for a Bass, Comp or
   Melody actor. Choose an ordinary sparse MIDI pitch clip, a playing rhythm
   donor and the **Event Heads** alignment.
2. Open that actor's **MIDI Event Heads** page. Choose **Field 1** for
   **Head 1 Position** and **Field 2** for **Head 2 Position**. The source
   MIDI clip may contain only one note or a few simultaneous chord groups.
3. Open HarmonyBus under MIDI FX in Schwung Remote UI, or enlarge **Field
   outputs** in the browser workspace. The 2D surface is next to the same
   sixteen Field output sliders. With its default selectors X controls
   Field 1 and Y controls Field 2.
4. Drag one finger to position both read heads. Add a second finger to control
   Y independently while the first continues to move X. The green crosshair
   indicates the requested position and the cyan trail shows the gesture.
5. The two Event Heads read the corresponding source *event groups* on the
   **next accepted rhythm donor attack**. Moving the surface alone does
   not create notes. Native HarmonyBus note mapping, gate and release
   ownership and stock Move sound engines remain unchanged.

The source note/chord groups are addressed by their **ordinal**, not by the
silence between events. At the left/bottom edge, position zero chooses the
first source event; at the right/top edge, 100% chooses the last.
Reverse, PingPong, offsets, head count and note operations are still set in
Movy's existing MIDI Event Heads page. The Field channel selectors on this web
surface change its output channels only; they do not silently reconfigure
actors that read those channels.

## Input behavior

- **One finger:** controls both X and Y.
- **Two fingers:** first owns horizontal X, second vertical Y. Lifting the
  second returns both axes to the remaining finger. A third touch does
  not take over either axis.
- **Keyboard:** focus the XY area and use arrow keys; Shift changes the
  step from 1% to 0.1%. Escape releases without committing a latch.
- **Momentary:** releasing the last finger returns both selected Field
  channels to their underlying moving values, with the DSP's existing
  glide behavior.
- **Latch:** a normal final release commits both positions to the existing
  Field latch owner. Press Release all or the individual lanes to clear
  the held output. Pointer cancellation, blur, hiding the page, changing
  UI panels/tracks, or disconnecting never creates a new latch.
- **Conflicts:** choose *different* Field channels for X and Y. A held
  individual slider must be released before the XY pad can acquire its
  lane. Selecting a slider while XY owns that lane releases the entire
  XY gesture instead of making competing writes.

All overrides go through `midi_fx1:field_outputs_remote` as one sixteen-lane
frame. Finishes reuse `midi_fx1:field_output_finish`. The existing
underlying DSP enforces time-limited ownership, cancellation and latch
semantics. The surface does not create separate voice state or increase
audio callback work.

## Controller and recording limits

This is **precise 2D touch position control**, not MPE pitch bends,
audio-rate granular playback or an independent MIDI note scheduler.
The existing native Move/Launchpad Field Play output strips, knobs and
pressure remain available as alternative sources for the same channels;
a dedicated physical **2D pad layout** is a separate step.

Touchscreens without real force sensing supply position only. Existing
Move/Launchpad pressure-sensitive Field overrides are not replaced by
this XY pad. Neither Field output gestures nor MIDI head motions are
automatically recorded or baked: only the source-route settings are
saved with the Set. Deterministic head-motion recording, clip Bake
equivalence and optional harmony-aware connector notes remain planned.

## Verification

`tests/field_web_test.mjs` covers simultaneous X/Y ownership,
normalization and inverted display Y, final-packet commit, pagehide/blur
and pointer-cancel refusal, Release all, stale pointer rejection, keyboard
input, channel conflict, and unchanged per-lane slider semantics.
The shared web-component and native Field DSP suites run in CI.
A physical Move/Launchpad audio, latency and LED test remains necessary.
