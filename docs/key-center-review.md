# Key-center, closest mapping, and timing review

Review branch: `fix/key-center-timing-voicing`.
Implementation commits: `4591b32` and `b17a53f`.
The combined HarmonyBus 0.2.256 release includes the Next Pulse Off work (`6c764ce`, PR #42). The validation below records the desktop review before the release's device-package CI. Physical Move testing remains outstanding. Compatible with Movy hbclean.172; only HarmonyBus needs updating.

## Musical contract

Resolve a conductor's recorded harmonic function in the destination key before choosing its voicing. Proximity must not select a different progression root. Relative preserves corresponding voice roles; Closest can permute those roles and choose octaves to retain the original register.

Conductor voicing and follower closest mapping use the same assignment engine in `src/closest_split.h`. Each input has an allowed destination pool. For split mappings, chord-tone and non-chord-tone pools are hard boundaries: diversity cannot justify crossing them. Inside the permitted pools, maximize available distinct pitch classes, then minimize movement and register spill. The existing follower six-semitone limit remains a constraint; boundary fallback stays in the same pool. A complete generated conductor chord additionally supplies exact pitch-class multiplicities, preserving its required voices and extensions.

For C-major Dm–G–C–Am, the intended root progression remains degrees 2–5–1–6. In D major this gives Em–A–D–Bm. In A natural minor, with Dominant to Minor set to Harmonic Minor, it gives Bdim–E–Am–F. Chord qualities and the sixth degree's accidental follow the destination mode; the functional positions are retained. With sevenths this becomes Bm7b5–E7–Am7–Fmaj7.

A tested closest inversion is recorded D4–F4–A4 becoming D4–F4–B4, a Bdim/D voicing. The third input moves two semitones while the other two stay in place. The semantic root remains B; inversion must not make the footer call this a D-root chord.

The source note/gesture continues to own its release. A key or policy change after note-on must not change the pitch used by its eventual note-off. Explicit inversions and played bass/top constraints remain intentional.

## Reproduced defects and changes

1. **Conductor roots were snapped to previous harmony.** Through the production MIDI API, activating Closest Chord Tone in unchanged C major turned a Dm gesture into C major (`0x224` expected, `0x091` emitted). The root is now resolved in the destination parent key before applying closest voicing. Conductor single-note mapping retains its relative pitch class rather than snapping to unrelated chord/scale tones.
2. **A preceding dominant contaminated the next chord.** The seventh-chord progression test reproduced a minor-major seventh on the destination tonic: the previous V's harmonic-minor collection was still being used for the new tonic. Raw conductor chords now determine their own functional family rather than inheriting the preceding bus chord's palette during key reinterpretation.
3. **Closest diversity was only a small cost discount.** Independent collisions could win despite enough legal pitch classes being available. The shared assignment now maximizes feasible diversity within its allowed pools, while exact capacities preserve complete conductor voicings. Source/target group-size differences can still require reuse; no extra output voice is invented to conceal that.
4. **Single-entry caches were repeatedly displaced.** Current/next harmony previews and multiple register rows replaced each other's assignments. The reviewed pending fix retains 16 exact assignments per instance, including all inputs that affect the result. Only pure assignment results are retained from private previews, leaving live gestures and ownership untouched. Nearest pitch-class octaves are computed directly instead of scanning all 128 MIDI notes.
5. **Inactive operations caused unnecessary work.** Timing arguments were evaluated before inactive harmony/secondary/approach lanes rejected themselves. The active checks now precede those reads. Post-render harmony/parent-scale resolution is deferred until a relevant secondary, cadence, or rotation actually needs it.
6. **The footer used source-key chords against the destination key.** Current and next footer chords now pass through the same key-harmony resolver used for musical context before their Roman-numeral inputs are sent to Movy. Tests check roots and complete masks across major/minor destinations and every saved key-travel policy.

## Validation

- All 63 registered native suites passed, including undefined-behavior sanitizer coverage in the repository's configured suites.
- Added production-path progression coverage across 12 destination tonics, major/natural-minor modes, four key-travel policies, and triad/seventh forms: 768 rendered chord cases.
- Tested concrete inversion, preserved voice multiplicities, MIDI bounds, note-off pairing after a second key/policy change, class restrictions, required-capacity cache separation, and exact cached/fresh agreement.
- Re-ran the expanded footer test and follower reference/transposition tests after the final class-preserving boundary fallback change.
- The full-lookahead benchmark asserts zero additional solver calls on repeated unchanged snapshots after warming the cache.
- Pulse-widget and CI-orchestration tests passed.

The desktop full-lookahead benchmark used 32 visible chromatic pitches, an active C-to-A-minor key context, two alternating learned harmonies, trails off, and 50 repeated reads. Compared with the checked-out 0.2.255 main branch, average Closest Split pad-read times fell from approximately 666 to 484 microseconds with Auto Chord off, and from 637 to 447 microseconds with Scale Degree Auto Chord: about 27–30% less time. These are informational desktop measurements, not Move audio-deadline measurements.

## Remaining limits

The available Schwung host source services parameter reads in the pre-transfer path alongside audio/MIDI work, so repeated preview computation can consume time needed for that hardware cycle. This is a concrete mechanism for timing pressure, but the review has no physical Move underrun trace. The changes reduce reproduced CPU work; they do not establish that all audible crackling is eliminated.

Complete chord inversion is available when HarmonyBus has the generated voicing together, including modern recordings that retain one Auto Chord input. Older baked notes arriving individually retain their relative pitch class and choose its nearest octave. This patch does not add buffering to reconstruct a simultaneous chord from those events or pretend it knows their grouping on an unlearned first pass.

Release publication requires the combined branch to pass device-package CI. After installing, compare the user's affected set with Key Center off/on, Relative/Closest Split, full lookahead, held notes through a key change, and rapid repeated notes. The desktop tests cannot substitute for that physical timing evidence.
