#include <assert.h>
#include <stdio.h>
#include "../src/closest_split.h"

static void assert_legal(
    const int outputs[HB_CLOSEST_SPLIT_DEGREES],
    const unsigned int masks[HB_CLOSEST_SPLIT_DEGREES]) {
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert(masks[degree] & (1u << (outputs[degree] % 12)));
}

static void assert_distinct_notes(const int outputs[HB_CLOSEST_SPLIT_DEGREES]) {
    for (int left = 0; left < HB_CLOSEST_SPLIT_DEGREES; ++left)
        for (int right = left + 1; right < HB_CLOSEST_SPLIT_DEGREES; ++right)
            assert(outputs[left] != outputs[right]);
}

int main(void) {
    const unsigned int c_major_degree_mask[HB_CLOSEST_SPLIT_DEGREES] = {
        1u << 0, 1u << 2, 1u << 4, 1u << 5, 1u << 7, 1u << 9, 1u << 11
    };
    const unsigned int on_135 =
        c_major_degree_mask[0] | c_major_degree_mask[2] | c_major_degree_mask[4];
    const unsigned int off_2467 =
        c_major_degree_mask[1] | c_major_degree_mask[3] |
        c_major_degree_mask[5] | c_major_degree_mask[6];
    const unsigned int on_1357 = on_135 | c_major_degree_mask[6];
    const unsigned int off_246 =
        c_major_degree_mask[1] | c_major_degree_mask[3] | c_major_degree_mask[5];

    unsigned int masks[HB_CLOSEST_SPLIT_DEGREES];
    int outputs[HB_CLOSEST_SPLIT_DEGREES];

    /* Source-space F natural minor degrees in their ACTUAL register. Against
       a C-major 135/2467 split, Closest Split should stay near these pitches,
       not rebuild C-D-E-F-G-A-B as Relative would. */
    const int f_minor_nominals[HB_CLOSEST_SPLIT_DEGREES] = {
        65, 67, 68, 70, 72, 73, 75
    };
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        masks[degree] = (degree == 0 || degree == 2 || degree == 4)
            ? on_135 : off_2467;

    assert(hb_build_closest_split_assignment(f_minor_nominals, masks, outputs));
    assert_legal(outputs, masks);
    assert_distinct_notes(outputs);

    /* The root is the key anti-regression: Relative would send source-root F
       to target-root C. Closest Split must instead choose the nearby member of
       the 135 pool (E or G), proving the solver has not reconstructed Relative. */
    assert(outputs[0] == 64 || outputs[0] == 67);
    assert((outputs[0] % 12) != 0);

    int relative_shape = 1;
    const int c_major_relative[HB_CLOSEST_SPLIT_DEGREES] = {
        60, 62, 64, 65, 67, 69, 71
    };
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        if (outputs[degree] != c_major_relative[degree]) relative_shape = 0;
    assert(!relative_shape);

    /* 1357/246 must also remain a proximity mapping and produce legal,
       collision-free notes without requiring monotonic pitch-class order. */
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        masks[degree] = (degree == 0 || degree == 2 || degree == 4 || degree == 6)
            ? on_1357 : off_246;
    assert(hb_build_closest_split_assignment(f_minor_nominals, masks, outputs));
    assert_legal(outputs, masks);
    assert_distinct_notes(outputs);
    assert((outputs[0] % 12) != 0);

    /* Monotonicity is intentionally NOT a hard invariant. Construct a case
       where keeping every note close requires a local inversion; the solver
       must prefer proximity rather than throwing a degree an octave away. */
    const int crossing_nominals[HB_CLOSEST_SPLIT_DEGREES] = {
        72, 73, 74, 75, 76, 77, 78
    };
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        masks[degree] = (degree == 0 || degree == 2 || degree == 4)
            ? on_135 : off_2467;
    assert(hb_build_closest_split_assignment(crossing_nominals, masks, outputs));
    assert_legal(outputs, masks);
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert(outputs[degree] >= crossing_nominals[degree] - HB_CLOSEST_SPLIT_SEARCH_RADIUS &&
               outputs[degree] <= crossing_nominals[degree] + HB_CLOSEST_SPLIT_SEARCH_RADIUS);

    printf("closest_split_test: ok\n");
    return 0;
}
