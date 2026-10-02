#define main previous_chord_player_tests
#include "chord_player_test.c"
#undef main
static void configure(Inst *instance){
    API.set_param(instance,"motion_lane","1");
    API.set_param(instance,"motion_operation","Auto Chord Repeat");
    API.set_param(instance,"chord_form","Seventh");
    assert(instance->motion.lanes[0].operation==HB_MO_AUTO_CHORD_REPEAT);
    assert(!instance->motion.lanes[0].enabled&&!instance->player.repeat_override);
}
static void release_and_restore(void){
    Inst *instance=fixture();configure(instance);
    char before[16384],during[16384],label[64];
    API.get_param(instance,"state",before,sizeof(before));
    API.set_param(instance,"motion_gesture_1","Touch");
    assert(instance->player.repeat_override&&hb_cp_mode(&instance->player)==2&&hb_cp_playback(&instance->player)==1);
    assert(!instance->player.config.mode&&!instance->player.config.playback);
    API.get_param(instance,"chord_mode",label,sizeof(label));assert(!strcmp(label,"Off"));
    API.get_param(instance,"state",during,sizeof(during));assert(!strcmp(before,during));
    midi(instance,1,60);advance(instance,1,64);
    assert(hb_cp_held(&instance->player)==1);
    assert(instance->player.keys[0].count==4);
    int notes_on=0;
    for(int step=0;step<12;step++){
        int count=advance(instance,125,64);
        for(int event=0;event<count;event++)notes_on+=(output[event][0]&0xf0)==0x90&&output[event][2]>0;
    }
    assert(notes_on>1);
    API.set_param(instance,"motion_gesture_1","Up,500");
    assert(!instance->player.repeat_override&&!hb_cp_enabled(&instance->player));
    assert(!hb_cp_held(&instance->player));
    for(int step=0;step<8;step++){
        int count=advance(instance,125,64);
        for(int event=0;event<count;event++)assert((output[event][0]&0xf0)!=0x90||!output[event][2]);
    }
    assert(!instance->player.sounding_count);
    midi(instance,0,60);midi(instance,1,60);advance(instance,1,64);
    assert(!hb_cp_held(&instance->player)&&instance->follower_held[60]);
    API.destroy_instance(instance);
}
static void latch_settings_and_overlap(void){
    Inst *instance=fixture();configure(instance);
    API.set_param(instance,"chord_mode","Scale Degree");
    API.set_param(instance,"arp_playback","Once");
    API.set_param(instance,"motion_gesture_1","Touch");API.set_param(instance,"motion_gesture_1","Up,50");
    assert(instance->player.repeat_override&&hb_cp_mode(&instance->player)==1);
    API.set_param(instance,"chord_form","Ninth");assert(instance->player.config.size==4);
    API.set_param(instance,"motion_lane","2");API.set_param(instance,"motion_operation","Auto Chord Repeat");
    API.set_param(instance,"motion_hold_2","On");
    API.set_param(instance,"motion_gesture_1","Touch");API.set_param(instance,"motion_gesture_1","Up,50");
    assert(instance->player.repeat_override); /* Other lane still owns the gate. */
    API.set_param(instance,"motion_hold_2","Off");
    assert(!instance->player.repeat_override&&hb_cp_mode(&instance->player)==1);
    assert(hb_cp_playback(&instance->player)==2&&instance->player.config.size==4);
    char state[16384];API.get_param(instance,"state",state,sizeof(state));
    API.destroy_instance(instance);instance=API.create_instance("",0);API.set_param(instance,"state",state);
    assert(instance->motion.lanes[1].operation==HB_MO_AUTO_CHORD_REPEAT&&!instance->player.repeat_override);
    API.destroy_instance(instance);
}
static void automatic_gate(void){
    Inst *instance=fixture();configure(instance);
    API.set_param(instance,"motion_every","2");API.set_param(instance,"motion_from","2");
    API.set_param(instance,"motion_enabled","On");
    assert(!instance->player.repeat_override);
    position=4;advance(instance,0,64);assert(instance->player.repeat_override);
    unsigned long long events[HB_MOTION_LANES+1];
    hb_mo_capture(&instance->motion,4,4,60,events);
    assert(!(events[0]&HB_MO_RECORDED)); /* Track mode remains a live gate on replay. */
    position=8;advance(instance,0,64);assert(!instance->player.repeat_override);
    API.destroy_instance(instance);
}
static void arm_target_once(Inst *instance){
    API.set_param(instance,"motion_gesture_32","Knob,1000");
    API.set_param(instance,"motion_gesture_32","Up,40,1040");
    assert(instance->motion.gesture_once&(1ULL<<31));assert(instance->player.repeat_override);
}
static void target_release_once(void){
    Inst *instance=fixture();arm_target_once(instance);
    /* No approach is required: the whole first target hold owns the arp. */
    midi(instance,1,60);int attacks=0;
    for(int step=0;step<8;step++){
        int count=advance(instance,125,64);
        for(int event=0;event<count;event++)attacks+=(output[event][0]&0xf0)==0x90&&output[event][2]>0;
        assert(instance->player.repeat_override);
    }
    assert(attacks>1);
    midi(instance,1,64);midi(instance,0,64);advance(instance,125,64);assert(instance->player.repeat_override);
    uint8_t wrong_channel[3]={0x81,60,0};API.process_midi(instance,wrong_channel,3,output,lengths,64);assert(instance->player.repeat_override);
    midi(instance,1,67);midi(instance,0,60);assert(!instance->player.repeat_override); /* first target, not last held pad */
    assert(!(instance->motion.gesture_once&(1ULL<<31)));
    for(int step=0;step<4;step++){int count=advance(instance,125,64);for(int event=0;event<count;event++)assert((output[event][0]&0xf0)!=0x90||!output[event][2]);}
    midi(instance,0,67);API.destroy_instance(instance);
    /* Approach releases and recorded notes never consume the live one-shot. */
    instance=fixture();instance->approach_layout=1;instance->preview_count=32;arm_target_once(instance);
    API.set_param(instance,"hb_movy_input_approach","96,-36,3");midi(instance,1,96);advance(instance,125,64);midi(instance,0,96);advance(instance,125,64);
    assert(instance->player.repeat_override&&!instance->motion.gesture_target_owner[31]);
    instance->movy_playback=1;midi(instance,1,62);midi(instance,0,62);instance->movy_playback=0;advance(instance,125,64);
    assert(instance->player.repeat_override&&!instance->motion.gesture_target_owner[31]);
    midi(instance,1,60);advance(instance,250,64);assert(instance->player.repeat_override);
    instance->movy_playback=1;midi(instance,0,60);instance->movy_playback=0;advance(instance,125,64);assert(instance->player.repeat_override);
    uint8_t zero_velocity[3]={0x90,60,0};API.process_midi(instance,zero_velocity,3,output,lengths,64);assert(!instance->player.repeat_override);
    /* Explicit permanent latch still survives a complete target gesture. */
    API.set_param(instance,"motion_gesture_32","LatchOn");midi(instance,1,60);midi(instance,0,60);advance(instance,125,64);assert(instance->player.repeat_override);
    API.set_param(instance,"motion_gesture_32","LatchOff");assert(!instance->player.repeat_override);
    arm_target_once(instance);midi(instance,1,60);uint8_t stop=0xfc;API.process_midi(instance,&stop,1,output,lengths,64);advance(instance,0,64);
    assert(!instance->motion.gesture_target_owner[31]&&!instance->player.repeat_override);
    API.destroy_instance(instance);
}
int main(void){target_release_once();automatic_gate();release_and_restore();latch_settings_and_overlap();puts("Auto Chord Repeat: repeated MIDI, hold/release, latch, overlap, base settings, persistence and no stuck notes pass");}
