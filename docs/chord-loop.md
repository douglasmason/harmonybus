# Chord loop and automatic touch phrases — 0.2.300

Pair with Movy hbclean.260. Chord names no longer expose the internal explicit-tone flag as a trailing plus. Minor, diminished, augmented, extended and slash chords retain their musical names.

The existing queue status snapshot includes the complete loop in entry order and its actual playback cursor. Reading status never advances the queue. Repeated identical chords retain separate positions. The UI displays short loops in full and follows the current chord through longer loops.

Separate taps use Queue Depth as the start/refill threshold. Overlapping knob or step-button touches automatically form a phrase: releasing the final touch commits it, even below numeric depth. Touch Phrase is also the first depth option, for explicitly committing every completed gesture, including one-note taps. The numeric default remains 2; maximum capacity remains 16.

A new batch waits until the entry touch releases before arming, including at depth 1. Already-running playback and loop timing continue. Stop and Harmony Off clear the entry state; recording still captures scheduled activations rather than input gestures.

This release also includes the previously merged Euclidean Fill rhythm editor. No device firmware update is required.

Validation covers native queue scheduling, duplicate cursors, append/wrap transitions, read-only and short-buffer status, automatic phrase completion, separate taps, musical chord labels and the full native/ARM release checks. Movy verifies the real shared controller, display layout and production audio bridge separately.
