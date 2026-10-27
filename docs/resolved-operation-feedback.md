# Resolved operation feedback — HarmonyBus 0.2.306

Install HarmonyBus 0.2.306 with Movy 0.34.1-hbclean.274. Movy loads the separately
installed HB DSP. This pair includes the preceding Chord Discovery release.

While playing, the existing performance footer briefly shows the target,
concrete relative operation and rendered result. For example, in A harmonic
minor, Smart Below or Smart Above on F can show `F > UD F#dim7`; on G-sharp it
can show `G# > LT Gdim7`. Native accidental preferences determine note spelling.
With chord generation off, the result is a note without an invented quality.

The feedback uses resolved roots and rendered chord tones captured by the
native renderer. Chord inversions do not change the displayed root. Key and
Parallel mapping are reflected in the result. A motif's future steps do not
replace the current feedback while being scheduled: each step reports when it
plays. Live and recorded inputs retain their existing ownership and resolution.
It reports the latest resolved target, not a list of all simultaneous notes.
Chord quality describes the generated voicing; later motion transformations
remain independent.

Smart conductor selection now keeps the source harmony and its parent scale in
the same key before mapping them once to the destination. This fixes a case
where a key change could choose LT while the corresponding mapped motif chose
Upper Dim for the same target.
Raw notes and arpeggiators now retain that concrete choice in their onset
snapshot through the same capture routine used by recording. Later generated
notes cannot choose again using an already mapped output pitch as their input.

The compact `ro1` section travels in the existing `pad_view` snapshot. It contains
an event serial, concrete operation token, target/root pitch classes, rendered
pitch mask, chord flag, origin and native note spellings. The audio path copies
bounded numeric data; the getter reads one atomic snapshot and formats it.
Reading feedback does not resolve operations, advance motif/row cursors, or
consume modifiers. Pad previews use private instances.

Movy displays a new event for 1.8 seconds, then restores the harmony footer.
Plain notes and stop clear previous operation feedback. Track switches establish
a fresh baseline rather than replaying an old result. Missing or malformed
feedback from another native version cannot invalidate the rest of the pad view.
There are no extra host reads, controls, or manifest parameters.

Operation short labels now come from `data/operations.json` across the bank
panel, resolved feedback and motif-to-clip preview. Connector Below is `CCB`,
Leading Tone is `LT`, and Upper Dim is `UD`; their different harmonic meanings
are preserved. The clip preview no longer labels Connector Below as LT.

This feedback increment originally fixed motif-to-clip choices at write time.
HarmonyBus 0.2.309 and Movy hbclean.282 add optional [Adaptive export](adaptive-motif-export.md),
while preserving Fixed as the default and the existing relative-operation IDs.

Validation covers raw and chord playback, conductor and follower roles, both
Smart preferences, concrete recorded replay, key/Parallel changes, same-context
pad preview agreement, scheduled motif timing, read isolation, stop clearing,
footer expiry and track ownership, plus the complete native and controller suites.
