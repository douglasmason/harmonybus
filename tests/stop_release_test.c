#define main chord_suite_main
#include "chord_player_test.c"
#undef main

static int local_balance[128];
static void collect(int count){
    for(int index=0;index<count;index++){
        int kind=output[index][0]&0xf0,pitch=output[index][1];
        if(kind==0x90&&output[index][2])local_balance[pitch]++;
        if(kind==0x80||(kind==0x90&&!output[index][2]))local_balance[pitch]--;
    }
}
static void stop_case(int role,int generated,int baked,int explicit_stop){
    Inst *instance=fixture();
    memset(local_balance,0,sizeof(local_balance));
    API.set_param(instance,"role",role?"Follower":"Conductor");
    API.set_param(instance,"boundary_buffer_ms","0 ms");
    API.set_param(instance,"master_transpose","D");
    API.set_param(instance,"chord_mode",generated?"Scale Degree":"Off");
    API.set_param(instance,"arp_phase","Free");
    API.set_param(instance,"arp_playback",generated==2?"Repeat Arp":generated==3?"Strum":"Together");
    API.set_param(instance,"strum_spread","400");
    advance(instance,1,1); /* transport running is observed before Pause */
    instance->movy_playback=1;instance->movy_passthrough=baked;
    uint8_t down[]={0x90,60,100};
    collect(API.process_midi(instance,down,3,output,lengths,64));
    for(int tick_index=0;tick_index<16;tick_index++)collect(advance(instance,2,1));
    int sounded=0;for(int pitch=0;pitch<128;pitch++)sounded+=local_balance[pitch]>0;
    assert(sounded>0);
    if(explicit_stop){uint8_t stop=0xfc;collect(API.process_midi(instance,&stop,1,output,lengths,1));}
    transport=1;position=-1;
    for(int tick_index=0;tick_index<32;tick_index++)collect(advance(instance,2,1));
    for(int pitch=0;pitch<128;pitch++)if(local_balance[pitch]>0){fprintf(stderr,"stuck role=%d generated=%d baked=%d explicit=%d pitch=%d balance=%d\n",role,generated,baked,explicit_stop,pitch,local_balance[pitch]);assert(0);}
    int route_balance[128]={0};
    for(int index=0;index<render_count;index++){
        int kind=rendered[index][1]&0xf0,pitch=rendered[index][2];
        if(kind==0x90&&rendered[index][3])route_balance[pitch]++;
        if(kind==0x80||(kind==0x90&&!rendered[index][3]))route_balance[pitch]--;
    }
    for(int pitch=0;pitch<128;pitch++)assert(route_balance[pitch]<=0);
    assert(instance->follower_queue_count==0&&!instance->player.sounding_count);
}
int main(void){
    for(int direct=0;direct<2;direct++){
        stop_case(0,0,0,direct);
        stop_case(0,1,1,direct);
        stop_case(1,0,0,direct);
        for(int generated=1;generated<=3;generated++){stop_case(0,generated,0,direct);stop_case(1,generated,0,direct);}
    }
    Inst *pending=fixture();
    advance(pending,1,1);
    API.set_param(pending,"boundary_buffer_ms","500 ms");
    midi(pending,1,60);
    transport=1;position=-1;
    for(int tick_index=0;tick_index<300;tick_index++){
        int count=advance(pending,1,1);
        for(int index=0;index<count;index++)assert((output[index][0]&0xf0)!=0x90||!output[index][2]);
    }
    assert(pending->follower_queue_count==0);
    puts("Stop: raw, baked and generated conductor/follower voices release locally and on render route with capacity one");
    return 0;
}
