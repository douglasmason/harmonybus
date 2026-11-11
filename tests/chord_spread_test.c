/* Production note owners, pressure input and emitted MIDI under live spread. */
#define main ignored_chord_player_main
#include "chord_player_test.c"
#undef main

static void core_spread(void){
    const int expected[5][3]={{60,64,67},{60,64,79},{60,76,79},{60,76,91},{60,88,91}};
    for(int steps=0;steps<5;steps++){
        int notes[]={60,64,67};hb_chord_spread(notes,3,steps);expect_notes(notes,expected[steps],3);
    }
    for(int bass=0;bass<116;bass++)for(int steps=0;steps<=12;steps++){
        int notes[]={bass,bass+4,bass+7};hb_chord_spread(notes,3,steps);
        assert(notes[0]==bass&&notes[1]%12==(bass+4)%12&&notes[2]%12==(bass+7)%12);
        assert(notes[0]<notes[1]&&notes[1]<notes[2]&&notes[2]<=127);
    }
    int ceiling[]={108,112,115};hb_chord_spread(ceiling,3,12);
    const int bounded[]={108,124,127};expect_notes(ceiling,bounded,3);
    int singleton=60;hb_chord_spread(&singleton,1,12);assert(singleton==60);
}
static void choose_spread(Inst *instance){
    API.set_param(instance,"motion_operation_1","Chord Spread");
    assert(instance->motion.lanes[0].amount==4&&!instance->motion.lanes[0].enabled);
    API.set_param(instance,"motion_control_1","2");
}
static hb_cp_key *source_key(Inst *instance){
    for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used&&instance->player.keys[index].source==60)return &instance->player.keys[index];
    fprintf(stderr,"mode=%d role=%d used=%d pending=%d harmony=%d\n",hb_cp_mode(&instance->player),instance->role,instance->player.keys[0].used,instance->follower_queue_count,hb_render_harmony(instance).valid);assert(0);return 0;
}
static void expect_pool(Inst *instance,const int *base,int steps){
    int expected[HB_CP_VOICES];hb_cp_key *key=source_key(instance);
    memcpy(expected,base,(size_t)key->count*sizeof(int));hb_chord_spread(expected,key->count,steps);
    for(int voice=0;voice<key->count;voice++)if(key->notes[voice]!=expected[voice])fprintf(stderr,"pool steps=%d voice=%d actual=%d expected=%d mode=%d harmony=%d\n",steps,voice,key->notes[voice],expected[voice],hb_cp_mode(&instance->player),hb_render_harmony(instance).valid);
    expect_notes(key->notes,expected,key->count);
}
static void drain_spread(Inst *instance){for(int tick=0;tick<32;tick++)advance(instance,0,1);}
static void held_spread(int selection,int arp){
    Inst *instance=fixture();
    API.set_param(instance,"chord_mode",selection?"Off":"Conductor Chord");
    if(selection){API.set_param(instance,"role","Conductor");API.set_param(instance,"render_channel","3");}
    API.set_param(instance,"chord_form","Triad");API.set_param(instance,"chord_inversion","Root");
    API.set_param(instance,"arp_playback",arp?"Repeat Arp":"Together");
    API.set_param(instance,"arp_order","Down"); /* Spread must retune the currently sounding top. */
    API.set_param(instance,"chord_register","0");choose_spread(instance);
    if(selection)API.set_param(instance,"hb_movy_input_chord","0,1,2,1,0,1,12");
    midi(instance,1,60);advance(instance,0,64);
    hb_cp_key *key=source_key(instance);assert(key->count==3);
    int base[HB_CP_VOICES];memcpy(base,key->notes,sizeof(base));
    int first=render_count,actions=instance->action_count,recorded_before=recorded_count;
    double boundary=instance->player.next_beat,gate=instance->player.gate_beat;
    int step=instance->player.order_state.step;
    for(int trial=0;trial<3;trial++){
        API.set_param(instance,"motion_hold_1","On");drain_spread(instance);
        assert(instance->chord_spread_steps==2);expect_pool(instance,base,2);
        assert(instance->player.sounding[0][base[2]+12]);assert(!instance->player.sounding[0][base[2]]);
        if(!arp)assert(instance->player.sounding[0][base[0]]);
        assert(instance->player.sounding_count==(arp?1:3));
        API.set_param(instance,"motion_hold_1","Off");drain_spread(instance);expect_pool(instance,base,0);
        assert(instance->player.sounding[0][base[2]]&&!instance->player.sounding[0][base[2]+12]);
    }
    assert(render_count>first&&instance->action_count==actions&&recorded_count==recorded_before);
    if(arp){assert(instance->player.next_beat==boundary&&instance->player.gate_beat==gate);assert(instance->player.order_state.step==step);}
    API.set_param(instance,"motion_hold_1","On");drain_spread(instance);
    midi(instance,0,60);drain_spread(instance);assert(!instance->player.sounding_count);
    API.destroy_instance(instance);
}
static void squeeze(Inst *instance,int pressure){uint8_t packet[3]={0xA0,96,(uint8_t)pressure};API.process_midi(instance,packet,3,output,lengths,64);}
static void press_row(Inst *instance){API.set_param(instance,"hb_movy_input_approach","96,-36,3");midi(instance,1,96);}
static void composition_and_bounds(void){
    Inst *instance=fixture();API.set_param(instance,"chord_mode","Conductor Chord");
    API.set_param(instance,"chord_form","Triad");API.set_param(instance,"chord_inversion","Root");
    API.set_param(instance,"arp_playback","Together");API.set_param(instance,"chord_register","0");
    choose_spread(instance);midi(instance,1,60);advance(instance,0,64);
    API.set_param(instance,"motion_hold_1","On");drain_spread(instance);
    const int base[]={60,64,67};expect_pool(instance,base,2);
    API.set_param(instance,"chord_register","1");drain_spread(instance);
    const int higher[]={72,76,79};expect_pool(instance,higher,2);
    API.set_param(instance,"chord_register","0");drain_spread(instance);expect_pool(instance,base,2);
    API.set_param(instance,"motion_operation_2","Global Transpose");API.set_param(instance,"motion_control_2","5");
    API.set_param(instance,"motion_hold_2","On");drain_spread(instance);expect_pool(instance,base,2);
    assert(instance->motion_local.transpose.heard[0][65]&&instance->motion_local.transpose.heard[0][81]&&instance->motion_local.transpose.heard[0][84]);
    API.set_param(instance,"motion_hold_2","Off");midi(instance,0,60);drain_spread(instance);
    API.set_param(instance,"motion_operation_2","Chord/Arp State");API.set_param(instance,"motion_lane","2");
    API.set_param(instance,"motion_amount","Current Harmony Burst");API.set_param(instance,"chord_edit_target","Lane 2");
    API.set_param(instance,"chord_input","Root/Bass + Top");API.set_param(instance,"arp_playback","Together");
    API.set_param(instance,"motion_hold_2","On");midi(instance,1,60);midi(instance,1,76);drain_spread(instance);
    hb_cp_key *key=source_key(instance);assert(key->notes[0]==60&&key->notes[key->count-1]==76);
    API.set_param(instance,"motion_hold_1","Off");drain_spread(instance);
    assert(key->notes[0]==60&&key->notes[key->count-1]==76);
    midi(instance,0,60);midi(instance,0,76);drain_spread(instance);assert(!instance->player.sounding_count);
    API.destroy_instance(instance);
}
static void pressure_lifecycle(void){
    Inst *instance=fixture();instance->approach_layout=1;instance->preview_count=32;
    API.set_param(instance,"approach_pressure_row","3");API.set_param(instance,"approach_pressure_lane","1");
    API.set_param(instance,"approach_pressure_mode","1");choose_spread(instance);
    API.set_param(instance,"motion_control_1","4");
    API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Triad");
    API.set_param(instance,"chord_inversion","Root");API.set_param(instance,"arp_playback","Together");
    midi(instance,1,60);advance(instance,0,64);int base[HB_CP_VOICES];memcpy(base,source_key(instance)->notes,sizeof(base));
    Inst *peer=API.create_instance("",0);API.set_param(peer,"role","Follower");
    press_row(instance);squeeze(instance,127);drain_spread(instance);expect_pool(instance,base,4);
    assert(peer->chord_spread_steps==0&&g_global_operation_transpose==0);
    squeeze(instance,64);drain_spread(instance);expect_pool(instance,base,2);
    squeeze(instance,80);assert(instance->chord_spread_steps==2); /* Hysteresis at 2.52. */
    squeeze(instance,86);assert(instance->chord_spread_steps==3);
    midi(instance,0,96);drain_spread(instance);expect_pool(instance,base,0);
    API.set_param(instance,"approach_pressure_mode","3");API.set_param(instance,"approach_pressure_reverse","1");
    press_row(instance);squeeze(instance,80);midi(instance,0,96);drain_spread(instance);
    assert(instance->chord_spread_steps==1);expect_pool(instance,base,1);
    press_row(instance);midi(instance,0,96);drain_spread(instance);expect_pool(instance,base,0);
    API.set_param(instance,"approach_pressure_reverse","0");
    press_row(instance);squeeze(instance,127);squeeze(instance,20);midi(instance,0,96);drain_spread(instance);expect_pool(instance,base,4);
    press_row(instance);squeeze(instance,64);midi(instance,0,96);drain_spread(instance);expect_pool(instance,base,2);
    unsigned long long actions[HB_MOTION_LANES+1];hb_mo_capture(&instance->motion,0,0,60,actions);assert(!(actions[0]&HB_MO_RECORDED));
    char metadata[196608];API.set_param(instance,"motion_lane","1");API.get_param(instance,"chain_params",metadata,sizeof(metadata));
    assert(strstr(metadata,"Chord Spread")&&strstr(metadata,"Max Spread Steps"));
    char saved[32768];API.get_param(instance,"state",saved,sizeof(saved));API.set_param(instance,"state",saved);drain_spread(instance);
    assert(!instance->chord_spread_steps&&!instance->motion.pressure_active&&instance->motion.lanes[0].amount==4);
    API.set_param(instance,"motion_gesture_1","LatchOn");assert(instance->chord_spread_steps==4);
    API.set_param(instance,"motion_gesture_1","LatchOff");assert(!instance->chord_spread_steps);
    API.set_param(instance,"motion_control_1","999");assert(instance->motion.lanes[0].amount==12);
    API.set_param(instance,"motion_amount","-99");assert(instance->motion.lanes[0].amount==0);
    API.set_param(instance,"motion_control_1","4");API.set_param(instance,"motion_hold_1","On");
    API.set_param(instance,"motion_operation_1","Off");assert(!instance->chord_spread_steps);
    choose_spread(instance);press_row(instance);squeeze(instance,127);midi(instance,0,96);
    assert(instance->chord_spread_steps);position=-1;transport=0;advance(instance,0,64);assert(!instance->chord_spread_steps&&!instance->player.sounding_count);
    position=0;transport=2;API.set_param(instance,"motion_hold_1","On");API.set_param(instance,"role","Off");assert(!instance->chord_spread_steps);
    API.destroy_instance(peer);API.destroy_instance(instance);
}
int main(void){
    core_spread();for(int selection=0;selection<2;selection++)for(int arp=0;arp<2;arp++)held_spread(selection,arp);
    composition_and_bounds();pressure_lifecycle();
    puts("Chord Spread: anchored bass, MIDI ceiling, generated/selected held chords, arp clock, one-message drain, pressure/latch/reverse, track scope and persistence pass");
}
