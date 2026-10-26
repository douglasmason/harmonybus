# Implied harmony quality — HarmonyBus 0.2.302

Harmony has an expressed chord identity and an implied family. The expressed
identity describes the selected tones (major triad, power chord, sus, seventh,
shell, and so on). The implied family describes the resolved chord scale: the
available melody notes viewed from that chord's root. Chord Play's quality hue
uses this implied family; its chord name continues to describe the expression.

For example, G, G5 and Gsus4 with C-major context all imply a dominant family:
the root-relative collection contains B (the major third) and F (the minor
seventh). G5 with G-major context instead implies major. Changing the voicing,
range or inversion does not change that contextual character.

The portable core derives the family from a root and resolved scale. It does
not store a second editable quality, infer a replacement scale, change notes,
or alter chord recognition. Chord Play supplies the same resolved collection
already used by its live and recorded descriptors, including explicit scales,
borrowed/secondary harmonies and key-center transformations.

| Implied family | Chord Play hue |
| --- | --- |
| Major | Amber |
| Minor | Blue |
| Dominant | Coral |
| Dissonant implied 1-3-5 (including diminished and augmented) | Magenta |
| Unresolved | Grey |

Magenta is the shared color for dissonance already within the implied 1-3-5,
before considering the seventh. Whole-tone and altered scales therefore use
magenta even when their third/seventh imply dominant character. A half-whole
dominant scale retains an ordinary major triad and uses coral: its flat fifth
can remain a sharp-eleventh tension alongside the available perfect fifth.
Lydian's sharp fourth and harmonic major's flat sixth likewise do not override
a present perfect fifth. Locrian and Locrian natural 2 imply diminished;
melodic and harmonic minor remain minor. Lydian augmented implies augmented.
A missing root, insufficient collection,
or an ambiguous collection such as the chromatic scale remains unresolved.
These broad display families do not claim a unique tonal function for every
possible scale.

Chromatic brightness still describes the selected chord's tones relative to
the shared reference scale. Active harmony remains white. Intent + Scale and
Scale Only retain their existing meanings. Knobs and step buttons consume one
native classification, with no additional audio callback work or polling.

The chord queue already exposes known upcoming harmonies before its first
loop completes. Regression coverage now explicitly checks first-pass Loop
readiness and immediately switching lookahead Off/On or changing its amount.
Unpredictable recorded material still follows the existing eligibility rules.

Validation covers 27 scale/ambiguity cases in all twelve keys, voicing-independent
classification, resolved Home/Parallel/Blues banks, saved descriptors, the
read-only bank snapshot, and queue lookahead. Physical LED appearance and
device audio require hardware verification.
