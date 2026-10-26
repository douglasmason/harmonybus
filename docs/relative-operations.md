# Relative operations: shared definitions and Smart approaches

A relative operation resolves a root, a harmonic quality and a destination
chord-scale. LT is a leading-tone diminished operation; Connector Below is a
different operation with the Chromatic Keys quality policy. Sharing a root offset
does not make those operations interchangeable.

## Implementation plan

1. Centralize stable operation IDs, labels, aliases, root rules, base qualities,
   approach choices and recorded tokens in `data/operations.json`. Generate native
   definitions and the Movy menu catalog; check both against that source in CI.
2. Keep the existing live/motif chord resolver. Route the existing operation
   identity conversions through the catalog rather than add another engine.
3. Add Smart Below / Smart Above as **bank/row selectors**. Resolve them to existing
   LT / Upper Dim operations at onset. Preserve both the chosen root and harmonic
   interpretation in the existing recording format. Preview uses the same choice.
4. Verify old IDs, aliases, presets, chord qualities, ownership and playback; test
   the Smart pair across transpositions, harmonic-minor/major and augmented scales.
5. Follow up with authored adaptive motif events, ordinary operation lanes and
   additional selectable eight-note scales. Their format and harmonic degree
   contracts must be extended explicitly; do not reinterpret an existing ID.

## Scale-cardinality work

Keep one cardinality-aware pad calculation: for a zero-based pad index `p` and
`n` ordered scale degrees, use degree `p % n` and octave `floor(p / n)`.
The existing Movy mapper already uses this rule. Six-note rows therefore display
`1 2 3 4 5 6 1↑ 2↑`; eight-note rows display `1 2 3 4 5 6 7 8`.
Whole Tone and Augmented are currently selectable six-note HB collections.
Eight-note input presets still need to be added to the shared scale catalog.

Before adding those presets, define their parent-degree chord stacking and
relative-operation interpretation. In particular, a numbered scale degree in an
octatonic collection must not silently inherit a seven-note functional label.
Test every tonic, row wrap, alias target, preview and saved scale ID. Preserve
existing scale IDs and generate both HB and Movy choices from the scale catalog.

For adaptive authored motifs, version the event representation so a step can
store a selector separately from the concrete operation captured in a live
performance. Then reuse this selector/resolver in playback and ordinary motion
lanes, with old motif/preset migration and round-trip tests. The current compact
token field has no free operation codes; do not borrow a used code or extra flag.

## Smart selector contract

Smart Below prefers LT; Smart Above prefers Upper Dim. If the preferred adjacent
pitch is already in the effective target scale and the other adjacent pitch is
outside it, select the other **complete operation**. Otherwise keep the preferred
operation. At the MIDI boundary use the available side. Empty/unknown scale
information keeps the preferred operation. The decision uses the target after
travel/key mapping and the same effective collection used by approach rendering.

In A harmonic minor, Smart Below on F selects F-sharp Upper Dim, and on G-sharp
selects G LT. Both chromatic notes in the augmented-second gap are accessible.
The policy is local and deterministic: it does not promise a globally unique or
complete chromatic row for arbitrary collections. Both/neither adjacent pitch
being in the scale is deliberately not resolved by unpredictable history.

Smart selectors are additional bank choices; existing assignments and LT,
Connector Below/Above, Upper Dim and Tritone Sub retain their behavior. The
selected operation uses the existing chord form, voicing, arp and note ownership.
The selector itself does not force chord mode on.

## Persistence and boundaries

All existing motion IDs, approach choice indices and wire tokens remain fixed.
The 63 nonzero six-bit token codes are already occupied by operations and motif
references. Smart selectors therefore use internal-only IDs outside the recorded
token range, stored only in bank settings. Live input resolves them before action
capture; recorded notes keep an ordinary LT or Upper Dim token and relative
target, not an absolute rendered MIDI note. Replay is independent of subsequent
bank edits. Adaptive authored motif steps require a separate future format change.

Generate/check catalogs with:

```sh
python3 scripts/build_operation_catalog.py --movy ../harmonybus-movy
python3 scripts/build_operation_catalog.py --check --movy ../harmonybus-movy
```

The catalog describes existing operation identities and root/base-quality rules.
Destination-scale policies, borrowed harmony, key travel and voicing remain in
their existing shared resolvers. This avoids encoding the entire engine in data.

Native trigger-to-role/modifier conversions also read the catalog. Runtime
identity lookups use fixed indexed tables; JSON parsing and generation happen
only during development. Catalog drift is checked in both repositories' CI.

Validation covers stable IDs and aliases; missing pitch coverage for both Smart
preferences in five collections and all twelve transpositions; live/preview
chord equality; recording and motif capture; bank persistence; and the four
newer concrete tokens (59, 61–63) previously misclassified as motifs in preview.
