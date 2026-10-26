# Audible chord range — HarmonyBus 0.2.298

Range edits now retune the currently sounding chord on the next processing tick, including a comp track whose Retrigger Held setting is Off. The conductor's range is published before inherited follower ranges are resolved. An explicit comp range still overrides the conductor default. Raw melody remains unchanged.

Existing note ownership supplies the release and replacement notes. This does not enqueue or record another chord, consume the next queued chord, or reset the current arp/rhythm gate or division. An unchanged range emits nothing. Releasing the original input still releases the retuned notes.

Movy 258 sends each range encoder increment immediately. Turning knob 8 keeps the existing protection against accidental Harmony Off; a deliberate tap still turns harmony off. Chord-specific overrides, global defaults and Shift+touch reset keep their previous meanings.

Native regressions inspect the emitted MIDI for conductor and comp rendering, held arpeggio retuning, inherited/explicit ranges, no-op changes and release ownership. Existing queue and full native suites remain release gates.
