#ifndef HARMONYBUS_CLOSEST_SPLIT_H
#define HARMONYBUS_CLOSEST_SPLIT_H

#define HB_CLOSEST_SPLIT_DEGREES 7
#define HB_CLOSEST_SPLIT_MAX_MONOTONIC_DISPLACEMENT 6
#define HB_CLOSEST_SPLIT_MAX_EXTRA_COST 8

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

static inline int hb_cs_strictly_increasing(
    const int values[HB_CLOSEST_SPLIT_DEGREES]) {
    for (int degree = 1; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        if (values[degree] <= values[degree - 1]) return 0;
    return 1;
}

/* Closest Split is fundamentally a proximity mapper, not a scale-degree mapper.
 * First find the independently closest pitch in each degree's assigned split
 * pool. If that result is already strictly ordered, keep it exactly.
 *
 * When independent closest choices cross or collide, try a best-effort
 * monotonic repair. The repair is accepted only when it remains musically
 * close: no degree may move more than six semitones from its nominal and the
 * total movement may exceed the independent optimum by at most eight
 * semitones. Otherwise proximity wins and the independent result is returned.
 *
 * This intentionally does NOT require seven unique pitch classes. Repeated
 * pitch classes in different octaves are legal, and even local inversions are
 * legal when enforcing monotonicity would create an obviously non-closest
 * mapping. This keeps Closest Split behavior distinct from Relative.
 */
static inline int hb_build_closest_split_ladder(
    const int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    const unsigned int allowed_mask_by_degree[HB_CLOSEST_SPLIT_DEGREES],
    int output_by_degree[HB_CLOSEST_SPLIT_DEGREES]) {

    int independent[HB_CLOSEST_SPLIT_DEGREES];
    int independent_cost = 0;
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree) {
        int candidate = hb_cs_nearest(
            nominal_by_degree[degree], allowed_mask_by_degree[degree]);
        if (candidate < 0) return 0;
        independent[degree] = candidate;
        independent_cost += hb_cs_abs(candidate - nominal_by_degree[degree]);
    }

    if (hb_cs_strictly_increasing(independent)) {
        for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
            output_by_degree[degree] = independent[degree];
        return 1;
    }

    enum { INF = 1000000000 };
    int cost[HB_CLOSEST_SPLIT_DEGREES][128];
    int prev[HB_CLOSEST_SPLIT_DEGREES][128];

    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree) {
        for (int note = 0; note < 128; ++note) {
            cost[degree][note] = INF;
            prev[degree][note] = -1;
        }
    }

    for (int note = 0; note < 128; ++note) {
        if (!(allowed_mask_by_degree[0] & (1u << hb_cs_mod12(note)))) continue;
        int displacement = hb_cs_abs(note - nominal_by_degree[0]);
        if (displacement > HB_CLOSEST_SPLIT_MAX_MONOTONIC_DISPLACEMENT) continue;
        cost[0][note] = displacement;
    }

    for (int degree = 1; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree) {
        for (int note = 0; note < 128; ++note) {
            if (!(allowed_mask_by_degree[degree] & (1u << hb_cs_mod12(note)))) continue;
            int displacement = hb_cs_abs(note - nominal_by_degree[degree]);
            if (displacement > HB_CLOSEST_SPLIT_MAX_MONOTONIC_DISPLACEMENT) continue;
            for (int previous = 0; previous < note; ++previous) {
                if (cost[degree - 1][previous] == INF) continue;
                int candidate_cost = cost[degree - 1][previous] + displacement;
                if (candidate_cost < cost[degree][note]) {
                    cost[degree][note] = candidate_cost;
                    prev[degree][note] = previous;
                }
            }
        }
    }

    int best_note = -1;
    int best_cost = INF;
    for (int note = 0; note < 128; ++note) {
        if (cost[HB_CLOSEST_SPLIT_DEGREES - 1][note] < best_cost) {
            best_cost = cost[HB_CLOSEST_SPLIT_DEGREES - 1][note];
            best_note = note;
        }
    }

    if (best_note >= 0 &&
        best_cost <= independent_cost + HB_CLOSEST_SPLIT_MAX_EXTRA_COST) {
        int note = best_note;
        for (int degree = HB_CLOSEST_SPLIT_DEGREES - 1; degree >= 0; --degree) {
            output_by_degree[degree] = note;
            note = prev[degree][note];
        }
        return 1;
    }

    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        output_by_degree[degree] = independent[degree];
    return 1;
}

#endif
