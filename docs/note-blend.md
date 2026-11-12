# Pressure-weighted Note Blend — HarmonyBus 0.2.323

On the **Humanize / Tools** panel, knob 8 is **Note Blend**. It is per track,
saved with the track, and defaults to Off for new and older sessions. The same
parameter is available in the shared Remote UI. Launchpad X pressure and Move's
pad-pressure bridge feed the same engine path as Remote UI note pads.

| Setting | Result |
| --- | --- |
| Off | Normal polyphonic notes and poly pressure |
| Steps | One voice follows the pressure-weighted pitch in semitone steps |
| Bend 2 / 12 / 24 / 48 | One continuous voice, with the indicated ±semitone bend range |

Hold C4 and C5 with equal pressure: the target is F♯4. Change their pressure to
1:3 and the target becomes A4. Initial note velocity supplies each weight until
its first pressure update. A zero pressure value leaves the gate held; when all
weights are zero, the last target remains. Pitch approaches the target with a
25 ms smoothing time constant. Steps adds hysteresis to avoid boundary chatter.

The blend acts after harmony mapping, operation pitch changes, rhythm and final
Global Transpose. It includes all sounding notes on a channel, including chord
voices, playback and generated notes; it is not a separate finger-only layer.
Pressure drives pitch weighting while blending instead of also forwarding a
second poly-pressure gesture. Duplicate sources mapped to one output pitch share
the existing gate and latest pressure. Upstream delays and generated tails retain
their existing pressure behavior.

Both local sound and Render To use the blend. Internal Receiver tracks now carry
the full 14-bit bend value. Use a **dedicated destination channel/instrument** for
each continuous blend: ordinary MIDI pitch bend affects every voice on that
channel. This is monophonic channel bend, not MPE output or per-finger voice
allocation. The existing Remote UI cloud X/Y and MPE-input surfaces remain separate.

**Set the destination synth's pitch-bend range to match.** HB does not send RPN
messages or change the synth's saved range. For the browser's NuSaw, set its Bend
Range to 1.0 and select HB Bend 12; NuSaw's normalized control spans 0–12 semitones.
Stock Move instruments do not accept MIDI pitch bend; use Steps with those sounds.
Launchpad Mini has no pressure, so notes use their attack weights. Remote devices
without real pressure likewise use whatever pressure their existing input supplies.

The first note anchors a continuous voice. Releasing that pad while another stays
held preserves the voice; releasing the last note sends OFF and centers bend.
Crossing the selected bend span reanchors with an OFF/ON, so wide sweeps need an
appropriately wide synth range to avoid rearticulation. Switching Off restores
the still-held polyphonic notes and centers bend. Stop, panic, role/routing changes
and Receiver source loss drain releases before later notes. Bend updates are
emitted only when their 14-bit value changes; the audio path allocates no memory.

Regression tests cover pressure ratios, zero pressure, channel isolation, smoothing
at different block sizes, step hysteresis, reanchoring, mode changes, stored state,
transpose, one-message output buffers and cleanup. Production-chain tests cover
Move, Launchpad/Remote packets, local and Receiver output. Browser tests exercise
the compiled engine and NuSaw PCM. Physical hardware feel still needs auditioning.

Hardware reference: [Ableton: Using MIDI with Move](https://help.ableton.com/hc/en-us/articles/14661164865308-Using-MIDI-with-Move).
