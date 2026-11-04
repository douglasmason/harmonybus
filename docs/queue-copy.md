# Native queue export for Copy Queue

HarmonyBus **0.2.312** pairs with Movy **0.34.1-hbclean.318** to save a completed
retained progression directly to an empty conductor clip. Press Copy on the
Chord Play or Joint chord-bank surface, then choose an empty clip with step
keys 1–8 or Jog. The live queue keeps playing; X/K8 remains its explicit release.

The read-only `hb_chord_queue_export` getter exports native chord descriptors,
source pitches and durations derived from the same boundary resolver used by
queue playback. It preserves scale, intent and register without expanding
generated MIDI voices. A content/rhythm token detects changes during destination
selection. The getter does not advance, consume, clear or allocate the queue.

Only complete phrases fitting Movy's existing 16-bar/512-note clip format can
be copied. Unsupported timing, oversized queues and incomplete phrases return
an error without altering retained content. Native tests cover descriptor
fidelity, repeatable identity, bounded replies and allocation-free reads.
Movy's paired tests cover atomic editing, Undo, save/reload and real playback.
