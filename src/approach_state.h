#ifndef HARMONYBUS_APPROACH_STATE_H
#define HARMONYBUS_APPROACH_STATE_H

/* Follower modifier values intentionally match the existing HarmonyBus enum:
 *   0 = chromatic approach below
 *   1 = off / neutral
 *   2 = scale approach above
 */
enum {
    HB_APPROACH_CHROM_BELOW = 0,
    HB_APPROACH_OFF = 1,
    HB_APPROACH_SCALE_ABOVE = 2
};

static inline int hb_approach_normalize(int value) {
    return (value >= HB_APPROACH_CHROM_BELOW && value <= HB_APPROACH_SCALE_ABOVE)
        ? value : HB_APPROACH_OFF;
}

static inline int hb_approach_toggle(int persistent, int target, int on) {
    persistent = hb_approach_normalize(persistent);
    target = hb_approach_normalize(target);
    if (!on) return persistent == target ? HB_APPROACH_OFF : persistent;
    return target;
}

static inline int hb_approach_effective(int persistent, int next) {
    next = hb_approach_normalize(next);
    return next != HB_APPROACH_OFF ? next : hb_approach_normalize(persistent);
}

static inline int hb_approach_consume_next(int next) {
    (void)next;
    return HB_APPROACH_OFF;
}

#endif
