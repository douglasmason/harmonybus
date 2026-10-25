#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
static int playing=1;
static int test_clock(void){return playing?MOVE_CLOCK_STATUS_RUNNING:0;}
static void choose(Inst *instance){
    API.set_param(instance,"hb_movy_input_chord","10,1,0,1,0,1,12");
    uint8_t input[]={0x90,70,100};assert(hb_selection_input(instance,input,3));
}
int main(void){
    Inst *follower=fixture();host.get_clock_status=test_clock;
    Inst *conductor=API.create_instance("",0);API.set_param(conductor,"role","Conductor");
    API.set_param(conductor,"render_channel","Off");
    position=1;g_bus.clip_loop_end=8;hb_next_reset_knowledge();g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=chord(7,0,0)};
    g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    choose(conductor);assert(hb_render_harmony(follower).root_pc==10);
    hb_tick_conductor(conductor,128,48000);
    assert(g_bus.next_model_locked&&g_bus.next_model_count==2&&g_bus.observed_harmony.root_pc==0);
    assert(g_bus.next_model[0].harmony.root_pc==0&&g_bus.next_model[1].harmony.root_pc==7);
    char buffer[8192];API.get_param(follower,"pad_harmony",buffer,sizeof(buffer));
    unsigned current,effective,scale,look;int ready;
    assert(sscanf(buffer,"%u,%u,%u,%d",&current,&effective,&scale,&ready)==4);
    assert(current==0x424&&effective==0x424);
    position=4;hb_expire_live_selection(conductor);assert(!hb_live_current().valid);
    API.set_param(follower,"next_lookahead","3/4");position=1;choose(conductor);
    position=3.9;assert(hb_render_harmony(follower).root_pc==7); /* Recorded next remains available. */
    uint8_t off[]={0x80,70,0};assert(hb_selection_input(conductor,off,3));assert(!hb_live_current().valid);
    playing=0;position=-1;choose(conductor);assert(hb_render_harmony(follower).root_pc==10);
    API.get_param(follower,"pad_harmony",buffer,sizeof(buffer));
    assert(sscanf(buffer,"%u,%u,%u,%d",&current,&effective,&scale,&ready)==4);
    assert(current==0x424&&effective==0x424&&!ready);
    assert(hb_selection_input(conductor,off,3));assert(!hb_live_current().valid);
    choose(conductor);conductor->movy_playback=1;
    uint8_t recorded[]={0x90,67,100};assert(!hb_selection_input(conductor,recorded,3));
    assert(!hb_live_current().valid);conductor->movy_playback=0;
    API.destroy_instance(conductor);API.destroy_instance(follower);
    puts("Live chord overrides current only; recorded lookahead survives; stopped selection and Off pass");
}
