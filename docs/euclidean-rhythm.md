# Euclidean Fill for the shared rhythm library

Euclidean Fill creates an evenly distributed attack pattern in **one of the four existing global User rhythm slots**. It does not create a new sequencer, transport clock, arp mode, MIDI path, or note owner.

## Workflow

1. In **Rhythm Pattern Edit**, choose **Edit Pattern** (User 1–4).
2. Open the adjacent **Euclidean Fill** panel.
3. Set draft **Euclid Steps** to 1–16 divisions, **Euclid Hits** to 0–16 and **Euclid Rotate** to 0–15. The effective number of hits is bounded by the selected length; rotation wraps by the selected length.
4. Read **Euclid Preview**. `X` is an attack; `.` is a rest. All three draft controls and the preview leave the existing pattern untouched and trigger no notes.
5. Select **Apply** on **Fill Pattern** to replace only the chosen slot's attack/rest positions. Fill Pattern immediately reads back **Ready**, allowing another explicit application.
6. Select User 1–4 on the existing **Rhythm Generate** or Joint Play actor rhythm panel to use the result.

Zero rotation starts with an attack when Hits > 0. Increasing rotation moves the attacks later. As an example, 3 of 8 at rotation 1 previews `.X..X..X`. Zero hits makes all rests; hits >= length makes all attacks.

The operation changes only each active step's **Attack/Rest** status. Existing accent and gate values are preserved; a pre-existing Tie step at a generated rest becomes Rest and at a generated hit becomes Attack. On Apply, the slot length becomes Euclid Steps. Unused steps beyond the new length are left untouched.

Changes are shared globally in the same way as manual User rhythm edits. The pattern is saved through the existing `rg1` state; the generator's draft Steps, Hits and Rotate values use an optional `eg1` suffix, preserving compatibility when loading older saves. Applying is an explicit overwrite of a user slot, not a separate undo transaction. Save a valued pattern elsewhere before overwriting it.

## Runtime integration

The existing `hb_rg_advance` clock copies its pattern once at the start of each rhythmic cycle. A newly applied user pattern is picked up on the next cycle and does not cut off currently sounding gates. The existing attack, gate, note-off and ownership rules continue unchanged. All chord, motif, note-pool arp, performer travel, Source Weaving and generated clip pathways continue to consume the ordinary shared rhythm pattern.

The generator performs bounded constant-memory arithmetic (at most 16 steps) when a user explicitly applies it. Preview costs at most 16 integer calculations per UI read. It performs **no added work in the audio callback**, no allocation and no background polling.

## Validation

`tests/rhythm_generator_test.c` exercises all step counts, hit counts and rotations; cyclic spacing; zero and full attack patterns; preservation of accents and gates; preview-without-write; explicit application; shared slots; and save/restore compatibility.

This increment makes a new way to *construct* patterns, not a new live rhythm randomizer. It also does not yet add source-pitch permutation (Peal), lock-and-vary for clip pitch, or a variation history. Those remain separate follow-up choices.
