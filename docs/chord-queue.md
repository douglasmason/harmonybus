# Queued Chord Play

The conductor owns one retained progression. New sessions default to Queued
entry and Loop. Explicitly saved Immediate or Hold preferences remain respected.
Queue Depth (1–16, default 2) controls when a batch starts; it is not the maximum
progression length. Touch Phrase commits on the last release of an overlapping
knob/step gesture. A partial initial batch waits. When committed while stopped,
its first chord sounds; Play advances the remaining chords on Queue Time.

Hold reaches the final chord and waits for another batch. New choices append
to the retained progression, including after earlier batches finish. Loop repeats
the retained progression. Neither policy imposes a sixteen-chord limit. Storage
grows with available memory; if a growth allocation fails, the new choice is
rejected without changing the existing progression or cursor.

## Controls and lifetime

- Sample switches Immediate (bright) / Queued (dim), even with an empty queue.
- Loop switches Hold / Loop. It is bright for Loop, dim for Hold, and off when
  there is no queue. Shift+Loop retriggers the retained progression.
- Mute bypasses/reactivates the queue. It is bright only while the queue is
  muted, dim while active, and off with no queue. The queue continues its
  timeline while bypassed; reactivation adopts the current chord without rewind.
- X and the existing Range / Tap Off control explicitly cancel the queue.
  X and Copy are available/dim only while a queue exists.

Stop silences the live selection and preserves the progression. Play restarts
an assembled progression at its first chord, in both Hold and Loop. A partial
initial batch still obeys its arming rule. If an already assembled progression
has an unfinished appended batch, Stop/Play includes those retained choices.
Changing input modes, focus, entry preference or end policy does not clear the
queue. Explicit cancellation, Set teardown or conflicting conductor ownership
retires it.

Lookahead, pad prediction and motif destinations read the same scheduled native
queue. Incomplete refills are not advertised as scheduled harmony. Recording
writes actual chord activations into the conductor clip, never enqueue gestures.
Range edits update stored choices in place without an extra attack. Only the
currently sounding history entry is mutable; older history remains a timeline.

## Storage, scheduling and feedback

The native queue stores one growable progression. Pending entries are a suffix
view, so taking a chord advances a pointer instead of shifting all future entries.
Storage grows geometrically, beginning at 64 entries, on explicit enqueue edits.
There is no allocation in ordinary take, loop, retrigger or status reads. Host
parameter edits can still share the audio service thread; this does not claim
that allocation during an enqueue is hard real time or that device crackles are
resolved. Cancellation releases the storage.

Only the next 65 deadlines are materialized, matching the existing bounded
history horizon. Taking a chord replenishes that window. Loop/retrigger and
transport/timing changes reschedule at most 65 entries, regardless of retained
length. Current and delayed harmony reads and boundary searches use this window.
The history remains bounded at 65 actual activations.

Footer snapshots map at most 32 chord names using separate preview caches, so
feedback does not evict the musical key-map cache. Short progressions retain the
`cq1` protocol. Long progressions use `cq2`: the same ten header fields (pending
count, next beat, current name, next name, result, waiting, depth, loop, absolute
cursor, total retained count), followed by a zero-based window start and at most
32 names. The window follows the cursor; it never changes playback. Result 2
means the latest choice could not be stored. Consumers validate the window and
use the absolute cursor/total for counts. All retained entries remain replayable.

`hb_chord_queue_diagnostics` remains the read-only `qd1` snapshot: services,
activations, recoveries, lateChanges, clears, rearms, maxLateBeats, lastDueBeat,
lastActualBeat and owner. Lifetime counters survive explicit queue cancellation.
Lateness is in beats, not wall time; it cannot establish audio glitch causality.

## Validation

The native regression exercises 4,096 distinct queued activations, allocation
failure, bounded deadline/preview windows, continued Hold entry, a 4,099-chord
replay and retrigger, and verifies that playback does not allocate. Existing
coverage includes late service, 2,048 loop changes, pause/resume, bypass,
lookahead, cache isolation, range edits and conductor ownership. The Movy
production-chain fixture covers a 64-chord Hold queue, appended refill and
66-chord restart with balanced MIDI releases, in addition to its 128-bar loop
fixture. Controller tests cover long-queue counts, cursor visibility and button
availability. Physical Move audio and display verification remain necessary.
