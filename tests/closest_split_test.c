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
    {
        const int source[4]={60,62,64,65};
        const unsigned allowed[4]={0xfffu,0xfffu,0xfffu,0xfffu};
        int output[4]={60,64,62,65};
        int before=hb_cs_run_cost(4,source,output);
        hb_cs_order_run(4,source,allowed,output);
        assert(hb_cs_run_cost(4,source,output)<before);
        for(int row=1;row<4;row++)assert(output[row]>output[row-1]);
        const unsigned fixed[4]={1u,1u<<4,1u<<2,1u<<5};
        int constrained[4]={60,64,62,65};
        hb_cs_order_run(4,source,fixed,constrained);
        assert(constrained[1]==64&&constrained[2]==62); /* class boundary wins */
    }

    /* Diversity is not a small motion discount: three available chord tones
       must not collapse to two merely because two inputs are near the root. */
    {
        const int nominal[3]={60,61,62};
        const unsigned allowed[3]={0x91u,0x91u,0x91u};int mapped[3];
        assert(hb_build_closest_assignment(3,nominal,allowed,mapped));
        unsigned used=0;for(int index=0;index<3;index++)used|=1u<<hb_cs_mod12(mapped[index]);
        assert(used==0x91u);
        /* A restricted class may necessarily reuse a tone. It must never
           borrow an available pitch from the other class to hide that fact. */
        const unsigned restricted[3]={1u,1u,0x90u};
        assert(hb_build_closest_assignment(3,nominal,restricted,mapped));
        assert(hb_cs_mod12(mapped[0])==0&&hb_cs_mod12(mapped[1])==0);
        assert(restricted[2]&(1u<<hb_cs_mod12(mapped[2])));
        unsigned char required[12]={0};required[0]=required[4]=required[7]=1;
        hb_closest_cache cache={0};
        assert(hb_cached_closest_assignment_required(&cache,3,nominal,allowed,required,mapped));
        unsigned before=cache.cursor;
        assert(hb_cached_closest_assignment_required(&cache,3,nominal,allowed,required,mapped));
        assert(cache.cursor==before);
        required[0]=2;required[7]=0;
        assert(hb_cached_closest_assignment_required(&cache,3,nominal,allowed,required,mapped));
        int roots=0;used=0;
        for(int index=0;index<3;index++){roots+=hb_cs_mod12(mapped[index])==0;used|=1u<<hb_cs_mod12(mapped[index]);}
        assert(roots==2&&used==0x11u&&cache.cursor==before+1);
    }
    /* Independent exhaustive oracle: every collection, chromatic tie and
       boundary register must retain the old pitch choice exactly. */
    for(int nominal=-24;nominal<=151;nominal++)for(unsigned mask=0;mask<4096;mask++){
        int expected=-1,distance=1000000;
        for(int pitch=0;pitch<128;pitch++)if(mask&(1u<<(pitch%12))){
            int next=hb_cs_abs(pitch-nominal);
            if(next<distance){expected=pitch;distance=next;}
        }
        assert(hb_cs_nearest(nominal,mask)==expected);
    }
    /* Exact cached and fresh solves agree through context changes and eviction. */
    hb_closest_cache cache={0};
    for(int pass=0;pass<3;pass++)for(int context=0;context<40;context++){
        int count=1+context%12,nominal[12],fresh[12],cached[12];unsigned allowed[12];
        for(int row=0;row<count;row++){
            nominal[row]=24+(context*7+row*2)%80;
            allowed[row]=(context%3==0?0x91u:context%3==1?0xAB5u:0xFFFu);
        }
        int expected=hb_build_closest_assignment(count,nominal,allowed,fresh);
        assert(hb_cached_closest_assignment(&cache,count,nominal,allowed,cached)==expected);
        if(expected){
            for(int row=0;row<count;row++)assert(cached[row]==fresh[row]);
            unsigned cursor=cache.cursor;
            assert(hb_cached_closest_assignment(&cache,count,nominal,allowed,cached));
            assert(cache.cursor==cursor);
        }
    }
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
