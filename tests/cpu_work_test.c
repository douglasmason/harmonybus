/* Compile-time instrumentation verifies shared work is not repeated per track. */
#define main chord_player_suite_main
#include "chord_player_test.c"
#undef main
static unsigned refresh_calls;
void __cyg_profile_func_enter(void *function,void *caller) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *function,void *caller) __attribute__((no_instrument_function));
void __cyg_profile_func_enter(void *function,void *caller){
    (void)caller;if(function==(void *)hb_movy_refresh)refresh_calls++;
}
void __cyg_profile_func_exit(void *function,void *caller){(void)function;(void)caller;}
int main(void){
    Inst *instances[16];instances[0]=fixture();
    for(int track=1;track<16;track++)instances[track]=API.create_instance("",0);
    for(int track=0;track<16;track++){
        API.set_param(instances[track],"role",track<4?"Conductor":"Follower");
        char message[128];snprintf(message,sizeof(message),"0,384,0,%d,1,1,96,%d,0",100+track,track);
        API.set_param(instances[track],"hb_movy_clip",message);
    }
    refresh_calls=0;
    /* The bridge sends the same barrier through every loaded chain. */
    for(int track=0;track<16;track++)API.set_param(instances[track],"hb_movy_block","1,128,44100");
    assert(refresh_calls==1);
    for(int track=0;track<16;track++)API.tick(instances[track],128,44100,output,lengths,64);
    assert(refresh_calls==1);
    for(int track=0;track<16;track++){
        char message[128];snprintf(message,sizeof(message),"12,384,0,%d,1,1,96,%d,0",100+track,track);
        API.set_param(instances[track],"hb_movy_clip",message);
    }
    API.set_param(instances[0],"hb_movy_block","2,128,44100");
    assert(refresh_calls==2&&g_movy_tick==12);
    /* MIDI arriving after metadata keeps its immediate refresh behavior. */
    API.set_param(instances[15],"hb_movy_clip","24,384,0,115,1,1,96,15,0");
    uint8_t on[]={0x90,60,100};
    API.process_midi(instances[0],on,3,output,lengths,64);
    assert(refresh_calls==3&&g_movy_tick==24);
    /* Hosts without the shared barrier still refresh on each ordinary tick. */
    g_conductor_block_ready=0;refresh_calls=0;
    API.tick(instances[0],128,44100,output,lengths,64);
    assert(refresh_calls==1);
    for(int track=0;track<16;track++)API.destroy_instance(instances[track]);
    puts("Shared CPU work: one timeline refresh per block, duplicate barrier suppression, new metadata and MIDI visibility, standalone tick fallback pass");
}
