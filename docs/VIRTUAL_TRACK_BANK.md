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

The dedicated HarmonyBus/Movy integration creates the source bank automatically
for a fresh set:

- tracks 1-4 are forced to the native/Schwung host,
- tracks 5-8 are Movy-hosted source tracks,
- each source track gets HarmonyBus in `midi_fx1`,
- each source track gets a lightweight Plaits synth for pre-render monitoring,
- track 5 starts as Conductor -> ch3 monitor,
- tracks 6/7/8 start as Followers -> ch2/ch3/ch4.

Existing Movy sets are not silently rewritten. The standalone profile generator
remains available when an existing set should be converted deliberately.

## Canonical integration mechanism

The integration intentionally uses exact, idempotent source transformations
instead of carrying a fork or maintaining line-number-sensitive patch files.
If either upstream codebase changes an integration seam, the transform fails
loudly rather than modifying a nearby block heuristically.

- `scripts/apply_movy_harmonybus.py` transforms the three Movy seams:
  source-track audio-only mute, two-bank 1-4 <-> 5-8 navigation, and fresh-set
  source-bank creation.
- `scripts/prepare_movy_harmonybus.sh` applies that transform to a clean Movy
  checkout, installs dependencies, typechecks, and builds the device bundle.
- `scripts/apply_conductor_monitor.py` adds the Conductor monitor injection to a
  HarmonyBus DSP source copy. `Render To Ch` is the monitor destination when the
  instance role is Conductor; Followers keep transformed-render semantics.
- `scripts/make_movy_harmonybus_profile.py` converts an existing Movy
  `ui-state.json` deliberately without replacing unrelated chains.

CI clones current upstream Movy, applies the integration transform twice to prove
idempotence, runs TypeScript typecheck, and completes `build:device`. It also
applies the conductor transform twice against the current HarmonyBus DSP.

The first source-audio-mute implementation intentionally does not put those mute
gestures into Movy's undo history. The mixer mute itself persists through the
existing chain `mix` state. Undo should be added only with an explicit cache
update hook so audio state and Mute LEDs cannot diverge.

## First hardware test

1. Build/install the HarmonyBus module with the conductor-monitor transform applied.
2. Prepare/build the dedicated Movy integration using `scripts/prepare_movy_harmonybus.sh`.
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
