# Within-snapshot pad preview reuse

Status: promoted for 0.2.259 after extended equivalence checks; physical Move profiling remains necessary.

Render visible pad slots before the twelve pitch-class probes, keeping each result for this one read. An exact match of source note, approach shift and approach row reuses the single rendered voice, expanded output mask, gap mask and trail target. The cache is discarded after the read; no cross-frame invalidation is required. The live renderer and note ownership are unchanged.

Validation so far:

- 8,064 complete snapshots are byte-identical to 0.2.258 under UBSan: eight travel modes, three Auto Chord modes, trails on/off, four ordinary/duplicate/approach/key-center layouts and seven harmony-color modes, with active transpose/random operations, held approaches, stock motifs, playback ownership and trail history.
- Three alternating desktop benchmark runs produced a median per-case cost ratio of 0.893 across 24 configurations (about 11% less work). The worst case median ratio was 0.977. These are desktop wall times, not Move underrun measurements.
- The native suite now compares cached and uncached traversal within each scenario. Physical Move validation is still required to establish hardware headroom.

Reproduce the comparison with `python3 scripts/compare_pad_previews.py PATH_TO_0.2.258_CHECKOUT`. Build and run `tests/pad_refresh_bench.c` against both checkouts at `-O2` for the timing comparison.

Further candidates: reduce the roughly 700 KB private instance copy and measure UI parameter servicing alongside the audio callback. Do not cache whole snapshots across time without tracking every rendering dependency.
