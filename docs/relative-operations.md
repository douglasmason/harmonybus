# Relative operations and six/eight-note scales — HarmonyBus 0.2.304

Install with Movy **0.34.1-hbclean.272**. Both repositories are needed: Movy
loads the separately installed HarmonyBus DSP.

## Controls

- **Follower Scale / Parallel Scale:** Half-Whole Diminished and Whole-Half
  Diminished are now selectable. Movy's keyboard labels are Dim H-W / Dim W-H.
- **Operation:** Smart Below and Smart Above are available in ordinary user
  operation lanes as well as Harm Play bank/row assignments.
- **Motif Edit > Motif Tools > Relative Operation:** choose Captured, Smart Below
  or Smart Above. With a note step selected, this edits that step and supports
  Undo. It also sets the choice for newly entered steps. Captured retains the
  recorded operation; Smart steps reevaluate their approach at each playback.

An eight-pad target row uses degree `p % n` and octave `floor(p / n)`, where `p`
is the zero-based pad index and `n` is the scale's cardinality:

| Scale size | Pads 1–8 |
| --- | --- |
| Six | 1, 2, 3, 4, 5, 6, 1 up an octave, 2 up an octave |
| Seven | 1, 2, 3, 4, 5, 6, 7, 1 up an octave |
| Eight | 1, 2, 3, 4, 5, 6, 7, 8 |

## Pitch and harmonic meaning

A **relative operation (RO)** resolves a root, harmonic quality and destination
chord-scale. Leading Tone remains a diminished operation below the target.
Connector Below remains a separate operation using the Chromatic Keys quality
policy. Neither is renamed or replaced. Upper Dim likewise differs from
Connector Above and Tritone Sub.

Smart Below prefers LT; Smart Above prefers Upper Dim. When the preferred
adjacent semitone is in the effective target collection and the opposite
semitone is outside it, the selector switches to the other complete operation.
Otherwise it keeps its preference; at MIDI boundaries it uses the available side.

In A harmonic minor, both preferences yield F-sharp Upper Dim on F and G LT on
G-sharp. This exposes both missing pitches in the augmented-second gap. The
policy is local and deterministic; it does not promise a unique chromatic row
for every arbitrary collection.

Eight-note input degrees and numbered parent-degree operations use ordered
scale positions. Degree 8 is preserved through the input bridge and saved-note
projection. It is not silently treated as a seventh or tonic. Chord forms retain
their separate harmonic roles: a half-whole rotation supplies a dominant core
(0, 4, 7, 10); a whole-half rotation supplies a diminished core (0, 3, 6, 9).
A pad can rotate the collection into either family. Explicit chord quality still
takes precedence. Chord accommodation adds required tones without deleting an
arbitrary one of eight degrees to fit a seven-degree rule.

## Recording and authored motifs

Live bank and lane choices resolve once at note onset. The chosen existing LT or
Upper Dim operation is captured; later harmony changes or assignment edits do
not rewrite that outcome. Preview uses the same selector on a private instance.
Smart does not force chord mode on.

Authored motifs store a selector separately from captured pitch/quality intent.
The motif renderer resolves it against the current destination after key mapping,
then uses the existing chord, voicing, scheduling and ownership code. Automatic,
tap and bank/row motif playback share that renderer.

**Motif to Clip offers Fixed (default) and Adaptive.** Fixed names the resolved
LT / Upper Dim steps and stores simultaneous per-target choices. Adaptive keeps
Smart Below/Above for resolution against each playback target and harmony. Both
retain input notes and use the ordinary native renderer. Event/transfer limits
reject oversized exports before any clip changes. The source motif remains
adaptive. See [Fixed and Adaptive export](adaptive-motif-export.md).

## Compatibility and centralized definitions

Existing operation IDs, six-bit recorded codes and scale IDs are unchanged.
New native scale IDs are 20/21, keyboard IDs 23/24, and Parallel IDs 21/22.
The legacy Relative Major/Minor Parallel ID 18 is retained.

The 63 nonzero six-bit recorded token codes are already occupied. Smart bank
selectors use internal IDs 4096/4097; Smart lane IDs 67/68 are settings identities.
Neither is packed into that recorded token field. Live capture emits existing
operation IDs 51/52 and tokens 14/60.

Motif libraries containing adaptive steps use version **mf3**, with an explicit
selector field per event. Old mf1/mf2 libraries load without reinterpretation;
libraries containing only captured steps still save as mf2. Truncated or invalid
adaptive libraries cannot partially replace the current library.

- `data/operations.json` generates the native identities and Movy operation menu.
- `data/scales.json` generates HB masks/options, Movy TypeScript options and the
  Rust recording/projection scale table. Rust input, shared-context, chord-command
  and recorded-action validators use the generated counts.
- Existing scale-role, chord-quality, key-travel, voicing and note-ownership
  resolvers remain authoritative. The catalog does not duplicate those engines.
- Redundant parameter references were removed from three hierarchy panels; knob
  assignments and child links remain intact, keeping the install manifest below
  the host's 64 KiB limit.

Regenerate/check both catalogs:

```sh
python3 scripts/build_scale_catalog.py --movy ../harmonybus-movy
python3 scripts/build_operation_catalog.py --movy ../harmonybus-movy
python3 scripts/build_motion_metadata.py
python3 scripts/build_scale_catalog.py --check --movy ../harmonybus-movy
python3 scripts/build_operation_catalog.py --check --movy ../harmonybus-movy
```

Regression coverage includes all-key pad geometry and degree mapping, stable IDs,
chord-role policy, saved scale validation, Smart lane onset/capture, adaptive
motif key changes, legacy migration, atomic mf3 loading, draft Undo, simultaneous
per-target clip export, and the existing rendering/ownership suites.

## Motif editing

The motif editor now offers every supported catalog recipe as a reversible override. Captured restores the original event; LT/UD and CCB/CCA retain their distinct harmonic meanings. See [Motif Relative Operation editor](motif-rpo-editor.md).
