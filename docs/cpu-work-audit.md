# Shared timeline CPU audit — HarmonyBus 0.2.264

The Movy audio-block barrier calls `hb_movy_refresh` once before track rendering. Previously every track tick called it again, rescanning all instances, rebuilding the cache key and hashing each conductor timeline. This repeated global work scales quadratically with track count. The profiler did not report a recursion cycle in this workload.

The fix skips only the redundant tick refresh when the existing conductor-block barrier is active. New block barriers and incoming MIDI still refresh state. Hosts without that barrier retain per-tick refreshes. No new cache, allocation, thread, host change or musical mapping policy is introduced.

## Reproduction

Run serially, outside other builds:

```sh
cc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 tests/shared_work_bench.c src/harmony_core.c -lm -o /tmp/shared_work_bench
/tmp/shared_work_bench 16 20000
```

For a baseline comparison, use this same benchmark source against the 0.2.263 production sources. The fixture uses up to four conductors and twelve followers, active key travel, moving clip clocks and no incoming notes. It exercises the production API block barrier and track ticks. It does not render an instrument or emulate Move scheduling.

Three alternating baseline/modified runs with GCC `-O2`, without profiling instrumentation, produced these desktop timings (microseconds per block):

| Tracks | 0.2.263 runs | Modified runs |
| --- | --- | --- |
| 1 | 3.29, 3.34, 3.53 | 2.48, 2.43, 2.67 |
| 4 | 18.28, 19.47, 18.29 | 8.35, 8.13, 8.21 |
| 16 | 92.16, 91.49, 96.29 | 47.69, 51.17, 46.39 |

A separate instrumented 10,000-block, 16-track run reduced refresh calls from 170,001 to 10,001 and conductor timeline hashes from 680,004 to 40,004. The extra initial call comes from fixture setup. The new `cpu_work_test` checks one refresh per block, duplicate barrier suppression, metadata changes, immediate MIDI visibility and standalone fallback. Existing clip timeline/cache, follower boundary, harmony override, chord player, shared context and key travel suites passed locally.

These results establish reduced CPU work, not a confirmed device crackle cure. Remaining all-instance chord/arp and key-operation synchronization passes are separate audit candidates; their state/order semantics need verification before changing them. No performance claim is made about the proprietary instrument DSP, host callback scheduling, output driver, or physical audio.

## Device check

Update only HarmonyBus to 0.2.264 with Movy hbclean.184 already installed, restart, and play the same set normally. No preparation, long ABC diagnostic or photos are needed for this change. Report whether crackling and pad delay improve, remain, or worsen. Keep the existing diagnostics available for future targeted measurements.
