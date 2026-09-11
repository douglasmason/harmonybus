#ifndef HARMONYBUS_FOLLOWER_TIMING_H
#define HARMONYBUS_FOLLOWER_TIMING_H

/* Return the first boundary strictly after beat for the sequence
 * n * grid - anticipation. A non-positive grid disables the boundary. */
static inline double hb_ft_next_boundary(double beat, double grid, double anticipation) {
    if (grid <= 0.0) return -1.0;
    long cycle = (long)((beat + anticipation) / grid);
    double boundary = (double)cycle * grid - anticipation;
    while (boundary <= beat + 1e-9) boundary += grid;
    return boundary;
}

/* Follower Buffer is strictly PRE-boundary. Notes after a boundary are not
 * captured by the window that just ended. If chord and quant windows both
 * capture a note, choose the earliest upcoming boundary. */
static inline double hb_follower_capture_target(
    double beat,
    double chord_grid,
    double anticipation,
    double quant_grid,
    double capture_beats) {

    if (capture_beats <= 0.0) return -1.0;
    double target = -1.0;

    if (chord_grid > 0.0) {
        double boundary = hb_ft_next_boundary(beat, chord_grid, anticipation);
        double distance = boundary - beat;
        if (distance >= -1e-6 && distance <= capture_beats + 1e-6)
            target = boundary;
    }

    if (quant_grid > 0.0) {
        double boundary = hb_ft_next_boundary(beat, quant_grid, 0.0);
        double distance = boundary - beat;
        if (distance >= -1e-6 && distance <= capture_beats + 1e-6 &&
            (target < 0.0 || boundary < target))
            target = boundary;
    }

    return target;
}

/* A normal, uncaptured follower note may wait for the conductor only on its
 * FIRST release attempt. This protects same-scheduler-cycle chord+follower
 * ordering without turning conductor classification/confirmation into audible
 * follower latency. Captured notes render at their exact target boundary and
 * never wait beyond it for the conductor classifier. */
static inline int hb_follower_needs_same_tick_barrier(
    double target_beat,
    int age_frames_after_increment,
    int frames_per_tick,
    int conductor_pending) {

    if (target_beat >= 0.0 || !conductor_pending || frames_per_tick <= 0)
        return 0;
    return age_frames_after_increment <= frames_per_tick;
}

#endif
