# Motif to Clip

Use Movy hbclean.178 with HarmonyBus 0.2.261. The **Motif to Clip** panel is available in Steps, Perform and Harm Play.

1. Choose a stock motif body or User 1–16.
2. Play the intended target pad, then touch **Target Input** to capture its input note. Turn that knob to adjust the MIDI note. The current track's key-center and rendering settings still determine what that input sounds like.
3. Choose Parent or Parallel approach collection. Parallel uses the existing Parallel Scale setting and records that borrowing as note intent; it does not turn on the live Parallel operation.
4. Choose the rhythmic unit and pattern. As Entered scales the captured durations; the other patterns reshape them into the selected units.
5. Choose target placement: Saved, End, Start, Both or Omit. The motif body excludes all leading/trailing targets. Targets inside the body remain. Saved uses the motif's original boundary placement.
6. Enable Even Units to extend a boundary target until the phrase occupies an even number of units. It does not add another attack. Omit adds a rest instead.
7. Touch **Write to Clip** to review the actual event progression, target input, length and input-note count. Turn a knob to scroll a long progression. Touch knobs 1–4 for Cancel, Replace/Write, Append or Overdub.

Replace replaces the selected clip's notes, recorded operations, shared harmony events, automation locks and trig conditions. It retains clip speed, quantization and transpose settings. Append begins after the existing loop end and extends that loop. Overdub starts at the current loop start and retains existing material. Each successful write is one normal Undo action. Clip length, note capacity, recording state and the preview's clip identity are checked before writing; an invalid write leaves the clip intact.

Notes are stored as input notes with their velocities, harmonic operations and captured input context. Stock motifs follow current Auto Chord settings. User motifs retain captured operation words and chord intent. The writer does not bake a generated chord voicing or change the track's role/key.

**Motif Targets** exposes the same target placement, even-unit padding and rhythm controls for live motif playback. Harm Play Settings also includes target placement on its eighth knob. In live playback, padding uses the motif's beat units (then its Half/Double span); clip loading uses the chosen Unit. Spatial Approach layouts keep the target pad separate and omit the ending target; Start/Both can explicitly include an opening target.

Existing clip tokens retain their legacy event identities and timings. Motif storage uses a versioned normalized body, independent boundary-target settings and an index map for those old tokens. Old mf1 libraries migrate when loaded; saving uses mf2.
