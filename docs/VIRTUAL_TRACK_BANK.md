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

For Followers, `render_channel` means transformed render destination. For the
Conductor, the same field means unchanged monitor destination. This keeps the
persisted HarmonyBus state and the Movy profile to one routing field.

`src/hb_virtual_router.[ch]` owns the routing invariant that source audio mute
and HarmonyBus MIDI processing are independent.

`src/hb_virtual_midi.[ch]` owns the four-byte injected USB-MIDI packet format so
the conductor-monitor and follower-render paths share the same wire contract.

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

## Implementation artifacts

The `hb-virtual-tracks` branch currently carries the integration as small,
separable pieces:

- `scripts/make_movy_harmonybus_profile.py` -- merges source tracks 5-8 into a
  Movy `ui-state.json` without replacing unrelated chains.
- `patches/harmonybus-conductor-monitor.patch` -- gives a Conductor instance the
  same injected-note output mechanism Followers already use, with no harmonic
  remapping of the conductor note.
- `patches/schwung-movy-hb-source-audio-mute.patch` -- changes Mute on tracks
  5-8 to Movy's hosted-chain mixer mute, leaving sequencer MIDI running.
- `patches/schwung-movy-hb-two-bank-navigation.patch` -- limits user-facing
  focus/navigation to two groups of four without changing Movy's 16-track engine
  ABI.

The first source-audio-mute prototype intentionally does not put source-bank
mute gestures into Movy's undo history. The mixer mute itself is persisted in
the chain `mix` value and restored on set load. Undo can be added after hardware
routing is validated, with an explicit cache-update hook rather than risking an
undo operation that changes audio while leaving the Mute LED stale.

## First hardware acceptance test

1. Confirm Movy's first four tracks are still hosted by Schwung/native Move.
2. Apply the three overlay patches and generate/merge the source-bank profile.
3. Select virtual track 6.
4. Unmute 6 and mute native track 2.
5. Record C-D-E-F; hear track 6's source instrument and confirm the notes land
   in the track-6 clip.
6. Stop recording, mute 6, and unmute 2.
7. Play the clip; HarmonyBus renders the stored notes into MIDI channel 2 and
   Move track 2 sounds them.
8. With 6 still source-audio-muted, play live on it; rendered notes must sound
   on track 2 immediately.
9. Switch clips on track 6; rendered musical content changes.
10. Switch clips on track 2; its native overdubs/automation change independently.
11. Select source track 5 and verify its accepted conductor notes also monitor
    unchanged on MIDI channel 3 while harmony inference continues normally.
