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

static inline int hb_cs_popcount12(unsigned int mask) {
    int count = 0;
    mask &= 0x0FFFu;
    for (int pitch_class = 0; pitch_class < 12; ++pitch_class)
        if (mask & (1u << pitch_class)) ++count;
    return count;
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

static inline int hb_cs_nearest_pc_at_or_above(int nominal, int minimum,
                                                 int pitch_class) {
    if (minimum < 0) minimum = 0;
    pitch_class = hb_cs_mod12(pitch_class);
    int best = -1;
    int best_distance = 1000000;
    for (int candidate = minimum; candidate <= 127; ++candidate) {
        if (hb_cs_mod12(candidate) != pitch_class) continue;
        int distance = hb_cs_abs(candidate - nominal);
        if (distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best;
}

static inline void hb_cs_search_unique(
    int degree,
    const int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    const unsigned int allowed_mask_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    int previous_note,
    unsigned int used_pitch_classes,
    int running_score,
    int trial[HB_CLOSEST_SPLIT_DEGREES],
    int *best_score,
    int *have_best,
    int best_outputs[HB_CLOSEST_SPLIT_DEGREES]) {

    if (degree == HB_CLOSEST_SPLIT_DEGREES) {
        if (!*have_best || running_score < *best_score) {
            for (int index = 0; index < HB_CLOSEST_SPLIT_DEGREES; ++index)
                best_outputs[index] = trial[index];
            *best_score = running_score;
            *have_best = 1;
        }
        return;
    }

    if (*have_best && running_score >= *best_score) return;

    unsigned int allowed = allowed_mask_by_degree[degree] & 0x0FFFu;
    for (int pitch_class = 0; pitch_class < 12; ++pitch_class) {
        unsigned int bit = 1u << pitch_class;
        if (!(allowed & bit) || (used_pitch_classes & bit)) continue;
        int candidate = hb_cs_nearest_pc_at_or_above(
            nominal_by_degree[degree], previous_note + 1, pitch_class);
        if (candidate < 0) continue;
        trial[degree] = candidate;
        hb_cs_search_unique(
            degree + 1,
            nominal_by_degree,
            allowed_mask_by_degree,
            candidate,
            used_pitch_classes | bit,
            running_score + hb_cs_abs(candidate - nominal_by_degree[degree]),
            trial,
            best_score,
            have_best,
            best_outputs);
    }
}

/* Build seven distinct, strictly ascending MIDI pitches, one for each
 * follower scale degree 0..6.
 *
 * Strong invariant: when the union of the per-degree legal masks contains
 * exactly seven pitch classes and a complete assignment exists, use every one
 * exactly once. This is the normal case for the explicit Closest Split modes
 * 135 / 2467 and 1357 / 246: their two sides partition the seven-note
 * chord-scale, so all seven follower degrees map to seven distinct scale pitch
 * classes in strictly increasing MIDI order.
 *
 * If Content restrictions make a seven-pitch-class assignment impossible,
 * fall back to the weaker invariant: seven distinct, strictly increasing MIDI
 * notes. Pitch classes may then repeat in later octaves, but degree order can
 * never collapse or invert.
 */
static inline int hb_build_monotonic_degree_ladder(
    const int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    const unsigned int allowed_mask_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    int output_by_degree[HB_CLOSEST_SPLIT_DEGREES]) {

    unsigned int union_mask = 0;
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        union_mask |= allowed_mask_by_degree[degree] & 0x0FFFu;

    if (hb_cs_popcount12(union_mask) == HB_CLOSEST_SPLIT_DEGREES) {
        int trial[HB_CLOSEST_SPLIT_DEGREES];
        int best_outputs[HB_CLOSEST_SPLIT_DEGREES];
        int best_score = 1000000000;
        int have_best = 0;
        hb_cs_search_unique(
            0,
            nominal_by_degree,
            allowed_mask_by_degree,
            -1,
            0u,
            0,
            trial,
            &best_score,
            &have_best,
            best_outputs);
        if (have_best) {
            for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
                output_by_degree[degree] = best_outputs[degree];
            return 1;
        }
    }

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
