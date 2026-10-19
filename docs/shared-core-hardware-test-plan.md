# Shared core / FM1 USB MIDI candidate: approval required before installation

## Source and capability audit

HarmonyBus baseline: 30397ae7eb876500fcb5d4794252e3e1da7c8064.
Movy integration baseline: feature/grid-sequencer 1e6c65a3ba8e3186d974633b2218ebf47af5458e
(same checked-out content as local 4fa1ce6). Upstream Movy 5c01a4fe7745b730ee98111194fc64328ea2659b;
Schwung compatibility target 911136d75fb9e4bacfdd5cdeecb9377c2e2553fb (1.7.3).
Latest Schwung tree inspected: 586aebd4367952384e1b447e8e3a2fd627974ec8;
no host update is included. Existing dev/local processor inspected at 0a6dcc3;
it already serializes commands into one native DSP process and should be reused.

FM1 Open b178ac43c25f010c60a878056fb5331fe693fd62 advertises V13, Windows
PowerShell/JieLi PI32v2 clang 4.0.1 r3 firmware builds and WASI SDK 27 web builds.
The SDK exposes control-task MIDI send/post, 27 key bits, 7 encoder deltas,
240x240 display, on/off LEDs, and audio overrun counters. The native synth engine
is binary. Source must comply with GPLv3 and the SDK linking exception.
These are Open SDK capabilities, not proof of stock-firmware MIDI mappings.
Do not infer RGB LEDs, arbitrary display control over MIDI, or stock knob CCs.
No FM1 firmware has been changed, built, installed or flashed here.

## Approval scope

Approve one candidate HarmonyBus module and one candidate Movy module on the
specific Move, plus read-only capture of stock FM1 USB MIDI. Keep current module
archives and a backup of the test set. Record Move firmware, Schwung version,
FM1 firmware/version, USB descriptors and chosen MIDI channel in the report.
No host upgrade, FM1 update or firmware reset is part of this approval.

## Connection and stock mapping gate

Use Move's USB-A host port to FM1 USB-C with a data cable; use a suitable USB hub
if Launchpad is connected too. Verify enumeration and power stability physically.
Do not assume Move USB-C can host FM1. Capture raw press/release and controller
messages first. Confirm a dedicated channel. Start FM1 MIDI channel OFF; select
its verified channel in Movy after capture. Launchpad reserves channel 1 while
on; FM1 refuses that collision. If stock firmware cannot select a distinct
channel, test FM1 alone; concurrent use requires a verified upstream remapper
or future host device identity support. The host currently exposes channel/cable,
not reliable physical device identity to this adapter.

Only note-on/off and poly pressure are adapted. CCs, sustain, pitch bend,
transport and program changes are not FM1 controls in this candidate. Do not
claim a controller mapping without a stock capture. No LED or SysEx is sent by
this adapter. Unplug detection is not exposed: use FM1 OFF to release held notes
before/after cable removal; confirm recovery in this test.

## Musical acceptance matrix

1. Adapter OFF: compare saved set state and recorded output to prior modules.
2. FM1 alone: all keys, velocities, rapid repeats, zero-velocity releases,
   chords, long holds and track changes while held. Every onset has its original
   release; no doubled native Move echo. Record and Capture reproduce input.
3. Run conductor auto-chords, followers, key-center Relative/Closest Split,
   dominant palettes, arp/release tails, motif and Joint Play tests already in
   the musical suite. Compare rendered MIDI to the baseline, including stop.
4. FM1 plus Launchpad: same and different pitches on the same and different
   tracks; release one owner, disable either adapter, change selected track,
   enter/leave parked mode. Remaining owner must continue; final release stops.
5. Reject channel conflicts without stealing Launchpad's remap. Unload restores
   adapter-owned claims. Test loss/recovery of host acknowledgement and unplug.
6. Remote UI remains a client of the same processor. Test source namespace 2
   with session-scoped cancellation before exposing it through a network API.

## Performance gate

Run identical 1/4/16-track workloads on prior and candidate builds: idle, dense
MIDI, key center + Closest Split, arp + recording, then Launchpad feedback and
Remote UI separately and together. Collect existing callback wall/CPU timing,
overruns/late callbacks, MIDI input-to-render latency (p50/p95/p99/max), repeated
hit counts and output audio. Warm up, alternate baseline/candidate, and run the
known crackle case for at least 10 minutes. Record USB/LED queue refusals and
surface batch sizes. No synchronous display polling in MIDI input handlers.

Reject on stuck notes, missing/doubled attacks, altered recorded state, new
callback overruns or audible regressions. Desktop timings do not establish
Move deadlines or a crackle cure. Agree a latency budget from the measured
baseline before claiming hardware performance acceptance. Roll back both
candidate modules on failure; do not change firmware to work around a failure.

## Later firmware gate

Only after stock USB validation: confirm exact board revision, recovery and
stock restore procedure, memory/linker limits, audio interrupt call graph and
RAM placement, MIDI transport behavior, and SDK license/source-delivery terms.
Build/test in the simulator, then draft a separate FM1-specific flash plan and
seek explicit approval. SDK header availability alone is insufficient.
