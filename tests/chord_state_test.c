#define main existing_chord_tests
#include "chord_player_test.c"
#undef main
static void expect_value(Inst *instance,const char *key,const char *expected){char value[128];API.get_param(instance,key,value,sizeof(value));assert(!strcmp(value,expected));}
int main(void){
    Inst *instance=fixture();
    API.set_param(instance,"motion_lane","1");
    API.set_param(instance,"motion_operation","Chord/Arp State");
    API.set_param(instance,"motion_enabled","Off");
    API.set_param(instance,"motion_amount","Scale Degree Burst");
    hb_cp_config baseline=instance->player.config;
    API.set_param(instance,"chord_edit_target","Lane 1");
    expect_value(instance,"chord_mode","Rendered Note Root");
    expect_value(instance,"arp_start","Played / Pad");
    expect_value(instance,"arp_phase","First Note Free");
    assert(!instance->player.state_override);
    API.set_param(instance,"arp_gate","Half");
    API.set_param(instance,"arp_order","Shuffle Cycle Pin");
    expect_value(instance,"arp_order","Shuffle Cycle Pin");
    assert(!memcmp(&baseline,&instance->player.config,sizeof(baseline)));
    API.set_param(instance,"motion_hold_1","On");
    assert(instance->player.state_override&&hb_cp_mode(&instance->player)==1&&hb_cp_playback(&instance->player)==1);
    midi(instance,1,60);advance(instance,10,64);
    API.set_param(instance,"motion_hold_1","Off");advance(instance,0,64);
    assert(!instance->player.state_override);
    assert(!memcmp(&baseline,&instance->player.config,sizeof(baseline)));
    assert(!instance->player.sounding_count);
    API.set_param(instance,"chord_input","Root/Bass + Top");
    API.set_param(instance,"motion_hold_1","On");
    midi(instance,1,48);assert(!hb_cp_held(&instance->player));
    midi(instance,1,76);assert(hb_cp_held(&instance->player)==1);
    hb_cp_key *pair=NULL;for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used)pair=&instance->player.keys[index];
    assert(pair&&pair->notes[0]==48&&pair->notes[pair->count-1]==76&&pair->played_pitch==76);
    midi(instance,1,79);assert(hb_cp_held(&instance->player)==1);
    midi(instance,0,79);assert(hb_cp_held(&instance->player)==1);
    midi(instance,0,76);advance(instance,0,64);assert(!hb_cp_held(&instance->player)&&!instance->player.sounding_count);
    midi(instance,0,48);API.set_param(instance,"motion_hold_1","Off");
    API.set_param(instance,"chord_edit_target","Track Settings");
    API.set_param(instance,"motion_lane","2");API.set_param(instance,"motion_operation","Chord/Arp State");
    API.set_param(instance,"motion_enabled","Off");API.set_param(instance,"motion_amount","Current Harmony Burst");
    API.set_param(instance,"motion_hold_1","On");assert(hb_cp_mode(&instance->player)==1);
    API.set_param(instance,"motion_hold_2","On");assert(hb_cp_mode(&instance->player)==2);
    API.set_param(instance,"arp_rate","1/8");assert(hb_cp_settings(&instance->player)->rate==1);
    API.set_param(instance,"motion_hold_2","Off");assert(hb_cp_mode(&instance->player)==1);
    API.set_param(instance,"motion_hold_1","Off");assert(!instance->player.state_override);
    expect_value(instance,"arp_rate","1/8");
    char metadata[65536];assert(API.get_param(instance,"chain_params",metadata,sizeof(metadata))>0);
    assert(strstr(metadata,"Lane 1")&&strstr(metadata,"Lane 2"));
    API.set_param(instance,"chord_edit_target","Lane 2");API.set_param(instance,"chord_input","Root/Bass + Top");
    API.set_param(instance,"motion_hold_2","On");instance->retrigger_held=1;
    midi(instance,1,49);midi(instance,1,79);
    uint8_t changed_notes[3]={62,65,69};hb_commit_observed_harmony(hb_infer_harmony(changed_notes,3));
    hb_reharmonize_held_chords(instance);
    pair=NULL;for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used)pair=&instance->player.keys[index];
    assert(pair&&pair->notes[0]==49&&pair->notes[pair->count-1]==79&&pair->root_pc==2);
    midi(instance,0,49);midi(instance,0,79);API.set_param(instance,"motion_hold_2","Off");
    char state[131072];API.get_param(instance,"state",state,sizeof(state));
    Inst *copy=API.create_instance("",NULL);API.set_param(copy,"state",state);
    assert(copy->motion.lanes[0].chord_state_valid);
    assert(copy->motion.lanes[0].chord_state.order==6);
    assert(!memcmp(&copy->motion.lanes[0].chord_state,&instance->motion.lanes[0].chord_state,sizeof(baseline)));
    API.destroy_instance(copy);API.destroy_instance(instance);
    puts("Chord state isolation, activation, release and persistence passed");return 0;
}
