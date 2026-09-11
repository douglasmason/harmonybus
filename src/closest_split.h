#ifndef HARMONYBUS_CLOSEST_SPLIT_H
#define HARMONYBUS_CLOSEST_SPLIT_H

#define HB_CLOSEST_SPLIT_DEGREES 7

static inline int hb_cs_mod12(int value) {
    value %= 12;
    return value < 0 ? value + 12 : value;
}

static inline int hb_cs_abs(int value) {
    return value < 0 ? -value : value;
}

static inline int hb_cs_nearest_at_or_above(int nominal, int minimum,
                                             unsigned int pitch_mask) {
    int best = -1;
    int best_distance = 1000000;
    pitch_mask &= 0x0FFFu;
    if (!pitch_mask) return -1;
    if (minimum < 0) minimum = 0;
    for (int candidate = minimum; candidate <= 127; ++candidate) {
        if (!(pitch_mask & (1u << hb_cs_mod12(candidate)))) continue;
        int distance = hb_cs_abs(candidate - nominal);
        if (distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best;
}

/* Build seven distinct, strictly ascending MIDI pitches, one for each
 * follower scale degree 0..6.  Each degree may have a different allowed
 * pitch-class mask (e.g. the ON vs OUT side of Closest Split).
 *
 * The nominals express where ordinary scale-degree mapping would put each
 * degree.  The search chooses the closest legal note while enforcing
 * output[d] < output[d+1].  If a ladder would run off the top of MIDI range,
 * octave-shifted candidates are considered automatically.
 */
static inline int hb_build_monotonic_degree_ladder(
    const int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    const unsigned int allowed_mask_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    int output_by_degree[HB_CLOSEST_SPLIT_DEGREES]) {

    int best_outputs[HB_CLOSEST_SPLIT_DEGREES];
    int have_best = 0;
    int best_score = 1000000000;

    for (int octave_shift = -48; octave_shift <= 48; octave_shift += 12) {
        int trial[HB_CLOSEST_SPLIT_DEGREES];
        int previous = -1;
        int score = 0;
        int valid = 1;

        for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree) {
            int shifted_nominal = nominal_by_degree[degree] + octave_shift;
            int candidate = hb_cs_nearest_at_or_above(
                shifted_nominal, previous + 1, allowed_mask_by_degree[degree]);
            if (candidate < 0) {
                valid = 0;
                break;
            }
            trial[degree] = candidate;
            previous = candidate;
            /* Prefer the original register; octave shifts are only a way to
             * find a feasible ordered ladder near range boundaries. */
            score += hb_cs_abs(candidate - nominal_by_degree[degree]);
        }

        if (valid && (!have_best || score < best_score)) {
            for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
                best_outputs[degree] = trial[degree];
            best_score = score;
            have_best = 1;
        }
    }

    if (!have_best) return 0;
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        output_by_degree[degree] = best_outputs[degree];
    return 1;
}

#endif
