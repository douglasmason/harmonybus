# Follower note paths (0.2.139)

Input roles are relative to the follower root. With Explicit C, G is always
fifth. Neither current harmony, lookahead nor master transpose changes that
input reference. An explicit follower scale refines ambiguous chromatic degrees
(e.g. F-sharp is #4 in C Lydian and b5 in C Locrian). Infer uses stable input
interval buckets; inference still chooses the output parent scale.

New instances and Movy hbclean.52 fresh follower tracks use **Scale** content.
Saved sets retain their chosen content.

## Travel

- **Relative** carries the input degree into the target harmony's chord-scale.
- **Closest Split** chooses nearby output pitches within the selected split
  groups, jointly avoiding exact MIDI-note collisions. It preserves group
  membership rather than degree number. A fifth input can therefore render a
  seventh with Harm./Out or 1357/246. Choose 135/2467 to keep 1/3/5 together,
  or Relative when the exact degree should survive.
- **Closest Split Chromatic** uses the same mapping for diatonic inputs. A chromatic
  input becomes a semitone below the rendered next-higher diatonic input.

Travel and role transforms are solved in the reference key. Master transpose
is applied exactly once to the final pitch, after the output role is determined.
It changes neither input nor output role. At MIDI range limits, output folds by
octaves to preserve that role instead of clamping to an unrelated pitch class.

## Note-path pages

**Foll Trk 1-2 / 3-4** have two rows per page. The global follower pages and
per-track conductor page were removed in 0.2.139; **Cond All** remains:

| Raw Note | Input Role | Output Role | Rendered |
|---|---|---|---|
| G4 | 5th | b7 | G4 |

The example is a G input relative to C, rendered as G over A minor seventh.
Each row belongs to a single source note on the selected follower instance.
Use **Cond All** to inspect the combined conductor notes and harmony.

The output role is relative to the harmony captured when that note was mapped,
including lookahead. It does not relabel a held note against a newer harmony
when Retrigger Held is off. Pending notes show `--` until mapped. Generated
chords show their first voice followed by `+N` additional voices. Operation
owners supply their emitted pitch where available; these pages never rerun
probability or consume a performance gesture.

## Lookahead Anti Buffer

**Next Harm → Lookahead Anti Buffer** is global and independent of both
Lookahead and Follower Buffer. It supports milliseconds and note divisions.
The initial value is **25 ms**; **0 ms** restores the previous timing.

For positive lookahead:

`effective advance = max(0, lookahead − anti-buffer)`

In 4/4, with the next chord at beat 1 of the following bar, **3/4** lookahead
normally starts at beat 2. With a 100 ms anti-buffer, beat 2 and the following
100 ms retain the previous effective harmony. At 120 BPM, a **1/16** anti-buffer
instead moves that start one sixteenth note later (125 ms).

With a nonzero anti-buffer, harmonic pre-capture cannot bypass this start
boundary. Follower Buffer still controls Quant Grid capture; a note explicitly
quantized across the boundary uses the harmony at its release. Notes are not
held for the anti-buffer: only the harmony-selection boundary moves.

Negative lookahead keeps its existing late-harmony behaviour. Off remains Off.
An anti-buffer wider than positive lookahead clamps to the actual harmonic
boundary rather than making it late. Saved state includes the two lookahead
settings; stale per-track state restores cannot undo a live global edit.
