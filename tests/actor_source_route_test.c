#include <assert.h>
#include <stdio.h>
#include "../core/include/actor_source_route.h"
int main(void) {
    hb_clip_ref_t own={1,2};
    hb_actor_source_route_t route={0};
    hb_resolved_actor_source_t result=hb_actor_source_resolve(route,own,16,8);
    assert(result.valid && result.pitch.track==1 && result.pitch.clip==2);
    assert(result.rhythm.track==1 && result.rhythm.clip==2);
    route.pitch_kind=HB_SOURCE_CLIP_REF;
    route.pitch_ref=(hb_clip_ref_t){3,4};
    route.rhythm_kind=HB_RHYTHM_CLIP_REF;
    route.rhythm_ref=(hb_clip_ref_t){6,7};
    result=hb_actor_source_resolve(route,own,16,8);
    assert(result.valid && result.pitch.track==3 && result.pitch.clip==4);
    assert(result.rhythm.track==6 && result.rhythm.clip==7);
    route.pitch_kind=HB_SOURCE_CONDUCTOR_ROOT;
    result=hb_actor_source_resolve(route,own,16,8);
    assert(result.valid && result.pitch_kind==HB_SOURCE_CONDUCTOR_ROOT);
    route.pitch_kind=HB_SOURCE_GENERATED_CHORD;
    result=hb_actor_source_resolve(route,own,16,8);
    assert(result.valid && result.pitch_kind==HB_SOURCE_GENERATED_CHORD);
    route.pitch_kind=HB_SOURCE_CLIP_REF;
    route.pitch_ref.track=16;
    result=hb_actor_source_resolve(route,own,16,8);
    assert(!result.valid);
    route.pitch_ref.track=3;
    route.rhythm_ref.clip=8;
    result=hb_actor_source_resolve(route,own,16,8);
    assert(!result.valid);
    route.rhythm_ref.clip=7;
    result=hb_actor_source_resolve(route,(hb_clip_ref_t){-1,2},16,8);
    assert(!result.valid);
    result=hb_actor_source_resolve(route,own,0,8);
    assert(!result.valid);
    puts("Actor source routing: own, cross-clip, generative and invalid cases pass");
    return 0;
}
