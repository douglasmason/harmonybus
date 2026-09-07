# Harmony Bus

Experimental Schwung MIDI FX for Ableton Move.

Harmony Bus uses a **Conductor / Follower** model:

- a Conductor instance infers harmony from incoming notes and publishes it to a shared in-process bus;
- Follower instances keep their own rhythm/register, but remap their pitches against the current harmony;
- follower modes: **Transpose**, **Chord**, and **Nearest**;
- root policies: **Explicit**, **Current Input Root**, and **Auto-Infer**.

## Install with Schwung Manager

In Schwung Manager use:

**Custom → From GitHub URL → `douglasmason/harmonybus`**

Manager reads `release.json` and downloads the release asset.

## Status

Version 0.1.0 is an experimental ARM64 module candidate. It has passed host-side tests and ELF/architecture checks, but physical-Move loading is still being validated. Test it on a disposable Set first.

## Build

```bash
./scripts/build_harmonybus_move.sh
```

The output is `dist/harmonybus-module.tar.gz`.

## License

MIT.
