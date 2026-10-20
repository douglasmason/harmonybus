#include <assert.h>
#include "../src/approach_state.h"

int main(void) {
    int persistent = HB_APPROACH_OFF;
    int next = HB_APPROACH_OFF;

    /* Fresh state must never decorate the first note. */
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_OFF);

    /* Persistent Scale Above toggle. */
    persistent = hb_approach_toggle(persistent, HB_APPROACH_SCALE_ABOVE, 1);
    assert(persistent == HB_APPROACH_SCALE_ABOVE);
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_SCALE_ABOVE);
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_SCALE_ABOVE);

    /* Chrom toggle excludes Scale toggle. */
    persistent = hb_approach_toggle(persistent, HB_APPROACH_CHROM_BELOW, 1);
    assert(persistent == HB_APPROACH_CHROM_BELOW);
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_CHROM_BELOW);

    /* Turning the active toggle off returns to neutral. */
    persistent = hb_approach_toggle(persistent, HB_APPROACH_CHROM_BELOW, 0);
    assert(persistent == HB_APPROACH_OFF);

    /* A one-shot overrides persistent state and is consumed exactly once. */
    persistent = HB_APPROACH_CHROM_BELOW;
    next = HB_APPROACH_SCALE_ABOVE;
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_SCALE_ABOVE);
    next = hb_approach_consume_next(next);
    assert(next == HB_APPROACH_OFF);
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_CHROM_BELOW);

    /* Symmetric one-shot Chrom Below. */
    persistent = HB_APPROACH_SCALE_ABOVE;
    next = HB_APPROACH_CHROM_BELOW;
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_CHROM_BELOW);
    next = hb_approach_consume_next(next);
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_SCALE_ABOVE);

    /* Reset semantics. */
    persistent = HB_APPROACH_OFF;
    next = HB_APPROACH_OFF;
    assert(hb_approach_effective(persistent, next) == HB_APPROACH_OFF);

    return 0;
}
