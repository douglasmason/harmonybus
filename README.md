# Harmony Bus

Experimental Schwung MIDI FX for Ableton Move.

Harmony Bus uses a **Conductor / Follower** model:

- a Conductor instance infers harmony from incoming notes and publishes it to a shared in-process bus;
- Follower instances keep their own rhythm/register, but remap their pitches against the current harmony;
- follower content and travel settings control which tones are allowed and how input notes map to them;
- follower source-root policies: **Infer Input**, **Infer Notes**, and **Explicit**.

## Install with Schwung Manager

In Schwung Manager use:

**Custom → From GitHub URL → `douglasmason/harmonybus`**

Manager reads `release.json` and downloads the release asset.

## Timing and current release

HarmonyBus **0.2.102** makes Follower Buffer global with a **350 ms** default for
fresh settings. The same control also offers tempo-relative durations from 1/64
to 2 Bars. Existing saved values survive; the first restored legacy copy becomes
the shared value. Change it once and save to make all snapshots agree.

Already-on-grid notes remain on-grid even when the buffer spans a full interval.
Lookahead remains Off by default. Use Movy **0.34.1-hbclean.24** for the new explicit
Quantize + Fill Gaps clip operation and prepared 350 ms defaults.

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

The output is `dist/harmonybus-v0.2.102-module.tar.gz`.

## License

MIT.
