# Chord range and display isolation

HarmonyBus 0.2.301 supports a chosen top chord tone through the existing Played Top Note voicing routine. Legacy octave ranges and saved selections retain their meaning; new top ranges are carried by versioned selection descriptors without changing their original note owner. Held selections and generated follower chords retune without adding queued attacks or restarting arp timing. Key mapping preserves the selected top ceiling.

Chord-bank display reads cache each of the 21 slot decodes and key transformations separately from the musical transformation cache. This removes repeat display work during sustained playback. Desktop measurements show lower polling cost; they do not establish that Move crackles are fixed.

Validation covers cold/warm display parity across scales, transposes and key policies; successive audible inversions; key changes; original note releases; arp phase and gates; save/load; descriptor validation; and existing native regressions. The Movy integration additionally checks queued recording and replay through the production Rust/native chain.

Voicing adds Root Below alongside the existing choices. It adds the nearest root below the lowest voice only when that voice is not already the root. Upper notes and the selected or played top remain unchanged. It does not add another octave to a root-position voicing. At the MIDI floor, it omits an unavailable lower root. Existing saved voicing values and defaults retain their meanings.
