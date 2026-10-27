# Parent key before the first chord — HarmonyBus 0.2.307

Follower notes now use the selected parent key even before a conductor has played
or selected a chord. Previously the display could show D major while the same
input tonic still sounded C. With Relative key travel, it now sounds D.

The fallback reuses the existing shared key-travel mapper. Input-root inference,
explicit input roots, recorded source degrees, Parallel and master transpose keep
their existing meaning. Closest and directional key-travel policies remain
intentional alternatives to Relative. No new mode or key setting is added.

Pair with Movy hbclean.275 for the parent-key readout and pad-position control.
In Set Parameters, knob 8 changes position; Shift + touch resets it. Left/Right
still shift pads during melodic note play, including Chord Play. The parent key
does not move when the pad pattern moves.

Native regressions cover all twelve roots, low/middle/high registers, master
transpose, chromatic travel, Parallel, unchanged Closest travel and note release.
The Movy integration checks the production audio chain, position changes and
recorded-degree playback. These checks do not substitute for listening and LED
inspection on Move.
