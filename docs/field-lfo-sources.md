# Field channels as LFO sources — candidate integration

This candidate requires the patched Schwung host as well as HarmonyBus and
Movy. Updating only the two module repositories cannot update the chain DSP
or master-FX engine. The host patch is pinned to Schwung commit
586aebd4367952384e1b447e8e3a2fd627974ec8 and built by
`.github/workflows/field-lfo-host-candidate.yml`. No device deployment occurs.

The existing Shape parameter gains Field 1 through Field 16 (values 6–21).
Existing assignment, target, signed depth and polarity remain in use.
Field speed, placement and override controls remain in the Field panel.
Local oscillator rate, phase and retrigger do not affect a shared Field source.
Movy offers Field choices only when the destination host reports support.

HB publishes all sixteen effective outputs, including raw pad/pressure/knob
holds and latches, once per host audio block, from whichever active HB instance ticks first. Exact motif probability
overrides are not raw field outputs. Each channel is carried in one lock-free
64-bit word with its timestamp. Readers neither allocate nor wait. Host-owned
exports are resolved at module initialization; no host-API layout changes or
per-chain context pointers are involved. Expired samples (250 ms) cease
modulation. Destroying the final HB instance clears publication immediately.

The host starts a new field epoch before every SPI audio block. Only the first
active HB instance advances the field in that epoch; bypassing one instance
therefore does not stop other instances. Samples published in a block become
visible together at the next block boundary, giving every assignment the same
channel value regardless of track render order. Bypassing all HB instances
expires the last signal. A second independently loaded HB image cannot publish
over a live owner; ordinary tracks share one loaded HB image.

Slot synth/FX, sends, LFO-to-LFO and the separate master path consume the same
source before applying their existing routing. Cross-LFO modulation now uses a
one-block delay and restores base settings before returning from the tick, so
modulation is not saved as a parameter edit. This corrects an observed upstream
bug where depth 0.200 remained at 0.700 after modulation was disabled.

Validated locally: HB publication/override/expiry/unload tests; real chain DSP
synth base/effective values and disable; real chain cross-LFO base preservation;
both cross-LFO directions and polarity; real master FX signal and disable/expiry restoration; send offset/disable;
Movy TypeScript checking, browser build and logic tests. Full host ARM build,
complete musical regressions, UI behavior, performance and hardware validation
remain release gates. This is not a published installable release.
