#define main reference_test_main
#include "follower_reference_test.c"
#undef main

int main(void){
    Inst *instance=fixture();char text[65536],state[65536],before[8192],after[8192];
    API.get_param(instance,"motion_host_version",text,sizeof(text));assert(!strcmp(text,"3"));
    API.set_param(instance,"motion_host","movy-clip-v2");
    API.get_param(instance,"chain_params",text,sizeof(text));assert(!strstr(text,"Drum Fill"));
    API.set_param(instance,"motion_host","movy-clip-v3");
    API.get_param(instance,"chain_params",text,sizeof(text));assert(strstr(text,"Drum Fill"));
    API.set_param(instance,"motion_operation","Drum Fill");
    API.get_param(instance,"motion_gesture_binding_1",text,sizeof(text));assert(!strncmp(text,"66,38,",6));
    API.set_param(instance,"motion_control_1","140");
    API.get_param(instance,"motion_control_1",text,sizeof(text));assert(!strcmp(text,"127"));
    API.set_param(instance,"motion_amount","-1");
    API.get_param(instance,"motion_amount",text,sizeof(text));assert(!strcmp(text,"0"));
    API.set_param(instance,"motion_amount","38");API.set_param(instance,"motion_enabled","On");
    assert(!instance->motion.lanes[0].enabled);assert(!hb_mo_lane_active(&instance->motion,0));
    assert(hb_mo_save(&instance->motion,state,sizeof(state),0)>0);
    hb_motion_config restored;hb_mo_defaults(&restored);hb_mo_restore(&restored,state);
    assert(restored.lanes[0].operation==HB_MO_DRUM_FILL&&restored.lanes[0].amount==38);
    hb_commit_observed_harmony(chord(0,0,1));
    // A future/stale learned boundary must not recolor Current before the
    // conductor changes. This used to disagree until the next loop reset.
    instance->next_lookahead=0;g_bus.clip_loop_end=4;
    g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,1)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,0)};
    unsigned reference_current=0;
    for(int beat=0;beat<12;beat++){
        position=beat+0.25;
        API.get_param(instance,"pad_harmony",text,sizeof(text));
        unsigned current=0,effective=0;assert(sscanf(text,"%u,%u",&current,&effective)==2);
        if(!beat)reference_current=current;
        assert(current==reference_current&&current==effective);
    }
    API.get_param(instance,"pad_view",before,sizeof(before));
    unsigned generation=g_timeline_generation;unsigned actions=instance->action_count;
    for(int read=0;read<100;read++){
        int size=API.get_param(instance,"harmony_status_snapshot",text,sizeof(text));
        assert(size>0&&!strncmp(text,"dp1|",4));
        int fields=0;for(int i=0;i<size;i++)if(text[i]=='|')fields++;assert(fields==8);
    }
    API.get_param(instance,"pad_view",after,sizeof(after));
    assert(!strcmp(before,after));assert(generation==g_timeline_generation&&actions==instance->action_count);
    memset(text,0x55,sizeof(text));assert(API.get_param(instance,"harmony_status_snapshot",text,12)<0);
    assert(text[12]==0x55);
    API.destroy_instance(instance);
    puts("Performance controls: capability-gated assignment, note limits, persistence, inert playback, read-only harmony inspection pass");
}
