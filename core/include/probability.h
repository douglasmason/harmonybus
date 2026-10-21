#ifndef HB_PROBABILITY_H
#define HB_PROBABILITY_H

/* Bounded categorical distributions. No allocation, random state or UI state.
 * We expose all N probabilities; normalization leaves N-1 degrees of freedom.
 */
#ifdef HB_FREESTANDING
extern double fabs(double);
#define hb_probability_finite(value) __builtin_isfinite(value)
#else
#include <math.h>
#define hb_probability_finite(value) isfinite(value)
#endif
#define HB_PROBABILITY_MAX 16

static inline int hb_probability_count(int count) {
    return count > 0 && count <= HB_PROBABILITY_MAX;
}

/* Normalize nonnegative finite weights. Zero mass means uniform, including
 * the case where the only formerly nonzero entry has been manually reduced.
 * Scale before summing so even DBL_MAX weights cannot overflow. */
static inline int hb_probability_normalize(const double *weights, int count,
                                           double *result) {
    if (!weights || !result || !hb_probability_count(count)) return 0;
    double maximum = 0, sum = 0;
    for (int index = 0; index < count; index++) {
        if (!hb_probability_finite(weights[index]) || weights[index] < 0) return 0;
        if (weights[index] > maximum) maximum = weights[index];
    }
    if (maximum == 0) {
        for (int index = 0; index < count; index++) result[index] = 1.0 / count;
        return 1;
    }
    for (int index = 0; index < count; index++) sum += weights[index] / maximum;
    for (int index = 0; index < count; index++) result[index] = (weights[index] / maximum) / sum;
    return 1;
}

/* Pinned entries are exact. Free entries share the remainder proportionally.
 * Invalid input leaves result untouched. All-pinned input must already sum
 * to one: there is no free degree of freedom left to absorb a remainder. */
static inline int hb_probability_constrain(const double *weights, int count,
                                           unsigned pinned, const double *values,
                                           double *result) {
    if (!weights || !values || !result || !hb_probability_count(count) ||
        (pinned >> count)) return 0;
    double next[HB_PROBABILITY_MAX], maximum = 0, sum = 0, committed = 0;
    int free_count = 0;
    for (int index = 0; index < count; index++) {
        if (!hb_probability_finite(weights[index]) || weights[index] < 0) return 0;
        if (pinned & (1u << index)) {
            if (!hb_probability_finite(values[index]) || values[index] < 0 || values[index] > 1) return 0;
            committed += values[index];
            next[index] = values[index];
        } else {
            free_count++;
            if (weights[index] > maximum) maximum = weights[index];
        }
    }
    if (committed > 1 + 1e-12 || (!free_count && fabs(committed - 1) > 1e-12)) return 0;
    double remainder = committed < 1 ? 1 - committed : 0;
    if (maximum > 0)
        for (int index = 0; index < count; index++)
            if (!(pinned & (1u << index))) sum += weights[index] / maximum;
    for (int index = 0; index < count; index++) {
        if (!(pinned & (1u << index)))
            next[index] = remainder * (maximum > 0 ? (weights[index] / maximum) / sum : 1.0 / free_count);
    }
    for (int index = 0; index < count; index++) result[index] = next[index];
    return 1;
}

/* Exact-value edit, clamped to the mass left by other pinned entries. This
 * does not alter the mask: the caller chooses momentary or latched ownership. */
static inline int hb_probability_edit(const double *weights, int count,
                                      unsigned pinned, const double *values,
                                      int selected, double requested, double *result) {
    if (!values || !hb_probability_count(count) || selected < 0 || selected >= count ||
        !hb_probability_finite(requested) || (pinned >> count)) return 0;
    double targets[HB_PROBABILITY_MAX], available = 1;
    for (int index = 0; index < count; index++) {
        targets[index] = values[index];
        if (index != selected && (pinned & (1u << index))) {
            if (!hb_probability_finite(values[index]) || values[index] < 0 || values[index] > 1) return 0;
            available -= values[index];
        }
    }
    if (available < -1e-12) return 0;
    if (available < 0) available = 0;
    targets[selected] = requested < 0 ? 0 : requested > available ? available : requested;
    /* With every other entry pinned, the selected value is determined. */
    if ((pinned | (1u << selected)) == (1u << count) - 1)
        targets[selected] = available;
    return hb_probability_constrain(weights, count, pinned | (1u << selected), targets, result);
}

/* Convex interpolation preserves normalization and common pinned values.
 * The caller supplies the current distribution and the newly constrained
 * field distribution when releasing an override. */
static inline int hb_probability_blend(const double *from, const double *to,
                                       int count, double amount, double *result) {
    if (!from || !to || !result || !hb_probability_count(count) || !hb_probability_finite(amount)) return 0;
    double first[HB_PROBABILITY_MAX], second[HB_PROBABILITY_MAX];
    if (!hb_probability_normalize(from, count, first) || !hb_probability_normalize(to, count, second)) return 0;
    if (amount < 0) amount = 0;
    if (amount > 1) amount = 1;
    for (int index = 0; index < count; index++) result[index] = first[index] + amount * (second[index] - first[index]);
    return 1;
}

#endif
