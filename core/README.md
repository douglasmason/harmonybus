# HarmonyBus core, incremental extraction 1

Canonical source package, kept in the existing HarmonyBus repository to avoid
introducing another release and synchronization mechanism during extraction.
Consumers pin a Git commit. `src/` compatibility includes preserve existing
Schwung build commands and public symbols; they contain no musical implementation.

`include/harmony_core.h` and `src/harmony_core.c`: unchanged chord inference,
contextual refinement, chord masks, transposition and voice mapping.
`include/closest_split.h`: unchanged bounded assignment and caller-owned cache.
`include/rhythm_pattern.h`: unchanged rhythm vocabulary and weights.
`surfaces/input.ts`: bounded shared command transport and stock MIDI decoder.

Build C with CMake or compile core/src/harmony_core.c with core/include. No audio,
USB, filesystem, threads, wall-clock or host API enters the portable C package.
The classifier uses libc string/formatting functions and bounded automatic arrays;
this is portable C, not yet a certified interrupt-safe FM1 firmware component.

The existing HarmonyBus instance and bus retain sole ownership of musical state.
Movy owns transport, clips and recording; surfaces own physical addressing and
pending transport messages. Extracting the remaining stateful key, operation,
arp and motif code requires an explicit context API and differential tests first.
Do not instantiate a second HarmonyBus bus for a new surface.

Move uses the existing pad route; Launchpad and FM1 share SurfaceInput and the
engine's source-aware SurfaceRoute. Source addresses are LP 0..127, FM1 128..255,
Remote UI 256..383. Release fences are per source; cross-source pitch aliases use
one engine ownership table. Existing Remote UI/local processor integration can
send the same surface_events/surface_release_source commands to its single native
processor. No new HTTP service or duplicated JavaScript music engine is added.

Move/Launchpad retain their current layouts and previews. FM1 is a chromatic MIDI
keyboard into the selected track, preserving incoming pitch and velocity. Its
input is transformed by the same HarmonyBus instance as other track input. It
has no guessed CC, LED, screen, SysEx, auto-detection or firmware dependency.
