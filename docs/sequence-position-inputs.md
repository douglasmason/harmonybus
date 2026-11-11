# HarmonyBus 0.2.316 — Sequence position inputs

HB exposes two per-instance numeric controls, **Sequence Input 1** and **Sequence Input 2**, under **Sequence Position Inputs** in its shared native/Remote UI metadata. Each ranges from 0 to 1 and defaults to 0.5. They are modulation inputs, not operations or a second oscillator.

In Movy hbclean.341, select **Input 1** or **Input 2** as an Instrument or HB Render sequence position driver, and assign the existing stock LFO to the corresponding HB input. A bipolar LFO at full depth around a base of 0.5 scans the whole sequence. Its existing shape, sync, rate and depth controls apply. Direct **Field 1–16** drivers reuse the shared Perlin Field and its existing pad overrides; they do not need these inputs.

The compact read-only `sequence_inputs` response is `sp1,input1,input2`. It returns the current effective values after host modulation, without moving the LFO or Field. Input values save per instance; the stock host's normal base-value serialization keeps active modulation out of saved settings. Legacy states reset the inputs to 0.5. Invalid and non-finite writes are ignored.

The inputs appear early in canonical/runtime parameter metadata so the stock host's bounded 256-entry modulation cache can resolve them. Empty optional leaf-panel `params` arrays are omitted to keep the manifest below the stock loader's 64 KiB limit. Knob assignments and panel contents are unchanged.

Validation includes actual HB loaded inside the unmodified pinned Schwung chain: LFO modulation, effective/base separation, neutral repeated reads, state round-trip, instance isolation, and existing Field route lifecycle/stress checks. No Schwung host modification is required. This release does not add pitch glide or MPE output.
