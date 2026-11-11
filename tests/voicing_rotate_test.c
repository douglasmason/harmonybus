/* Real generated/selected owners and pressure paths, including actual MIDI. */
#define HB_SPREAD_TEST_HELPERS
#include "chord_spread_test.c"

static void core_rotation(void){
    const int up[4][3]={{60,64,67},{64,67,72},{67,72,76},{72,76,79}};
    const int down[4][3]={{60,64,67},{55,60,64},{52,55,60},{48,52,55}};
    for(int steps=0;steps<4;steps++)for(int direction=-1;direction<=1;direction+=2){
        int notes[]={60,64,67};hb_voicing_rotate(notes,3,steps*direction);
        expect_notes(notes,direction>0?up[steps]:down[steps],3);
    }
    for(int voices=2;voices<=7;voices++)for(int base=0;base<128-3*voices;base++)for(int steps=-12;steps<=12;steps++){
        int notes[7],before[12]={0},after[12]={0};
        for(int voice=0;voice<voices;voice++){notes[voice]=base+voice*3;before[notes[voice]%12]++;}
        hb_voicing_rotate(notes,voices,steps);
        for(int voice=0;voice<voices;voice++){
            assert(notes[voice]>=0&&notes[voice]<128);if(voice)assert(notes[voice]>notes[voice-1]);
            after[notes[voice]%12]++;
        }
        expect_notes(before,after,12);
    }
    int wide[]={60,76,91};hb_voicing_rotate(wide,3,1);expect_notes(wide,(int[]){76,91,96},3);
    int ceiling[]={120,124,127};hb_voicing_rotate(ceiling,3,1);expect_notes(ceiling,(int[]){120,124,127},3);
    int floor[]={0,4,7};hb_voicing_rotate(floor,3,-1);expect_notes(floor,(int[]){0,4,7},3);
    int single=60;hb_voicing_rotate(&single,1,12);assert(single==60);
}
static void choose_rotation(Inst *instance,int steps){
    API.set_param(instance,"motion_operation_1","Voicing Rotate");
    assert(instance->motion.lanes[0].amount==3&&!instance->motion.lanes[0].enabled);
    char value[16];snprintf(value,sizeof(value),"%d",steps);API.set_param(instance,"motion_control_1",value);
}
static void rotation_pool(Inst *instance,const int *base,int steps,int spread){
    hb_cp_key *key=source_key(instance);int notes[HB_CP_VOICES];memcpy(notes,base,sizeof(notes));
    hb_voicing_rotate(notes,key->count,steps);hb_chord_spread(notes,key->count,spread);
    expect_notes(key->notes,notes,key->count);
}
static void rotation_owners(int selection,int arp,int direction){
    Inst *instance=fixture();API.set_param(instance,"chord_mode",selection?"Off":"Conductor Chord");
    if(selection){API.set_param(instance,"role","Conductor");API.set_param(instance,"render_channel","3");}
    API.set_param(instance,"chord_form","Triad");API.set_param(instance,"chord_inversion","Root");
    API.set_param(instance,"chord_register","0");API.set_param(instance,"arp_playback",arp?"Repeat Arp":"Together");
    API.set_param(instance,"arp_order",direction>0?"Up":"Down");choose_rotation(instance,direction);
    if(selection)API.set_param(instance,"hb_movy_input_chord","0,1,2,1,0,1,12");
    midi(instance,1,60);advance(instance,0,64);hb_cp_key *key=source_key(instance);assert(key->count==3);
    int base[HB_CP_VOICES];memcpy(base,key->notes,sizeof(base));
    double next=instance->player.next_beat,gate=instance->player.gate_beat;
    int step=instance->player.order_state.step,actions=instance->action_count,recorded_before=recorded_count;
    for(int trial=0;trial<3;trial++){
        int first=render_count;API.set_param(instance,"motion_hold_1","On");drain_spread(instance);rotation_pool(instance,base,direction,0);
        assert(instance->chord_rotation_steps==direction&&render_count>first);
        if(!arp){assert(render_count==first+2);assert(instance->player.sounding[0][direction>0?base[0]+12:base[2]-12]);}
        assert(instance->player.sounding_count==(arp?1:3));
        API.set_param(instance,"motion_hold_1","Off");drain_spread(instance);rotation_pool(instance,base,0,0);
        if(arp){assert(instance->player.next_beat==next&&instance->player.gate_beat==gate&&instance->player.order_state.step==step);}
    }
    assert(instance->action_count==actions&&recorded_count==recorded_before);
    API.set_param(instance,"motion_hold_1","On");
    API.set_param(instance,"motion_operation_2","Chord Spread");API.set_param(instance,"motion_control_2","2");
    API.set_param(instance,"motion_hold_2","On");drain_spread(instance);rotation_pool(instance,base,direction,2);
    API.set_param(instance,"motion_hold_2","Off");drain_spread(instance);rotation_pool(instance,base,direction,0);
    midi(instance,0,60);drain_spread(instance);assert(!instance->player.sounding_count);
    API.destroy_instance(instance);
}
static void rotation_pressure(int direction){
    Inst *instance=fixture();instance->approach_layout=1;instance->preview_count=32;
    API.set_param(instance,"approach_pressure_row","3");API.set_param(instance,"approach_pressure_lane","1");
    API.set_param(instance,"approach_pressure_mode","1");choose_rotation(instance,4*direction);
    API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Triad");
    API.set_param(instance,"chord_inversion","Root");API.set_param(instance,"arp_playback","Together");
    midi(instance,1,60);advance(instance,0,64);int base[HB_CP_VOICES];memcpy(base,source_key(instance)->notes,sizeof(base));
    Inst *peer=API.create_instance("",0);API.set_param(peer,"role","Follower");
    press_row(instance);squeeze(instance,127);drain_spread(instance);rotation_pool(instance,base,4*direction,0);
    assert(!peer->chord_rotation_steps&&!instance->chord_spread_steps&&!g_global_operation_transpose);
    squeeze(instance,64);drain_spread(instance);rotation_pool(instance,base,2*direction,0);
    squeeze(instance,80);assert(instance->chord_rotation_steps==2*direction);
    squeeze(instance,86);assert(instance->chord_rotation_steps==3*direction);
    midi(instance,0,96);drain_spread(instance);rotation_pool(instance,base,0,0);
    API.set_param(instance,"approach_pressure_mode","3");API.set_param(instance,"approach_pressure_reverse","1");
    press_row(instance);assert(instance->chord_rotation_steps==4*direction);
    squeeze(instance,80);midi(instance,0,96);drain_spread(instance);rotation_pool(instance,base,direction,0);
    press_row(instance);midi(instance,0,96);drain_spread(instance);rotation_pool(instance,base,0,0);
    API.set_param(instance,"approach_pressure_reverse","0");
    press_row(instance);squeeze(instance,127);midi(instance,0,96);drain_spread(instance);rotation_pool(instance,base,4*direction,0);
    press_row(instance);squeeze(instance,64);midi(instance,0,96);drain_spread(instance);rotation_pool(instance,base,2*direction,0);
    char metadata[196608],saved[32768],value[64];
    API.get_param(instance,"approach_pressure_value",value,sizeof(value));assert(strstr(value,"Rotate "));
    API.set_param(instance,"motion_lane","1");API.get_param(instance,"chain_params",metadata,sizeof(metadata));
    assert(strstr(metadata,"Voicing Rotate")&&strstr(metadata,"Rotation Steps"));
    unsigned long long captured[HB_MOTION_LANES+1];hb_mo_capture(&instance->motion,0,0,60,captured);assert(!(captured[0]&HB_MO_RECORDED));
    API.get_param(instance,"state",saved,sizeof(saved));API.set_param(instance,"state",saved);drain_spread(instance);
    assert(instance->motion.lanes[0].operation==HB_MO_VOICING_ROTATE&&instance->motion.lanes[0].amount==4*direction&&!instance->chord_rotation_steps);
    API.set_param(instance,"motion_gesture_1","LatchOn");assert(instance->chord_rotation_steps==4*direction);
    API.set_param(instance,"motion_gesture_1","LatchOff");assert(!instance->chord_rotation_steps);
    API.set_param(instance,"approach_pressure_mode","2");API.set_param(instance,"approach_pressure_prepared","50");
    press_row(instance);midi(instance,0,96);assert(instance->chord_rotation_steps==2*direction);
    position=-1;advance(instance,0,64);assert(!instance->chord_rotation_steps&&!instance->motion.pressure_active);
    API.set_param(instance,"motion_hold_1","On");API.set_param(instance,"role","Off");assert(!instance->chord_rotation_steps);
    API.destroy_instance(peer);API.destroy_instance(instance);
}
int main(void){
    core_rotation();for(int direction=-1;direction<=1;direction+=2){
        for(int selection=0;selection<2;selection++)for(int arp=0;arp<2;arp++)rotation_owners(selection,arp,direction);
        rotation_pressure(direction);
    }
    puts("Voicing Rotate: signed inversions, MIDI bounds, held/selected/arp owners, pressure hysteresis/latch/reverse, spread composition, persistence and cleanup pass");
}
