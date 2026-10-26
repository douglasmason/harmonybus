/* Informational benchmark of the production Chord Play display request. */
#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
#include <time.h>

static double monotonic_seconds(void){
    struct timespec stamp;clock_gettime(CLOCK_MONOTONIC,&stamp);
    return stamp.tv_sec+stamp.tv_nsec*1e-9;
}
int main(void){
    Inst *conductor=fixture();API.set_param(conductor,"role","Conductor");
    hb_set_shared_follower_scale(1);
    char bank[2048],snapshot[4096];int used=0;
    for(int index=0;index<21;index++)used+=snprintf(bank+used,sizeof(bank)-used,
        "%s0,0,0,%d,0,1,12,%d,%d",index?";":"",index>=14?1:3,index<7?1:index<14?12:13,index%7);
    API.set_param(conductor,"hb_chord_bank",bank);
    for(int key_active=0;key_active<2;key_active++){
        g_key_context=(hb_key_context){.active=key_active,.source_root=0,.target_root=9,
            .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(9,2)};
        double started=monotonic_seconds(),maximum=0;
        for(int iteration=0;iteration<1000;iteration++){
            double before=monotonic_seconds();
            assert(API.get_param(conductor,"hb_chord_bank_snapshot",snapshot,sizeof(snapshot))>0);
            double elapsed=monotonic_seconds()-before;if(elapsed>maximum)maximum=elapsed;
        }
        printf("bank key%d mean_us %.2f max_us %.2f bytes %zu\n",key_active,
            (monotonic_seconds()-started)*1e3,maximum*1e6,strlen(snapshot));
    }
    API.destroy_instance(conductor);
}
