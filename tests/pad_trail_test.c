#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
static void play(Inst *instance,int pitch){
    uint8_t message[3]={0x90,(uint8_t)pitch,100},out[128][3];int lens[128];
    API.process_midi(instance,message,3,out,lens,128);
    g_conductor_block_ready=1;API.tick(instance,128,48000,out,lens,128);
}
static void mapped_target(void){
    Inst *instance=fixture();hb_effective_write(chord(0,0,0));
    instance->motion.lanes[0].operation=HB_MO_TRANSPOSE;instance->motion.lanes[0].amount=2;instance->motion.lanes[0].offset=0;
    API.set_param(instance,"motion_gesture_1","LatchOn");
    play(instance,60);assert(instance->trail_valid[62]);assert(!instance->trail_valid[60]);
    double onset=instance->trail_at[62];hb_effective_write(chord(5,0,0));
    uint8_t out[128][3];int lens[128];API.tick(instance,128,48000,out,lens,128);
    assert(instance->trail_at[62]==onset);assert(instance->trail_chord_at[62]<instance->trail_chord);
    char history[8192];assert(API.get_param(instance,"trail_history",history,sizeof(history))>0);assert(strstr(history,";62,"));
    API.destroy_instance(instance);
}
static void preview_target(void){
    Inst *instance=fixture();hb_effective_write(chord(0,0,0));
    instance->motion.lanes[0].operation=HB_MO_TRANSPOSE;instance->motion.lanes[0].amount=2;instance->motion.lanes[0].offset=0;
    API.set_param(instance,"motion_gesture_1","LatchOn");
    API.set_param(instance,"trail_enable","1");
    API.set_param(instance,"pad_display","Effective");
    char pads[65];for(int i=0;i<32;i++){pads[i*2]='3';pads[i*2+1]='c';}pads[64]=0;
    API.set_param(instance,"pad_preview_inputs",pads);
    char snapshot[8192];API.get_param(instance,"pad_view",snapshot,sizeof(snapshot));
    assert(strstr(snapshot,"|targets1,62,62"));assert(strstr(snapshot,"|th2,"));assert(strstr(snapshot,"|footer1,"));
    API.set_param(instance,"role","Conductor");API.get_param(instance,"pad_view",snapshot,sizeof(snapshot));
    assert(strstr(snapshot,"|targets1,62,62"));
    API.destroy_instance(instance);
}
static void effective_targets_in_every_color_mode(void){
    Inst *instance=fixture();hb_set_shared_follower_scale(1);
    instance->boundary_buffer_ms=0;instance->next_anti_buffer_ms=0;
    g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,0)};
    API.set_param(instance,"trail_enable","1");
    char pads[65];for(int slot=0;slot<32;slot++)snprintf(pads+slot*2,3,"%02x",60+slot%12);
    API.set_param(instance,"pad_preview_inputs",pads);
    char reference[16384],view[16384],expected[256];
    const char *modes[]={"Current","Lookahead","Both","Full Lookahead","Both Full Lookahead"};
    const double phases[]={1.0,1.9,2.0};
    for(int travel=0;travel<8;travel++)for(int chromatic=0;chromatic<2;chromatic++){
        instance->travel_map=travel;instance->chromatic_map=chromatic;
        for(int shift=0;shift<2;shift++)for(int step=0;step<3;step++){
            position=phases[step];instance->next_lookahead=shift?4:0;
            g_bus.observed_harmony=g_bus.next_model[step==2?1:0].harmony;
            hb_effective_write(g_bus.observed_harmony);
            API.set_param(instance,"pad_display","Effective");
            API.get_param(instance,"pad_view",reference,sizeof(reference));
            const char *targets=strstr(reference,"|targets1,");assert(targets);
            size_t size=strcspn(targets+1,"|")+1;assert(size<sizeof(expected));
            memcpy(expected,targets,size);expected[size]=0;
            for(int mode=0;mode<5;mode++){
                API.set_param(instance,"pad_display",modes[mode]);
                API.get_param(instance,"pad_view",view,sizeof(view));
                targets=strstr(view,"|targets1,");assert(targets);
                if(strncmp(expected,targets,size))fprintf(stderr,"trail mismatch travel%d chromatic%d shift%d phase%.1f mode%s\n",travel,chromatic,shift,position,modes[mode]);
                assert(!strncmp(expected,targets,size));
            }
        }
    }
    /* Partial lookahead must actually change the effective trail targets. */
    position=1.9;instance->travel_map=0;instance->chromatic_map=0;
    g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    API.set_param(instance,"pad_display","Both Full Lookahead");
    instance->next_lookahead=0;API.get_param(instance,"pad_view",reference,sizeof(reference));
    instance->next_lookahead=4;API.get_param(instance,"pad_view",view,sizeof(view));
    assert(strncmp(strstr(reference,"|targets1,"),strstr(view,"|targets1,"),20));
    position=1.0;instance->next_lookahead=0;hb_trail_heard(instance,61);
    unsigned generation=instance->trail_chord;double onset=instance->trail_at[60];
    position=1.9;instance->next_lookahead=4;hb_trail_boundary(instance);
    assert(instance->trail_chord==generation&&instance->trail_chord_at[60]==generation);
    position=2.0;hb_trail_boundary(instance);
    assert(instance->trail_chord==generation+1&&instance->trail_chord_at[60]==generation);
    assert(instance->trail_at[60]==onset); /* Expire the window, never rewrite history. */
    API.destroy_instance(instance);
}
static void arp_once(void){
    Inst *instance=fixture();hb_effective_write(chord(0,0,0));
    API.set_param(instance,"chord_mode","Scale Root");API.set_param(instance,"arp_playback","Repeat Arp");
    assert(instance->player.config.playback==1);
    play(instance,60);assert(instance->trail_valid[60]);double onset=instance->trail_at[60];
    uint8_t out[128][3];int lens[128];
    for(int i=0;i<100;i++){position+=.05;API.tick(instance,1200,48000,out,lens,128);}
    assert(instance->trail_at[60]==onset);assert(!instance->trail_previous_valid[60]);
    assert(!instance->trail_valid[64]&&!instance->trail_valid[67]);
    API.destroy_instance(instance);
}
static void delayed_tag(void){
    hb_motion_route *route=calloc(1,sizeof(*route));assert(route);
    route->rhythm_enabled=1;route->rhythm_delay=1;route->trail_in=63;
    hb_mo_push(route,0x90,67,100);route->trail_in=0;
    uint8_t message[3];assert(!hb_mo_pop(route,message));assert(!route->trail_out);
    route->rhythm_now=1;assert(hb_mo_pop(route,message));assert(route->trail_out==63);
    assert(!hb_mo_pop(route,message));assert(!route->trail_out);free(route);
}
static void repeat_history(void){
    Inst *instance=fixture();hb_effective_write(chord(0,0,0));
    instance->motion_beat=1;hb_trail_heard(instance,61);
    instance->motion_beat=1.01;hb_trail_heard(instance,61);
    assert(instance->trail_previous_valid[60]);assert(instance->trail_previous_at[60]==1);
    char snapshot[16384];API.get_param(instance,"trail_history",snapshot,sizeof(snapshot));
    assert(strstr(snapshot,"th2,"));assert(strstr(snapshot,";60,1.010000,"));
    API.set_param(instance,"trail_clear","1");
    assert(!instance->trail_valid[60]&&!instance->trail_previous_valid[60]);
    hb_trail_heard(instance,61);assert(!instance->trail_previous_valid[60]);
    API.destroy_instance(instance);
}
int main(void){repeat_history();mapped_target();preview_target();effective_targets_in_every_color_mode();arp_once();delayed_tag();puts("pad trails: effective targets in every color mode, partial lookahead, stable history, arp once, delayed onset pass");}
