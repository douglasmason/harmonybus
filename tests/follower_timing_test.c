#include <assert.h>
#include <stdio.h>
#include "../src/follower_timing.h"

static int nearly_equal(double left, double right) {
    double difference = left - right;
    if (difference < 0.0) difference = -difference;
    return difference < 1e-9;
}

int main(void) {
    const double capture = 0.10;

    /* 4-beat chord grid, no anticipation. Only the interval BEFORE beat 4
       belongs to the beat-4 capture window. */
    assert(nearly_equal(hb_follower_capture_target(3.95, 4.0, 0.0, 0.0, capture), 4.0));
    assert(hb_follower_capture_target(3.80, 4.0, 0.0, 0.0, capture) < 0.0);

    /* Immediately AFTER the chord boundary must not be delayed as though it
       belonged to the previous window, nor should it be sent to beat 8. */
    assert(hb_follower_capture_target(4.0001, 4.0, 0.0, 0.0, capture) < 0.0);
    assert(hb_follower_capture_target(4.05, 4.0, 0.0, 0.0, capture) < 0.0);

    /* Anticipated chord boundaries remain one-sided too: with a 1/2-beat
       anticipation on a 4-beat grid, the boundary is 3.5, 7.5, ... */
    assert(nearly_equal(hb_follower_capture_target(3.45, 4.0, 0.5, 0.0, capture), 3.5));
    assert(hb_follower_capture_target(3.55, 4.0, 0.5, 0.0, capture) < 0.0);

    /* Quant Grid uses the same pre-window semantics. */
    assert(nearly_equal(hb_follower_capture_target(0.95, 0.0, 0.0, 1.0, capture), 1.0));
    assert(hb_follower_capture_target(1.05, 0.0, 0.0, 1.0, capture) < 0.0);

    /* Same-cycle conductor ordering is allowed to cost exactly one release
       attempt, never an open-ended wait for harmony confirmation. */
    assert(hb_follower_needs_same_tick_barrier(-1.0, 64, 64, 1));
    assert(!hb_follower_needs_same_tick_barrier(-1.0, 128, 64, 1));
    assert(!hb_follower_needs_same_tick_barrier(-1.0, 64, 64, 0));

    /* A deliberately captured note is governed solely by target_beat and must
       never be held AFTER its boundary by conductor-pending state. */
    assert(!hb_follower_needs_same_tick_barrier(4.0, 64, 64, 1));

    /* A wide window must not advance an exactly aligned note. */
    assert(nearly_equal(hb_follower_capture_target(1.0, 4.0, 0.0, 0.5, 0.7), 1.0));
    assert(nearly_equal(hb_follower_capture_target(3.5, 4.0, 0.5, 0.0, 8.0), 3.5));
    assert(nearly_equal(hb_follower_capture_target(0.0, 4.0, 0.0, 0.5, 0.7), 0.0));
    assert(nearly_equal(hb_follower_capture_target(1.0001, 4.0, 0.0, 0.5, 0.7), 1.5));
    assert(nearly_equal(hb_follower_capture_target(3.4, 4.0, 0.0, 0.5, 0.7), 3.5));

    printf("follower_timing_test: ok\n");
    return 0;
}
