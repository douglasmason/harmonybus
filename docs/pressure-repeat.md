# Pressure note repeat — HarmonyBus 0.2.315

Pair with Movy hbclean.338. **Note Repeat** selects 1, 2, 4, 8 or 16 repetitions
per quarter-note beat. **Repeat Divisions** allows every integer from 1 to 16.
Max Repeats / Beat limits the pressure range, defaulting to 16. Both operations
reuse the approach-row pressure modes and the existing owned-note repeat scheduler.

Positive pressure selects a tempo division with hysteresis; zero pauses repeats.
The pressure panel reports the actual division. Rate changes take effect on the
next division boundary without restarting the input note. Generated notes keep
the source attack's pitch/velocity and use a 50% gate. Held chord voices repeat
together. Reverse and both latch modes reuse the existing normalized gesture.

Source note-off cancels only that seed's repeats. A latched rate does not retain
released notes. Stop, source changes and restore use existing cleanup. No MIDI
input recursion, extra player, audio-thread allocation or unbounded catch-up loop
is added. Each source/lane owns at most one of the existing bounded burst slots.
The disabled path avoids scanning the voice and burst pools.

Use **Auto Off** for pressure-only activation. These are live operations, like
Chord + Arp; rate gestures are not frozen into onset snapshots or restored as live
notes. Continuous fractional rates and recorded pressure envelopes remain future
work. See the [Movy guide](https://github.com/douglasmason/harmonybus-movy/blob/main/docs/pressure-repeat.md)
for setup and gesture semantics.
