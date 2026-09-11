#ifndef HARMONYBUS_CLOSEST_SPLIT_H
#define HARMONYBUS_CLOSEST_SPLIT_H

#define HB_CLOSEST_SPLIT_DEGREES 7
#define HB_CLOSEST_SPLIT_SEARCH_RADIUS 6

static inline int hb_cs_mod12(int value) {
    value %= 12;
    return value < 0 ? value + 12 : value;
}

static inline int hb_cs_abs(int value) {
    return value < 0 ? -value : value;
}

static inline int hb_cs_nearest(int nominal, unsigned int pitch_mask) {
    int best = -1;
    int best_distance = 1000000;
    pitch_mask &= 0x0FFFu;
    if (!pitch_mask) return -1;
    for (int candidate = 0; candidate <= 127; ++candidate) {
        if (!(pitch_mask & (1u << hb_cs_mod12(candidate)))) continue;
        int distance = hb_cs_abs(candidate - nominal);
        if (distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best;
}

static inline void hb_cs_search_distinct(
    int degree,
    const int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    const unsigned int allowed_mask_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    unsigned char used_note[128],
    int running_cost,
    int trial[HB_CLOSEST_SPLIT_DEGREES],
    int *best_cost,
    int *have_best,
    int best_outputs[HB_CLOSEST_SPLIT_DEGREES]) {

    if (degree == HB_CLOSEST_SPLIT_DEGREES) {
        if (!*have_best || running_cost < *best_cost) {
            for (int index = 0; index < HB_CLOSEST_SPLIT_DEGREES; ++index)
                best_outputs[index] = trial[index];
            *best_cost = running_cost;
            *have_best = 1;
        }
        return;
    }
    if (*have_best && running_cost >= *best_cost) return;

    int nominal = nominal_by_degree[degree];
    unsigned int allowed = allowed_mask_by_degree[degree] & 0x0FFFu;
    for (int distance = 0; distance <= HB_CLOSEST_SPLIT_SEARCH_RADIUS; ++distance) {
        int candidates[2] = { nominal - distance, nominal + distance };
        int count = distance == 0 ? 1 : 2;
        for (int index = 0; index < count; ++index) {
            int candidate = candidates[index];
            if (candidate < 0 || candidate > 127) continue;
            if (used_note[candidate]) continue;
            if (!(allowed & (1u << hb_cs_mod12(candidate)))) continue;
            used_note[candidate] = 1;
            trial[degree] = candidate;
            hb_cs_search_distinct(
                degree + 1,
                nominal_by_degree,
                allowed_mask_by_degree,
                used_note,
                running_cost + distance,
                trial,
                best_cost,
                have_best,
                best_outputs);
            used_note[candidate] = 0;
        }
    }
}

/* Closest Split is a proximity mapper with a role-dependent candidate pool.
 * It deliberately does NOT enforce scale-degree monotonicity: with the explicit
 * 135/2467 or 1357/246 partitions, a strict seven-degree ordering tends to
 * reconstruct Relative mapping exactly and defeats the purpose of "Closest".
 *
 * Instead, each source degree keeps its actual register position as its nominal
 * and chooses from its assigned split pool. We solve the seven degrees jointly
 * only to avoid exact MIDI-note collisions when a distinct assignment exists
 * within six semitones of each nominal. Pitch-class repetition and local
 * inversions are allowed. If no bounded distinct assignment exists, fall back
 * to the independently closest choices rather than making octave-scale jumps.
 */
static inline int hb_build_closest_split_assignment(
    const int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    const unsigned int allowed_mask_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    int output_by_degree[HB_CLOSEST_SPLIT_DEGREES]) {

    int independent[HB_CLOSEST_SPLIT_DEGREES];
    int has_collision = 0;
    unsigned char independent_used[128] = {0};
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree) {
        int candidate = hb_cs_nearest(
            nominal_by_degree[degree], allowed_mask_by_degree[degree]);
        if (candidate < 0) return 0;
        independent[degree] = candidate;
        if (independent_used[candidate]) has_collision = 1;
        independent_used[candidate] = 1;
    }

    if (!has_collision) {
        for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
            output_by_degree[degree] = independent[degree];
        return 1;
    }

    unsigned char used_note[128] = {0};
    int trial[HB_CLOSEST_SPLIT_DEGREES];
    int best_outputs[HB_CLOSEST_SPLIT_DEGREES];
    int best_cost = 1000000000;
    int have_best = 0;
    hb_cs_search_distinct(
        0,
        nominal_by_degree,
        allowed_mask_by_degree,
        used_note,
        0,
        trial,
        &best_cost,
        &have_best,
        best_outputs);

    if (have_best) {
        for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
            output_by_degree[degree] = best_outputs[degree];
        return 1;
    }

    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        output_by_degree[degree] = independent[degree];
    return 1;
}

#endif
