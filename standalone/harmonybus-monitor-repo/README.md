# HarmonyBus Monitor

Source-aware cable-2 note monitor companion for HarmonyBus on Ableton Move / Schwung.

This tool follows the same monitoring pattern as ChordDex: it receives Move's played-note stream through the source-aware plugin API, keeps per-channel active-note state, and publishes that state through shared memory for the HarmonyBus MIDI FX.

## Install

Using Schwung's installer:

```bash
./install.sh install-module-github douglasmason/harmonybus-monitor
```

After installation, restart Move or rescan Schwung modules. HarmonyBus Monitor should appear in the Tools list.

## Build

```bash
bash scripts/build.sh
```

The release artifact is `dist/harmonybus-monitor-v0.1.91-tool.tar.gz`.
