#define HB_PRESSURE_TEST_HELPERS
#include "approach_pressure_test.c"

static void choose(Inst *instance,int steps){
    API.set_param(instance,"motion_operation_1","Rotate");
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_enabled","Off");
    char value[16];snprintf(value,sizeof(value),"%d",steps);API.set_param(instance,"motion_control_1",value);
}
static int resolve(Inst *instance,int source){
    uint8_t message[3]={0x90,(uint8_t)source,100};int pitch,velocity,pan,skip;double off;
    hb_motion_values(instance,message,&pitch,&velocity,&pan,&off,&skip);assert(!skip);return pitch;
}
static int note_edge(Inst *instance,int source,int on,int expected){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];int lengths[64];
    int count=API.process_midi(instance,message,3,output,lengths,64);
    count+=API.tick(instance,64,48000,output+count,lengths+count,64-count);
    int matches=0;
    for(int index=0;index<count;index++)if((output[index][0]&240)==(on?0x90:0x80)){
        if(output[index][1]==expected)matches++;
    }
    if(matches!=1){fprintf(stderr,"edge source=%d on=%d expected=%d count=%d\n",source,on,expected,count);for(int index=0;index<count;index++)fprintf(stderr,"  %02x %d %d\n",output[index][0],output[index][1],output[index][2]);}
    assert(matches==1);return count;
}
static void gestures(int direction){
    Inst *instance=setup();configure(instance,HB_PRESSURE_HOLD,0);choose(instance,4*direction);
    API.set_param(instance,"follower_scale","Major");instance->travel_map=7;instance->content_map=1;
    press(instance,96);pressure(instance,96,64);assert(amount(instance)==2*direction);
    pressure(instance,96,80);assert(amount(instance)==2*direction);
    pressure(instance,96,86);assert(amount(instance)==3*direction);
    pressure(instance,96,80);assert(amount(instance)==3*direction);
    pressure(instance,96,70);assert(amount(instance)==2*direction);
    pressure(instance,96,0);assert(amount(instance)==0&&instance->approach_rows.pressure[3].down);
    /* Onset pitches retain their original OFF even as the pressure changes. */
    pressure(instance,96,32);assert(amount(instance)==direction);
    int first=direction>0?62:59,second=direction>0?67:60;
    note_edge(instance,60,1,first);
    pressure(instance,96,64);note_edge(instance,64,1,second);
    input(instance,96,0);assert(!instance->motion.pressure_active);
    note_edge(instance,60,0,first);note_edge(instance,64,0,second);
    API.set_param(instance,"approach_pressure_mode","3");
    press(instance,96);pressure(instance,96,127);input(instance,96,0);assert(amount(instance)==4*direction);
    press(instance,96);pressure(instance,96,32);input(instance,96,0);assert(amount(instance)==direction);
    press(instance,96);input(instance,96,0);assert(!instance->motion.pressure_active);
    API.set_param(instance,"approach_pressure_reverse","1");
    press(instance,96);assert(amount(instance)==4*direction);
    pressure(instance,96,96);input(instance,96,0);assert(amount(instance)==direction);
    press(instance,96);input(instance,96,0);assert(!instance->motion.pressure_active);
    API.set_param(instance,"approach_pressure_mode","2");API.set_param(instance,"approach_pressure_reverse","0");
    API.set_param(instance,"approach_pressure_prepared","50");
    press(instance,96);input(instance,96,0);assert(amount(instance)==2*direction);
    char readout[64],metadata[196608],saved[32768];
    API.get_param(instance,"approach_pressure_value",readout,sizeof(readout));assert(strstr(readout,"steps"));
    API.get_param(instance,"chain_params",metadata,sizeof(metadata));assert(strstr(metadata,"Collection Steps"));
    /* Existing per-note automation captures the resolved integer outcome. */
    unsigned long long actions[HB_MOTION_LANES+1];hb_mo_capture(&instance->motion,0,0,60,actions);
    assert(actions[0]&HB_MO_RECORDED);assert(hb_mo_word_operation(actions[0])==HB_MO_ROTATE);
    assert((int32_t)(uint32_t)actions[0]==2000*direction);
    API.get_param(instance,"state",saved,sizeof(saved));API.set_param(instance,"state",saved);
    assert(instance->motion.lanes[0].operation==HB_MO_ROTATE&&!instance->motion.pressure_active&&!instance->motion.pressure_rotate[0]);
    press(instance,96);pressure(instance,96,127);hb_stop_instance_note_state(instance);
    assert(!instance->motion.pressure_active&&!instance->motion.pressure_rotate[0]);
    API.destroy_instance(instance);
}
static void collections(void){
    Inst *instance=setup();configure(instance,HB_PRESSURE_HOLD,0);choose(instance,1);
    API.set_param(instance,"follower_scale","Major");instance->content_map=1;
    press(instance,96);pressure(instance,96,127);
    assert(resolve(instance,60)==62&&resolve(instance,64)==65&&resolve(instance,67)==69);
    choose(instance,-1);hb_ap_refresh(instance);
    assert(resolve(instance,60)==59&&resolve(instance,64)==62&&resolve(instance,67)==65);
    /* Collection choice is the existing Rotate policy, including six/eight tones. */
    input(instance,96,0);API.set_param(instance,"follower_scale","Whole Tone");choose(instance,6);
    g_bus.observed_harmony=(hb_harmony_t){.valid=1,.root_pc=0,.pitch_mask=0x555,.chord_index=HB_HARMONY_EXPLICIT_TONES|HB_HARMONY_FIXED_SCALE,.intent_scale=0x555};hb_effective_write(g_bus.observed_harmony);
    press(instance,96);pressure(instance,96,127);assert(resolve(instance,60)==72);
    input(instance,96,0);API.set_param(instance,"follower_scale","Half-Whole Diminished");choose(instance,8);
    g_bus.observed_harmony=(hb_harmony_t){.valid=1,.root_pc=0,.pitch_mask=0x6db,.chord_index=HB_HARMONY_EXPLICIT_TONES|HB_HARMONY_FIXED_SCALE,.intent_scale=0x6db};hb_effective_write(g_bus.observed_harmony);
    press(instance,96);pressure(instance,96,127);assert(resolve(instance,60)==72);
    g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);
    instance->content_map=0;choose(instance,1);hb_ap_refresh(instance);assert(resolve(instance,60)==64);
    instance->content_map=2;assert(resolve(instance,60)==61);
    /* Field Amount bounds remain signed; zero is an exact neutral point. */
    hb_motion_lane *lane=&instance->motion.lanes[0];lane->field_target=2;lane->field_low=-4;lane->field_high=4;
    pressure(instance,96,32);assert(amount(instance)==-2);
    pressure(instance,96,64);assert(amount(instance)==0);
    pressure(instance,96,96);assert(amount(instance)==2);
    pressure(instance,96,0);assert(amount(instance)==0);
    lane->field_target=0;choose(instance,400);hb_ap_refresh(instance);assert(lane->amount==24);
    pressure(instance,96,127);assert(amount(instance)==24);
    API.set_param(instance,"motion_amount","-400");hb_ap_refresh(instance);assert(amount(instance)==-24);
    API.set_param(instance,"role","Off");assert(!instance->motion.pressure_active&&!instance->motion.pressure_rotate[0]);
    API.destroy_instance(instance);
}
int main(void){
    gestures(1);gestures(-1);collections();
    puts("pressure Rotate: scale/chord/chromatic collections, signed hysteresis, onset releases, latch/reverse, Field bounds, capture, persistence and Stop pass");
}
