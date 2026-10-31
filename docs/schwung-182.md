# Schwung 1.8.2 host adaptation

Schwung 1.7.3 and 1.8.2 expose the same plugin host vtable. The only change
to `plugin_api_v1.h` is the documented behavior of `get_bpm`: 1.8.x prefers
Move's exact tempo, otherwise a held measurement of the running clock.
See the [upstream release](https://github.com/charlesvestal/schwung/releases/tag/v1.8.0)
and the [full integration audit](https://github.com/douglasmason/harmonybus-movy/blob/main/docs/schwung-182.md).

HarmonyBus now uses `hb_host_bpm()` as its single optional-host tempo boundary.
It preserves valid fractional BPM from 20 through 999 and returns the established
120 BPM default for missing callbacks, NaN, infinity or out-of-contract values.
Lookahead confirmation, buffer conversion, chord timing, motion, Field and the
display use the same validation. Previously several consumers only checked
`<= 0`, allowing NaN to poison motion or release timing.

There is no smoothing, second transport, version-string check or song-reader
dependency. Movy's abstract clip clock remains authoritative when present.
Native Schwung p-lock/song-reader recovery in 1.8.2 remains host-owned and is
independent of HB operations and Movy abstract musical automation.

`host_tempo_test.c` drives the production plugin with missing/invalid callbacks
and valid fractional BPM, including the varying readings possible on 1.7.3.
It checks actual timing consumers, not just the validator. Both exact host
headers also pass Movy's ABI parity check. This does not measure physical audio
latency, crackling, or device scheduler cadence; no Move installation is made.
