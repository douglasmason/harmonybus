#define HB_NOTE_BLEND_TEST_HELPERS
#include "note_blend_test.c"
static void field_tick(Inst *instance){
    uint8_t output[64][3];int lengths[64];API.tick(instance,480,48000,output,lengths,64);
}
static void assign_field(Inst *instance,int lane,const char *operation,const char *amount){
    char key[64];snprintf(key,sizeof(key),"motion_operation_%d",lane);API.set_param(instance,key,operation);
    snprintf(key,sizeof(key),"motion_control_%d",lane);API.set_param(instance,key,amount);
}
static void gesture_field(Inst *instance,int lane,const char *gesture){
    char key[64];snprintf(key,sizeof(key),"motion_gesture_%d",lane);API.set_param(instance,key,gesture);
}
int main(void){
    Inst *instance=setup();instance->travel_map=7;instance->content_map=2;
    API.set_param(instance,"chord_mode","Off");API.set_param(instance,"field_glide","0");
    assign_field(instance,1,"Field Rate","0");
    gesture_field(instance,1,"Knob,1000");gesture_field(instance,1,"Up,40,1040");field_tick(instance);
    assert(g_hb_field.rate==1&&instance->motion.gesture_once==1);
    production_event(instance,0x90,60,100);assert(g_hb_field.rate==0&&instance->motion.gesture_input_started==1);
    production_event(instance,0x90,64,100);production_event(instance,0x80,60,0);assert(g_hb_field.rate==0);
    production_event(instance,0x80,64,0);field_tick(instance);assert(g_hb_field.rate==1&&!instance->motion.gesture_once);
    API.set_param(instance,"motion_control_1","-100");gesture_field(instance,1,"LatchOn");field_tick(instance);assert(g_hb_field.rate==-1);
    API.set_param(instance,"field_gesture","50,100");field_tick(instance);assert(g_hb_field.rate==-.5);
    gesture_field(instance,1,"LatchOff");field_tick(instance);assert(g_hb_field.rate==.5);
    API.set_param(instance,"field_release","1");field_tick(instance);assert(g_hb_field.rate==1);
    assign_field(instance,2,"Field Zoom","200");gesture_field(instance,2,"Knob,2000");field_tick(instance);assert(g_hb_field.zoom==2);
    gesture_field(instance,2,"Up,500,2500");field_tick(instance);assert(g_hb_field.zoom==1);
    assign_field(instance,3,"Field Output","75");gesture_field(instance,3,"LatchOn");field_tick(instance);assert(hb_nf_output(2,.1)==.75);
    const char *released="fo1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1";
    API.set_param(instance,"field_outputs_remote","fo1,-1,-1,2000,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1");field_tick(instance);assert(fabs(hb_nf_output(2,.1)-.2)<1e-9);
    API.set_param(instance,"field_outputs_remote",released);field_tick(instance);assert(hb_nf_output(2,.1)==.75);
    /* Assignments are shared, activations are per-track. Ending one preserves another. */
    Inst *other=API.create_instance("",0);API.set_param(other,"role","Follower");
    gesture_field(instance,2,"LatchOn");gesture_field(other,2,"LatchOn");field_tick(instance);assert(g_hb_field.zoom==2);
    gesture_field(instance,2,"LatchOff");field_tick(instance);assert(g_hb_field.zoom==2);
    gesture_field(other,3,"LatchOn");gesture_field(instance,3,"LatchOff");field_tick(instance);assert(hb_nf_output(2,.1)==.75);
    API.destroy_instance(other);field_tick(instance);assert(hb_nf_output(2,.1)==.1&&g_hb_field.zoom==1);
    gesture_field(instance,3,"LatchOn");field_tick(instance);assert(hb_nf_output(2,.1)==.75);
    /* Pressure uses the existing neutral-to-target mapping, including reverse. */
    gesture_field(instance,3,"LatchOff");assign_field(instance,1,"Field Rate","0");
    instance->motion.pressure_active=1;instance->motion.pressure_value[0]=.25;
    hb_field_operations_sync();hb_nf_advance(.01,120);assert(g_hb_field.rate==.75);
    instance->motion.pressure_value[0]=1;hb_field_operations_sync();hb_nf_advance(.01,120);assert(g_hb_field.rate==0);
    instance->motion.pressure_active=0;field_tick(instance);assert(g_hb_field.rate==1);
    char state[65536],metadata[65536];API.get_param(instance,"state",state,sizeof(state));
    gesture_field(instance,3,"LatchOn");API.set_param(instance,"state",state);field_tick(instance);assert(!g_hb_field_operation_outputs);
    API.get_param(instance,"chain_params",metadata,sizeof(metadata));assert(strstr(metadata,"Field Output 3 %"));
    gesture_field(instance,3,"LatchOn");
    API.set_param(instance,"hb_movy_block","100,480,48000,1");assert(!g_hb_field_operation_outputs);
    API.set_param(instance,"hb_movy_block","101,480,48000,0");assert(g_hb_field_operation_outputs==4);
    double before=g_hb_field.position[0];API.set_param(instance,"hb_movy_block","101,480,48000,0");assert(g_hb_field.position[0]==before);
    hb_stop_instance_note_state(instance);API.set_param(instance,"hb_movy_block","102,480,48000,0");assert(!g_hb_field_operation_outputs);
    API.destroy_instance(instance);
    puts("Field operations: shared arm/hold/latch, phrase completion, pressure, leased input priority, multi-track cleanup, state, bypass and single clock pass");
}
