# Field probability contract

The shared categorical probability helpers live in `core/include/probability.h`.
Play Motif lanes can now use this distribution through Field target 3 (Choice).
All populated user slots participate, with slot N sampling field channel N.
The Remote UI Motif probabilities disclosure provides exact-value controls,
using the existing Momentary / Latch selector and Field Glide setting.
Raw field channels remain playable from Move, Launchpad X and Remote UI.
LFO source routing is a separate integration and is not enabled by this change.

Display all N probabilities. Their sum is one, so there are N−1 degrees of
freedom. N nonnegative field weights can be normalized directly: multiplying
all weights by the same positive factor does not change the distribution.
This avoids introducing an arbitrary coordinate basis or a privileged last
choice. Field channels and probability overrides are separate controls.

## Direct edits

Setting one probability to q keeps it exactly at q and rescales the other
probabilities in proportion to their existing weights. For example,
20/30/50 becomes 60/15/25 when the first probability is set to 60%.

Held or latched probabilities are pinned. An edit cannot consume their mass:
it is clamped to the remainder available after other pins. Unpinned choices
divide the remaining mass in proportion to their underlying field weights.
If those weights are all zero, they divide it equally. With N−1 entries pinned,
the last is determined by the remainder; it cannot be adjusted independently.
For N=1 the sole probability is always one.

The core does not choose gesture ownership, latch state, timeouts or transport
behavior. Callers must retain the underlying field weights separately from
the effective probabilities so an override does not erase them. Input masks
use stable candidate indices; changing candidate membership requires explicit
remapping or clearing of pins, never silent reassignment to a different choice.

## Release

Compute the target distribution under the remaining pins, then interpolate
the whole distribution toward it. Convex interpolation preserves the total
and nonnegativity at every step. Both endpoints must agree on pins that are
still held. Apply a new gesture immediately using the edit helper before
starting a release transition; otherwise smoothing could move a held value.
The caller advances time once per engine update, never during display reads.

## Boundary and error behavior

All-zero field weights produce a uniform distribution. Normalization scales
by the maximum weight before summing to avoid overflow. Nonfinite or negative
weights, invalid masks and invalid counts are rejected without modifying the
output. All-pinned constraint input is accepted only if its total is one to
floating-point tolerance (1e-12). Edits clamp finite requests to the feasible
interval. Zero choices is an empty bank, not a synthetic probability entry.

The helpers allocate no memory and contain no random state. Sampling a choice
is a separate event: the motif integration draws once at successful phrase
launch and retains the selected phrase through its remaining taps. Preview
reads consume no draws. Empty libraries fail with the existing empty-motif
status. Membership changes clear probability overrides. Overrides are local
to the selected HarmonyBus instance and are cleared on state restoration;
they are not recorded or saved. Choice mappings use the existing shared
operation lane persistence. Stock motifs and editor auditions are unaffected.

`tests/probability_test.c` checks proportional edits, pins, exhausted budgets,
zero weights, maximum finite weights, invalid input, in-place operation and
normalized interpolation for distributions of 1–16 choices. It is registered
in the standard native suite manifest.
