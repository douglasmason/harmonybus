#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include "../core/include/probability.h"

static void valid(const double *values, int count) {
    double sum = 0;
    for (int index = 0; index < count; index++) {
        assert(isfinite(values[index]) && values[index] >= 0 && values[index] <= 1);
        sum += values[index];
    }
    assert(fabs(sum - 1) < 1e-12);
}

int main(void) {
    double weights[16] = {.2, .3, .5}, values[16] = {0}, output[16];
    assert(hb_probability_edit(weights, 3, 0, values, 0, .6, output));
    assert(fabs(output[0] - .6) < 1e-12);
    assert(fabs(output[1] - .15) < 1e-12);
    assert(fabs(output[2] - .25) < 1e-12);
    values[0] = .6;
    assert(hb_probability_edit(weights, 3, 1, values, 1, .8, output));
    assert(output[0] == .6 && fabs(output[1] - .4) < 1e-12 && output[2] == 0);
    values[1] = .3;
    assert(hb_probability_edit(weights, 3, 3, values, 2, .7, output));
    assert(fabs(output[2] - .1) < 1e-12);
    weights[0] = 1; weights[1] = weights[2] = 0;
    assert(hb_probability_edit(weights, 3, 0, values, 0, .6, output));
    assert(output[1] == .2 && output[2] == .2);
    weights[0] = weights[1] = DBL_MAX;
    assert(hb_probability_normalize(weights, 3, output)); valid(output, 3);
    assert(output[0] == .5 && output[1] == .5);
    output[0] = 123;
    weights[1] = NAN;
    assert(!hb_probability_normalize(weights, 3, output) && output[0] == 123);
    assert(!hb_probability_edit(weights, 3, 8, values, 0, .5, output));
    assert(!hb_probability_normalize(weights, 0, output));
    assert(!hb_probability_normalize(weights, 17, output));
    assert(hb_probability_edit(weights, 1, 0, values, 0, .1, output));
    assert(output[0] == 1);
    for (int count = 2; count <= 16; count++) {
        for (int index = 0; index < count; index++) weights[index] = (index + 1.0) * (index + 1);
        for (int selected = 0; selected < count; selected++) {
            for (int step = 0; step <= 100; step++) {
                double requested = step / 100.0;
                assert(hb_probability_edit(weights, count, 0, values, selected, requested, output));
                valid(output, count); assert(output[selected] == requested);
                double underlying[16], blended[16];
                assert(hb_probability_normalize(weights, count, underlying));
                for (int tick = 0; tick <= 10; tick++) {
                    assert(hb_probability_blend(output, underlying, count, tick / 10.0, blended));
                    valid(blended, count);
                }
                /* In-place use must preserve proportional redistribution. */
                assert(hb_probability_edit(output, count, 0, values, selected, requested, output));
                valid(output, count);
            }
        }
    }
    puts("Probability normalization, edits, constraints and release interpolation passed");
}
