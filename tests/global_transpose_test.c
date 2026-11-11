#define HB_PRESSURE_TEST_HELPERS
#include "approach_pressure_test.c"

static int heard[16][128],attacks,releases;
static void observe(const uint8_t *message){
    int channel=message[0]&15,pitch=message[1]&127;
    if((message[0]&240)==0x90&&message[2]){heard[channel][pitch]=1;attacks++;}
    if((message[0]&240)==0x80||((message[0]&240)==0x90&&!message[2])){heard[channel][pitch]=0;releases++;}
}
static void drain(Inst *instance){
    uint8_t output[1][3];int lengths[1];
    for(int attempt=0;attempt<256;attempt++){
        int count=API.tick(instance,64,48000,output,lengths,1);
        if(count)observe(output[0]);
    }
}
static void edge(Inst *instance,int source,int on){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];int lengths[64];
    int count=API.process_midi(instance,message,3,output,lengths,64);
    for(int index=0;index<count;index++)observe(output[index]);
    drain(instance);
}
static void core_boundaries(void){
    hb_output_transpose state={0};uint8_t message[3]={0x92,125,95};
    assert(hb_ot_message(&state,message));
    hb_ot_target(&state,12);assert(hb_ot_pending(&state,message)&&message[0]==0x82&&message[1]==125);
    assert(!hb_ot_pending(&state,message)); /* Outside MIDI range stays owned but silent. */
    hb_ot_target(&state,-12);assert(hb_ot_pending(&state,message)&&message[0]==0x92&&message[1]==113);
    hb_ot_target(&state,0); /* Reverse direction midway through a one-message drain. */
    while(hb_ot_pending(&state,message))observe(message);
    assert(state.heard[2][125]&&!state.heard[2][113]);
    hb_ot_target(&state,2);assert(hb_ot_pending(&state,message)&&message[0]==0x82);
    hb_ot_panic(&state);while(hb_ot_pending(&state,message))assert((message[0]&240)==0x80);
    for(int pitch=0;pitch<128;pitch++)assert(!state.heard[2][pitch]);
    memset(heard,0,sizeof(heard));
}
int main(void){
    core_boundaries();
    Inst *owner=setup();configure(owner,HB_PRESSURE_HOLD,0);
    API.set_param(owner,"motion_operation_1","Global Transpose");
    API.set_param(owner,"motion_control_1","12");
    owner->travel_map=7;owner->content_map=2;
    Inst *peer=API.create_instance("",0);API.set_param(peer,"role","Follower");
    peer->boundary_buffer_ms=peer->next_anti_buffer_ms=0;peer->travel_map=7;peer->content_map=2;
    edge(peer,60,1);edge(peer,64,1);assert(heard[0][60]&&heard[0][64]);
    press(owner,96);pressure(owner,96,127);drain(peer);
    assert(g_global_operation_transpose==12&&g_bus.global_transpose==0);
    assert(!heard[0][60]&&!heard[0][64]&&heard[0][72]&&heard[0][76]);
    pressure(owner,96,64);drain(peer);assert(g_global_operation_transpose==6);
    assert(heard[0][66]&&heard[0][70]&&!heard[0][72]&&!heard[0][76]);
    edge(peer,60,0);assert(!heard[0][66]&&heard[0][70]);
    input(owner,96,0);drain(peer);assert(g_global_operation_transpose==0&&heard[0][64]);
    edge(peer,64,0);assert(!heard[0][64]);
    /* A shared assignment is not multiplied by track count. Other live owners
       keep their contribution when the first controller releases. */
    API.set_param(owner,"motion_hold_1","On");API.set_param(peer,"motion_hold_1","On");
    assert(g_global_operation_transpose==12);
    API.set_param(owner,"motion_hold_1","Off");assert(g_global_operation_transpose==12);
    API.set_param(peer,"motion_hold_1","Off");assert(g_global_operation_transpose==0);
    peer->motion.lanes[0].enabled=1;hb_key_operations_sync();assert(g_global_operation_transpose==12);
    press(owner,96);pressure(owner,96,32);drain(peer);assert(g_global_operation_transpose==3);
    input(owner,96,0);assert(g_global_operation_transpose==12);
    peer->motion.lanes[0].enabled=0;hb_key_operations_sync();assert(g_global_operation_transpose==0);
    API.set_param(owner,"motion_gesture_1","LatchOn");assert(g_global_operation_transpose==12);
    API.set_param(owner,"motion_gesture_1","LatchOff");assert(g_global_operation_transpose==0);
    /* Existing row latch/reverse lifecycle, live base edits, persistence. */
    API.set_param(owner,"approach_pressure_mode","3");API.set_param(owner,"approach_pressure_reverse","1");
    press(owner,96);pressure(owner,96,80);input(owner,96,0);
    assert(g_global_operation_transpose==4);
    API.set_param(owner,"transpose","3");drain(owner);assert(g_bus.global_transpose==3);
    /* The legacy base setter intentionally clears runtime gestures. */
    assert(!owner->motion.pressure_active&&g_global_operation_transpose==0);
    API.set_param(owner,"transpose","0");
    press(owner,96);pressure(owner,96,80);input(owner,96,0);assert(g_global_operation_transpose==4);
    /* Final-output transposition never enters captured per-note actions. */
    unsigned long long actions[HB_MOTION_LANES+1];hb_mo_capture(&owner->motion,0,0,60,actions);
    assert(!(actions[0]&HB_MO_RECORDED));
    char metadata[65536];API.set_param(owner,"motion_lane","1");API.get_param(owner,"chain_params",metadata,sizeof(metadata));
    assert(strstr(metadata,"Global Transpose")&&strstr(metadata,"Global Shift (semitones)"));
    char saved[32768];API.get_param(owner,"state",saved,sizeof(saved));
    API.set_param(owner,"state",saved);drain(owner);assert(g_global_operation_transpose==0);
    API.set_param(owner,"motion_hold_1","On");assert(g_global_operation_transpose==12);
    API.destroy_instance(owner);assert(g_global_operation_transpose==0);
    API.destroy_instance(peer);
    puts("global transpose: held voices, overlap, pressure/reverse/latch, MIDI bounds, base/state and teardown passed");
}
