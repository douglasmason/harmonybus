# Field performance operations — HarmonyBus 0.2.325

Assign these operations to the usual sixteen slots. They appear in Movy's
**Field** operation group and use the shared Move, Launchpad and Remote UI
controls, including approach-row pressure assignments.

| Operation | Amount | Meaning |
| --- | --- | --- |
| Field Rate | -400 to 400 percent; default 200 | 100 normal, 0 freeze, negative reverse |
| Field Zoom | 25 to 400 percent; default 200 | 100 normal, 200 twice the Field zoom factor |
| Field Output | 0 to 100 percent; default 100 | Slot N drives Field output N |

Hold for momentary action; use the usual persistent latch to keep it active.
A touch-knob tap arms the next live-note phrase. Its first note starts the
operation; overlapping handoffs continue it until all live notes are released.
Clip playback and generated chord voices cannot consume the arm. Cancel, stop,
and preset restore use the existing gesture cleanup. Auto-enabled lanes retain
their normal configured behavior.

Pressure uses the existing row mapping, including its response, reverse and
hold modes. Rate and Zoom interpolate from normal (100 percent) toward Amount;
Output interpolates from zero. Existing field low/high mapping can specify a
different pressure range. Zero pressure releases the mapped contribution.

Field is global. Broadcasting one operation to several tracks must not multiply
its effect repeatedly. Among active Rate or Zoom operations, the largest
percentage deviation from 100 wins; equal deviations use stable instance/slot
order. Output uses the highest active value for that numbered output. Releasing
one owner preserves other active owners. Contributions are resolved once per
Field clock block; reading the UI does not advance the clock.

Operations are temporary contributions. Rate and Zoom multiply the current
leased Field gesture, within the existing limits. A direct Field output contact
or legacy Field latch takes priority over an Output operation. Releasing the
direct contact reveals the still-active operation. Ending the operation reveals
the current underlying Field value, using the existing glide. No saved base
setting is overwritten. Operations on a bypassed Movy-hosted chain stop
contributing when its bypass heartbeat is observed.

Existing Field terrain/output views and their leased input protocols remain
compatible. This first migration exposes their musical actions through shared
operation slots; it does not replace the old views' controls. Use the shared
operation cancel/latch-off to end an operation, and the Field view's clear to
clear its direct output overrides.

Field outputs can already feed supported synth/FX modulation routes and
sequence-position drivers. For example, assign Field Output to slot 1, choose
Field 1 in the stock LFO source control, and target filter cutoff. The existing
modulation route preserves the synth parameter's base, including edits made
while the operation is active.

Regression coverage includes phrase boundaries, momentary/latch controls,
pressure, direct-contact priority, multiple owners, preset restoration,
bypass, single-clock behavior, actual synth modulation and compiled browser
Remote input. Physical hardware feel remains untested.
