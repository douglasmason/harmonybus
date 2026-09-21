#define main chord_player_regression_main
#include "chord_player_test.c"
#undef main

static void attack(Inst *instance,int velocity,int expected){
    uint8_t note[3]={0x90,60,(uint8_t)velocity};
    int before=render_count;
    int count=API.process_midi(instance,note,3,output,lengths,64);
    count+=API.tick(instance,128,48000,output+count,lengths+count,64-count);
    if(count!=1||output[0][2]!=velocity)fprintf(stderr,"gain=%d velocity=%d expected=%d count=%d out=%d\n",instance->render_velocity_gain,velocity,expected,count,output[0][2]);assert(count==1&&output[0][2]==velocity); /* local velocity and source untouched */
    if(expected)assert(render_count==before+1&&rendered[before][3]==expected);
    else assert(render_count==before);
    note[0]=0x80;note[2]=37;before=render_count;
    API.process_midi(instance,note,3,output,lengths,64);API.tick(instance,128,48000,output,lengths,64);
    assert(render_count==before+1&&(rendered[before][1]&0xf0)==0x80); /* release at any gain */
}
int main(void){
    Inst *instance=fixture();char state[8192],value[64];
    API.set_param(instance,"boundary_buffer_ms","0 ms");
    attack(instance,127,127);
    API.set_param(instance,"render_velocity_gain","0.5");attack(instance,127,64);
    API.set_param(instance,"hb_movy_playback","1");attack(instance,100,50);
    API.set_param(instance,"hb_movy_playback","0");
    API.get_param(instance,"state",state,sizeof(state));assert(strstr(state,";rv1,5000"));
    API.set_param(instance,"render_velocity_gain","2");attack(instance,100,127);attack(instance,40,80);
    API.set_param(instance,"render_velocity_gain","0");attack(instance,100,0);
    API.set_param(instance,"render_velocity_gain","0.0001");attack(instance,1,1);
    API.set_param(instance,"state",state);attack(instance,100,50);
    API.set_param(instance,"render_velocity_gain","nan");API.set_param(instance,"render_velocity_gain","-1");
    API.set_param(instance,"render_velocity_gain","4.1");API.set_param(instance,"render_velocity_gain","0.2junk");
    API.get_param(instance,"render_velocity_gain",value,sizeof(value));assert(!strcmp(value,"0.5000"));
    Inst *other=API.create_instance("",NULL);assert(other->render_velocity_gain==10000);
    API.set_param(instance,"state","hb16,1,0,0,25,1,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,25,0,0,1,0");
    assert(instance->render_velocity_gain==10000); /* old sets remain unchanged */
    /* Scaling is downstream of operations and applies to delayed arp/echo attacks too. */
    API.set_param(instance,"render_velocity_gain","0.5");
    uint8_t packet[4]={0x29,0x93,60,80};int before=render_count;
    hb_mo_push(&instance->motion_render,0x93,60,80);hb_motion_flush_render(instance);
    assert(render_count==before+1&&rendered[before][3]==40&&packet[3]==80);
    API.set_param(other,"role","Receiver");API.set_param(other,"source_channel","4");
    hb_send_render_raw(instance,packet,1);
    assert(other->receiver_count==1&&other->receiver_queue[0].velocity==40);
    puts("render velocity: live/replay, local isolation, zero/release, gain limits, receiver routing and state pass");
}
