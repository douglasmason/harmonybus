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
    assert(strstr(snapshot,"|targets1,62,62"));assert(strstr(snapshot,"|th1,"));
    API.set_param(instance,"role","Conductor");API.get_param(instance,"pad_view",snapshot,sizeof(snapshot));
    assert(strstr(snapshot,"|targets1,62,62"));
    API.destroy_instance(instance);
}
static void arp_once(void){
    Inst *instance=fixture();hb_effective_write(chord(0,0,0));
    API.set_param(instance,"chord_mode","Scale Root");API.set_param(instance,"arp_playback","Repeat Arp");
    assert(instance->player.config.playback==1);
    play(instance,60);assert(instance->trail_valid[60]);double onset=instance->trail_at[60];
    uint8_t out[128][3];int lens[128];
    for(int i=0;i<100;i++){position+=.05;API.tick(instance,1200,48000,out,lens,128);}
    assert(instance->trail_at[60]==onset);
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
int main(void){mapped_target();preview_target();arp_once();delayed_tag();puts("pad trails: resolved target, stable history, arp once, delayed onset pass");}
