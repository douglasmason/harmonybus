> Release 0.2.118 applies master transpose to recorded conductor playback as well as live and recorded followers. New generated conductor recordings store reference-key voices; detection and playback transpose once. Existing four-bar controls and Receiver routing are preserved.





# Harmony Bus

Experimental Schwung MIDI FX for Ableton Move.

Harmony Bus uses a **Conductor / Follower / Receiver** model:

- a Conductor instance infers harmony from incoming notes and publishes it to a shared in-process bus;
- Receiver instances deliver already-rendered notes on their selected channel to the next instrument, without remapping or rebroadcasting;
- Follower instances keep their own rhythm/register, but remap their pitches against the current harmony;
- follower content and travel settings control which tones are allowed and how input notes map to them;
- follower source-root policies: **Infer Input**, **Infer Notes**, and **Explicit**.

## Install with Schwung Manager

In Schwung Manager use:

**Custom → From GitHub URL → `douglasmason/harmonybus`**

Manager reads `release.json` and downloads the release asset.

## Timing and current release

HarmonyBus **0.2.118** makes Follower Buffer global with a **1/16-note** default for
fresh settings. Millisecond choices are 0, 25, 50, then 100 to 1000 in 50 ms steps. The same control also offers tempo-relative durations from 1/64
to 2 Bars. Existing saved values survive; the first restored legacy copy becomes
the shared value. Change it once and save to make all snapshots agree.

Already-on-grid notes remain on-grid even when the buffer spans a full interval.
Lookahead remains Off by default. Use Movy **0.34.1-hbclean.25** for the new explicit
Quantize + Fill Gaps clip operation and prepared 1/16-note defaults.

Read the canonical [timing guide](docs/timing-guide.md), with editable
[SVG diagrams](docs/timing). CI generates the PDF from those sources and attaches
it to each release. The PDF is an export, not a separately maintained document.
Native tests cover timing, mapping, state restoration and complete module loading;
physical Move behavior still needs device verification.

To rebuild the guide (Linux with `fonts-dejavu-core` installed):

```bash
python3 -m pip install -r scripts/docs-requirements.txt
python3 scripts/build_timing_guide.py
```

## Build

```bash
./scripts/build_harmonybus_move.sh
```

The output is `dist/harmonybus-v0.2.118-module.tar.gz`.

## License

MIT.

HB 0.2.104 adds negative lookahead (late harmony) with the capture window before the shifted boundary, and resolves recognized two-note Movy voicings before due followers. Lookahead stays Off by default; new sets use a 1/16-note global buffer. See the [timing guide](https://github.com/douglasmason/harmonybus/blob/main/docs/timing-guide.md).

### Auto Chord and Arp / Strum (0.2.118)

Per-track Auto Chord provides Off, Scale Degree and Conductor Chord, with selectable power, triad, seventh, ninth, add9, sixth, 6/9, eleventh, thirteenth and suspended forms, key-selected or explicit inversion, and Close / Root + Fifth Low / Alternate Up / Shell voicings. Arp / Strum provides Together, Repeat Arp and Once; Momentary/Latch; five orders; musical rates and gate lengths; and ms or musical strum spreads. Both generators are inactive by default. Use Movy hbclean.31 to record and replay rendered conductor chords.

Generation happens after follower-buffer release, before arpeggiation. Generated gestures keep their initial voicing until released or replaced. Chord Mode Off with an active arp uses raw input pitches, plus transpose; existing Content/Travel/Approach mapping applies when both generators are inactive. See the [canonical guide](docs/timing-guide.md) for inversion examples, control locations, latch and cancellation semantics.

Dominant Scale (Off / Harmonic Minor / Melodic Minor / Altered V) substitutes the output collection on functional V or raised-vii diminished harmony relative to the configured tonic. It applies to ordinary follower scale mapping and generated extensions, while preserving source-degree labels and recognized chord tones. The guide details triggers, altered-scale spelling and held-gesture behavior.
