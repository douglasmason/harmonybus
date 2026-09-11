# HarmonyBus virtual source bank

## Goal

Add a second four-track composition bank without replacing Move's native tracks
1-4 and without rebuilding Move-like clip recording from scratch.

The native bank remains the audible/export bank:

| Move track | Purpose |
|---|---|
| 1 | Native drums |
| 2 | Render destination for source track 6 |
| 3 | Render destination for source track 7; default conductor monitor |
| 4 | Render destination for source track 8 |

The virtual bank is hosted by a Move-like sequencer (initial target: Movy):

| Virtual track | HarmonyBus role | Default destination |
|---|---|---|
| 5 | Conductor | raw monitor to MIDI channel 3 |
| 6 | Follower | render to MIDI channel 2 |
| 7 | Follower | render to MIDI channel 3 |
| 8 | Follower | render to MIDI channel 4 |

Move tracks 2-4 are assumed to be configured to receive MIDI channels 2-4.
Automatic configuration is a convenience feature, not a dependency of v1.

## Clip view

The user-facing Clip/Session view has two banks only:

- Bank A: tracks 1-4
- Bank B: tracks 5-8

`+` and `-` switch between the banks. Movy's existing grouped-track/session
machinery should be reused rather than implementing clip capture, step editing,
launching, quantization, undo, and persistence again.

## Dual-output source tracks

Every MIDI event on a virtual track has two independent branches:

1. **Source instrument branch** -- feeds the virtual track's local instrument.
2. **HarmonyBus branch** -- feeds Conductor/Follower processing and, for
   Followers, injects the rendered result into the configured MIDI channel.

For a follower the conceptual route is:

```
virtual clip / live pads
          |
          +--> local source instrument --> local source audio
          |
          +--> HarmonyBus follower ------> native Move destination
```

The same routing applies to live input, recording input, step-entered notes, and
clip playback. Recording does not require a special HarmonyBus monitor mode.

## Mute semantics

Source-track mute MUST be an **audio mute only**. It must never suppress source
MIDI from reaching HarmonyBus.

This supports the established workflow:

### Record/source-monitor mode

- source track unmuted
- native render destination muted
- user hears the unrendered note through the source instrument while the source
  clip records it

### Rendered/live mode

- source track audio muted
- native render destination unmuted
- live input and source clip playback continue through HarmonyBus and are heard
  through the destination instrument

Both may be unmuted intentionally to compare/layer source and rendered output.

Movy's ordinary sequencer mute is unsuitable for this because it gates
sequenced MIDI. The integration should instead use its chain-mixer audio mute
(`TrackMix.muted`) for HarmonyBus source tracks.

## Configuration contract

`src/hb_virtual_bank.[ch]` owns the default logical mapping. MIDI channels are
stored zero-based to match HarmonyBus' existing `render_channel` representation.

`src/hb_virtual_router.[ch]` owns the routing invariant that source audio mute
and HarmonyBus MIDI processing are independent.

The first implementation should keep the existing HarmonyBus `render_channel`
parameter as the follower routing control instead of introducing another
routing abstraction.

## Sequencer reuse boundary

Do not vendor or rewrite Movy's full sequencer until there is evidence that an
overlay/integration cannot work. Its sequencer consists of a substantial Rust
engine plus TypeScript UI and already provides the behavior needed here.

Preferred integration order:

1. Run Movy as the virtual-track/clip host for 5-8.
2. Seed its per-set `chains` document with HarmonyBus plus a lightweight source
   instrument on tracks 5-8.
3. Keep tracks 1-4 on their existing native host.
4. Adapt the source-track Mute gesture to chain audio mute while leaving
   sequencer MIDI alive.
5. Restrict the HarmonyBus-facing bank navigation to 1-4 <-> 5-8.
6. Only if these seams prove insufficient, vendor the minimal Movy components.

## First hardware acceptance test

1. Select virtual track 6.
2. Unmute 6 and mute native track 2.
3. Record C-D-E-F; hear track 6's source instrument and confirm the notes land
   in the track-6 clip.
4. Stop recording, mute 6, and unmute 2.
5. Play the clip; HarmonyBus renders the stored notes into MIDI channel 2 and
   Move track 2 sounds them.
6. With 6 still source-audio-muted, play live on it; rendered notes must sound
   on track 2 immediately.
7. Switch clips on track 6; rendered musical content changes.
8. Switch clips on track 2; its native overdubs/automation change independently.
