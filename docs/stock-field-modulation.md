# Field modulation on stock Schwung: feasibility increment

This branch keeps Schwung unchanged. It does not depend on the separate
`work/field-lfo` custom-host experiment, modify release.json, or publish a release.
The prototype is compiled only with `HB_STOCK_FIELD_MOD`; ordinary builds retain
no routing UI, configuration, or runtime work from this experiment.

## Implemented

One experimental route per HB instance uses the existing host `mod_emit_value`
and `mod_clear_source` callbacks. It samples the existing shared Field channel
and its output override, then sends a normalized bipolar signal to the stock
modulation engine. Depth and polarity use stock range scaling. No parameter
values are directly overwritten. Each instance captures its own callback context
at creation; the init/create handoff is thread-local. Source IDs distinguish HB
instances, including multiple instances in one chain.

Test-only parameter protocol:

- `field_mod_route = channel,depth,bipolar,target,param`
- Channels 1–16; depth -10000 through 10000; bipolar 0 or 1.
- Targets: same-chain `synth`, `fx1` through `fx8`.
- Example: `1,5000,1,synth,gain`.
- `field_mod_route = off` clears the source immediately.
- State restore, performance reset, and instance destruction clear the route.
- Routes are deliberately not persisted in this feasibility build.

Missing callbacks disable emission. Invalid syntax leaves the prior route
unchanged. A valid retarget clears the previous contribution first; an unknown
parameter cannot leave its previous target modulated.

## Reproduce

Use unmodified Schwung v1.7.3, commit
`a6f7ec9e5cf4d0df172a1dbc00b29d0b5eb67124`, as a source checkout:

```sh
bash scripts/test_stock_field_host.sh /path/to/schwung-v1.7.3
```

The harness compiles the actual stock chain and actual HB DSP. Its only synthetic
musical component is a parameter probe used as a synth and audio effect.
Assertions cover Field output overrides, channel selection, depth, polarity,
base preservation, user base edits during modulation, track isolation, invalid
destination cleanup, disable, unload, audio FX routing, and coexistence with a
second modulation source. The source checkout is not patched.

## Confirmed limitation and next implementation boundary

Stock chain bypass stops calling the MIDI FX tick without notifying the module
or clearing its contributions. The test deliberately reproduces the resulting
held modulation value. Explicit `off` still clears it while bypassed. Also, the
existing shared Field clock uses the lowest allocated HB instance; bypassing
that instance can stop Field evolution. This increment does not solve that
pre-existing clock ownership limitation.

Before enabling this in a release, integrate route lifecycle with Movy's own
bypass/removal/track management, and establish a shared clock that survives
bypass without multiplying Field speed by track count. Verify all bypass paths,
including actions outside Movy's UI; do not promise universal bypass cleanup
from a callback the stock host never invokes. Add target selection and persistence
only after that behavior is settled. Master, sends, and LFO-to-LFO routing remain
outside this prototype. No Move hardware has been tested or deployed.
