#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
int main(void){
    Inst *instance=fixture();char buffer[65536],state[16384];
    for(int lane=0;lane<16;lane++)assert(instance->motion.lanes[lane].operation==HB_MO_OFF);
    API.set_param(instance,"motion_lane","Approach Harmony 3: Connector Above");assert(instance->motion.selected==18);
    API.set_param(instance,"motion_operation","Velocity");assert(instance->motion.lanes[18].operation==HB_MO_CHROM_ABOVE);
    API.set_param(instance,"motion_lane","Step Seq 1: Off");API.set_param(instance,"motion_operation","Velocity");
    API.set_param(instance,"motion_control_1","-20");assert(instance->motion.selected==0&&instance->motion.lanes[0].amount==-20);
    API.get_param(instance,"motion_lane",buffer,sizeof(buffer));assert(!strcmp(buffer,"Step Seq 1: Velocity"));
    instance->travel_map=7;instance->content_map=1;instance->player.config.mode=1;API.set_param(instance,"chord_form","Seventh");API.set_param(instance,"motion_gesture_28","Touch,1000");API.set_param(instance,"motion_gesture_28","Up,50,1050");
    assert(instance->motion.enclosure_lane==27);assert(hb_mo_pending_lanes(&instance->motion)==(1ULL<<27));
    assert(played(instance,60)==tones(56,3,7,10));release(instance,60);
    assert(played(instance,60)==tones(61,4,7,10));release(instance,60);
    assert(played(instance,60)==tones(60,4,7,11));release(instance,60);assert(!instance->motion.enclosure);
    API.set_param(instance,"motion_gesture_33","Touch,2000");assert(instance->motion.held&(1ULL<<32));
    API.get_param(instance,"motion_named_lights",buffer,sizeof(buffer));assert(strtoull(buffer,0,10)&(1u<<16));
    API.set_param(instance,"motion_gesture_33","Cancel");assert(!(instance->motion.held&(1ULL<<32)));
    API.set_param(instance,"motion_lane","Chord Play 1: Chord Form");API.set_param(instance,"motion_amount","Ninth");
    API.get_param(instance,"state",state,sizeof(state));assert(strstr(state,";named1"));
    hb_motion_config restored={0};hb_mo_restore(&restored,state);
    assert(restored.lanes[30].operation==HB_MO_CHORD_FORM&&restored.lanes[30].amount==4);
    assert(restored.lanes[0].operation==HB_MO_VELOCITY&&restored.lanes[0].amount==-20);
    assert(restored.lanes[12].operation==HB_MO_OFF);
    hb_mo_restore(&restored,";ft1,1,2,3,4,13,14,15,16;mo1,0,3,0,2,0,0,3,3,0,100,0,0");
    assert(restored.lanes[0].operation==HB_MO_OCTAVE&&restored.lanes[12].operation==HB_MO_SECONDARY_VI);
    assert(restored.lanes[27].operation==HB_MO_CADENCE_TRITONE);
    API.destroy_instance(instance);puts("named controls: separate banks, fixed identity, amount turns, three-press rendering, 64-bit lights, saved and legacy state pass");
}
