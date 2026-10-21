#define main field_source_fixture_main
#include "chord_player_test.c"
#undef main
#include "../integration/schwung/field_source_runtime.h"
int main(void) {
    Inst *instance=fixture();
    assert(g_field_publish); /* Resolved through the actual optional export. */
    float signal=99;
    assert(!schwung_field_read_v1(0,&signal));
    API.set_param(instance,"field_glide","0");
    API.set_param(instance,"field_outputs","fo1,2500,7500,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1");
    schwung_field_begin_block();advance(instance,3,64);schwung_field_begin_block();
    assert(schwung_field_read_v1(0,&signal)&&fabs(signal+.5f)<1e-6);
    assert(schwung_field_read_v1(1,&signal)&&fabs(signal-.5f)<1e-6);
    float repeated=0;
    assert(schwung_field_read_v1(1,&repeated)&&repeated==signal);
    assert(!schwung_field_read_v1(-1,&signal));
    assert(!schwung_field_read_v1(16,&signal));
    Inst *second=API.create_instance("",NULL);
    advance(instance,3,64);double before=g_hb_field.position[0];
    advance(second,3,64);assert(g_hb_field.position[0]==before);
    float other[16]={0};int competing_owner=0;
    assert(!schwung_field_publish_v1(&competing_owner,other,16));
    /* Simulate bypassing the original clock owner: another instance ticks first. */
    schwung_field_begin_block();advance(second,3,64);assert(g_hb_field.position[0]>before);
    before=g_hb_field.position[0];advance(instance,3,64);assert(g_hb_field.position[0]==before);
    API.destroy_instance(instance);
    schwung_field_begin_block();advance(second,3,64);assert(g_hb_field.position[0]>before);
    assert(schwung_field_read_v1(1,&signal));
    __atomic_fetch_sub(&field_samples[1], (uint64_t)251<<32, __ATOMIC_RELAXED);assert(!schwung_field_read_v1(1,&signal));
    schwung_field_begin_block();advance(second,3,64);schwung_field_begin_block();assert(schwung_field_read_v1(1,&signal));
    API.destroy_instance(second);assert(!schwung_field_read_v1(1,&signal));
    assert(schwung_field_publish_v1(&competing_owner,other,16));schwung_field_begin_block();
    assert(schwung_field_read_v1(1,&signal)&&signal==-1);
    assert(schwung_field_publish_v1(&competing_owner,0,0));
    puts("field source bridge: overrides, shared clock, ownership, expiry and unload passed");
    return 0;
}
