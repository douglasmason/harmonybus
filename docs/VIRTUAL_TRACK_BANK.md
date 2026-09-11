# HarmonyBus virtual source bank

## Layout

Native Move/Schwung tracks remain the destination bank:

| Track | Purpose |
|---|---|
| 1 | Native drums |
| 2 | Destination for source track 6 |
| 3 | Destination for source track 7 and conductor monitor |
| 4 | Destination for source track 8 |

Movy hosts the source bank:

| Track | Role | Default output |
|---|---|---|
| 5 | Conductor | unchanged monitor -> MIDI ch3 |
| 6 | Follower | HarmonyBus render -> MIDI ch2 |
| 7 | Follower | HarmonyBus render -> MIDI ch3 |
| 8 | Follower | HarmonyBus render -> MIDI ch4 |

Movy-internal hosted-chain MIDI is explicitly accepted by HarmonyBus on MIDI
channel 1. Source tracks can therefore be identified without relying on the
native Move track fallback logic.

## Routing invariant

Every source event fans out independently to:

1. the source track's local instrument, and
2. HarmonyBus.

Source-track Mute is audio-only. It must never suppress MIDI from reaching
HarmonyBus. This lets the user record/hear the source instrument first, then mute
its audio and hear the same live/clip MIDI through the native destination track.

## Fresh-set behavior

The dedicated HarmonyBus/Movy overlay creates the source bank automatically for
a fresh set:

- tracks 1-4 are forced to the native/Schwung host,
- tracks 5-8 are Movy-hosted source tracks,
- each source track gets HarmonyBus in `midi_fx1`,
- each source track gets a lightweight Plaits synth for pre-render monitoring,
- track 5 starts as Conductor -> ch3 monitor,
- tracks 6/7/8 start as Followers -> ch2/ch3/ch4.

Existing Movy sets are not silently rewritten. The standalone profile generator
remains available when an existing set should be converted deliberately.

## Integration artifacts

- `scripts/prepare_movy_harmonybus.sh` applies all Movy overlays to a clean Movy
  checkout and runs its TypeScript typecheck.
- `scripts/make_movy_harmonybus_profile.py` merges tracks 5-8 into an existing
  Movy `ui-state.json` without replacing unrelated chains.
- `patches/harmonybus-conductor-monitor.patch` makes Conductor `Render To Ch`
  mean monitor destination.
- `patches/schwung-movy-hb-source-audio-mute.patch` makes Mute on tracks 5-8
  toggle Movy's chain mixer audio mute instead of sequencer MIDI mute.
- `patches/schwung-movy-hb-two-bank-navigation.patch` exposes only the 1-4 and
  5-8 track groups. Movy's existing Session/Clip `+/-` routing already changes
  groups, so no new hardware-button mapping is introduced.
- `patches/schwung-movy-hb-auto-source-bank.patch` seeds a fresh set with the
  four HarmonyBus source chains and keeps tracks 1-4 native.

The first source-audio-mute prototype intentionally does not put those mute
gestures into Movy's undo history. The mixer mute itself persists through the
existing chain `mix` state. Undo should be added only with an explicit cache
update hook so audio state and Mute LEDs cannot diverge.

## First hardware test

1. Build/install the HarmonyBus module with the conductor-monitor hook applied.
2. Prepare/build the dedicated Movy overlay using `scripts/prepare_movy_harmonybus.sh`.
3. Open a fresh set and verify `+/-` in Clip/Session view switches only 1-4 <-> 5-8.
4. Verify tracks 1-4 remain native and tracks 5-8 appear already populated.
5. Select source track 6, unmute 6, mute native track 2, and record C-D-E-F.
6. Confirm track 6's local synth is heard and the notes are stored in its clip.
7. Mute source track 6 and unmute native track 2.
8. Play the clip and verify HarmonyBus renders it to MIDI channel 2 / Move track 2.
9. While 6 remains source-audio-muted, play it live and verify track 2 sounds immediately.
10. Switch source clips on 6 and verify rendered note content changes.
11. Switch native clips on 2 and verify their overdubs/automation remain independent.
12. Select track 5 and verify conductor notes monitor on MIDI ch3 while harmony inference continues normally.
