# Common-tone continuity

Retrigger Held now offers **Off**, **On**, and **Preserve Common**. Existing saved values and defaults are unchanged.

Preserve Common follows live harmonic changes with the existing chord owner and MIDI diff. In Together playback, a pitch already sounding on the same channel stays sounding; only departing pitches receive note-off and new pitches receive note-on. For example, C–E–G becoming C–F–G keeps C and G held. This is exact pitch continuity, not pitch-class matching or a new inversion algorithm.

Fresh pad attacks, arpeggio, strum, baked rhythms and rhythmic gates retain their articulation. Existing release-tail policy and voice ownership remain authoritative. No musical objects are converted to stored MIDI, and no allocation or second scheduler is added to harmonic refresh.

The scoped flag exists only during revoicing an existing owner and is restored immediately. The existing bounded MIDI flush handles output backpressure. The setting persists in the existing hb16 field; no wire format is added.

Validation includes production DSP tests for unchanged common pitches, legacy retrigger behavior, capacity-one output, release/replay and state round-trip. Desktop checks do not establish audible crackle performance on a physical Move.
