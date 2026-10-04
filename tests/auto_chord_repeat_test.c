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
static void selectable_modes(void){
    Inst *instance=fixture();configure(instance);
    const char *modes[]={"Chord Only / Release","Arp Only / Release","Both / Release","Chord Only / Press","Arp Only / Press","Both / Press"};
    for(int mode=0;mode<6;mode++){
        API.set_param(instance,"motion_control_32",modes[mode]);
        API.set_param(instance,"motion_gesture_32","LatchOn");
        assert(hb_cp_mode(&instance->player)==(mode%3==1?0:2));
        assert(hb_cp_playback(&instance->player)==(mode%3==0?0:1));
        midi(instance,1,60);advance(instance,1,64);
        assert(hb_cp_held(&instance->player)==1);
        assert(instance->player.keys[0].count==(mode%3==1?1:4));
        midi(instance,0,60);advance(instance,125,64);
        assert(instance->player.repeat_override); /* Release never consumes permanent latch. */
        API.set_param(instance,"motion_gesture_32","LatchOff");advance(instance,125,64);
        assert(!instance->player.repeat_override&&!instance->player.sounding_count);
        char state[16384],label[32];API.get_param(instance,"state",state,sizeof(state));
        API.destroy_instance(instance);instance=API.create_instance("",0);API.set_param(instance,"state",state);
        API.get_param(instance,"motion_control_32",label,sizeof(label));assert(!strcmp(label,modes[mode]));
    }
    /* Switching while held flushes old voices and preserves the latch. */
    API.set_param(instance,"motion_gesture_32","LatchOn");midi(instance,1,60);advance(instance,125,64);
    API.set_param(instance,"motion_control_32","Arp Only");advance(instance,125,64);
    assert(instance->motion.gesture_persistent&(1ULL<<31));
    assert(!hb_cp_held(&instance->player)&&!instance->player.sounding_count);
    midi(instance,0,60);API.destroy_instance(instance);
    /* A numeric amount saved before modes existed must still mean Both. */
    hb_motion_config old,restored;hb_mo_defaults(&old);hb_mo_defaults(&restored);
    old.lanes[31].amount=2;char saved[16384]={0};hb_mo_save(&old,saved,sizeof(saved),0);
    char *marker=strstr(saved,";ca1");assert(marker);memmove(marker,marker+4,strlen(marker+4)+1);
    hb_mo_restore(&restored,saved);assert(restored.lanes[31].amount==1);
}
static void six_resolution_modes(void){
    const char *choices[]={"Chord Only / Release","Arp Only / Release","Both / Release","Chord Only / Press","Arp Only / Press","Both / Press"};
    for(int choice=0;choice<6;choice++){
        Inst *instance=fixture();
        API.set_param(instance,"motion_control_32",choices[choice]);
        arm_target_once(instance);
        instance->movy_playback=1;midi(instance,1,62);midi(instance,0,62);instance->movy_playback=0;
        assert(instance->player.repeat_override);
        midi(instance,1,60);advance(instance,1,64);
        assert((instance->player.repeat_override!=0)==(choice<3));
        if(choice>=3)assert(instance->follower_held[60]);
        midi(instance,0,60);advance(instance,125,64);
        assert(!instance->player.repeat_override);
        assert(!(instance->motion.gesture_once&(1ULL<<31)));
        API.destroy_instance(instance);
    }
}
static void activate_existing_hold(void){
    for(int retrigger=0;retrigger<2;retrigger++){
        Inst *instance=fixture();instance->retrigger_held=retrigger;
        midi(instance,1,60);advance(instance,1,64);unsigned captured=instance->action_count;
        API.set_param(instance,"motion_gesture_32","Knob,1000");
        assert(instance->player.repeat_override&&hb_cp_held(&instance->player)==1);
        assert(instance->motion.gesture_target_owner[31]==61);
        assert(instance->action_count==captured);
        API.set_param(instance,"motion_gesture_32","Up,40,1040");
        assert(instance->motion.gesture_target_owner[31]==61);
        advance(instance,1,64);midi(instance,0,60);
        assert(!instance->player.repeat_override);
        advance(instance,1,64);assert(!instance->player.sounding_count);
        API.destroy_instance(instance);
    }
}
static void rapid_same_pad(void){
    for(int arp=0;arp<2;arp++){
        Inst *instance=fixture();API.set_param(instance,"chord_mode","Scale Degree");
        API.set_param(instance,"arp_playback",arp?"Repeat Arp":"Off");
        instance->player.config.latch=0;instance->player.config.phase=2;
        midi(instance,1,60);advance(instance,1,64);
        for(int repeat=0;repeat<12;repeat++){
            midi(instance,0,60);midi(instance,1,60);int count=advance(instance,1,64),attacks=0;
            for(int event=0;event<count;event++)attacks+=(output[event][0]&0xf0)==0x90&&output[event][2];
            assert(attacks>0);
        }
        midi(instance,0,60);advance(instance,1,64);assert(!instance->player.sounding_count);
        API.destroy_instance(instance);
    }
}
/* Separate spatial approach rows must keep emitting after the target consumes
   Chord + Arp / Release. Check injected render notes, not only player flags. */
static void spatial_release_cycles(void){
    const int gaps[]={0,1,250,1000};
    for(int layout=1;layout<=2;layout++)for(int stock=0;stock<3;stock++)for(int gap=0;gap<4;gap++){
        Inst *instance=fixture();instance->approach_layout=layout;instance->preview_count=32;
        instance->player.config.phase=2;instance->player.config.start=5;
        instance->next_anti_buffer_ms=25;instance->retrigger_held=1;
        API.set_param(instance,"approach_bank_1",stock?"Stock: ii-V-Target":"Secondary V (Dom)");
        if(stock==2){
            API.set_param(instance,"approach_bank_1","Secondary V (Dom)");
            API.set_param(instance,"approach_bank_2","Secondary II");
            API.set_param(instance,"approach_touch_2","Down");
            API.set_param(instance,"approach_touch_1","Down");
            API.set_param(instance,"approach_touch_2","Up,50");
        }else API.set_param(instance,"approach_touch_1","Down");
        API.set_param(instance,"approach_touch_1","Up,50");
        int source=layout==1?96:92;
        const char *alias=layout==1?"96,-36,3":"92,-32,0";
        arm_target_once(instance);
        for(int cycle=0;cycle<3;cycle++){
            for(int repeat=0;repeat<4;repeat++){
                render_count=0;
                API.set_param(instance,"hb_movy_input_approach",alias);midi(instance,1,source);
                for(int frame=0;frame<4;frame++)advance(instance,1,64);
                int attacks=0;for(int event=0;event<render_count;event++)
                    attacks+=(rendered[event][1]&0xf0)==0x90&&rendered[event][3]>0;
                assert(attacks>0);
                midi(instance,0,source);
                if(gaps[gap])advance(instance,gaps[gap],64);
            }
            midi(instance,1,60);advance(instance,10,64);midi(instance,0,60);
            assert(!instance->player.repeat_override);
            if(gaps[gap])advance(instance,gaps[gap],64);
        }
        advance(instance,10,64);
        assert(!instance->motif.pending&&!instance->player.sounding_count);
        API.destroy_instance(instance);
    }
}
static void target_after_approach_attack(void){
    for(int phase=0;phase<3;phase++)for(int latch=0;latch<2;latch++){
        Inst *instance=fixture();instance->approach_layout=1;instance->preview_count=32;
        instance->player.config.phase=phase;instance->player.config.start=5;
        instance->player.config.latch=latch;
        API.set_param(instance,"approach_touch_2","Down");API.set_param(instance,"approach_touch_1","Down");
        API.set_param(instance,"approach_touch_2","Up,50");API.set_param(instance,"approach_touch_1","Up,50");
        arm_target_once(instance);
        API.set_param(instance,"hb_movy_input_approach","96,-36,3");midi(instance,1,96);advance(instance,1,64);
        if(latch)midi(instance,0,96); /* A retained arp pool has the same timing risk as overlap. */
        render_count=0;midi(instance,1,60);advance(instance,1,64);
        int attacks=0;for(int event=0;event<render_count;event++)attacks+=(rendered[event][1]&0xf0)==0x90&&rendered[event][3]>0;
        assert((attacks>0)==(phase!=1)); /* Auto stays quantized; other modes attack before release. */
        midi(instance,0,60);midi(instance,0,96);advance(instance,1,64);
        assert(!instance->player.repeat_override&&!instance->player.sounding_count);
        API.destroy_instance(instance);
    }
}
static void reset_track_and_advance(void){
    Inst *instance=fixture(),*other=API.create_instance("",0);
    API.set_param(other,"role","Follower");API.set_param(other,"render_channel","8");
    API.set_param(instance,"approach_mode_active","1");
    API.set_param(instance,"approach_bank_1","Stock: ii-V-Target");
    API.set_param(instance,"approach_control_1","LatchOn");
    midi(instance,1,60);advance(instance,1,64);
    unsigned captured=instance->action_count;
    unsigned event=instance->approach_rows.event;
    API.set_param(instance,"harm_play_advance","Next");
    assert(instance->advance_pending==1);
    for(int frame=0;frame<3;frame++)advance(instance,1,64);
    assert(instance->advance_pending==0&&instance->physical_velocity[0][60]);
    assert(instance->action_count==captured);
    assert(instance->approach_rows.event!=event);
    midi(instance,0,60);API.set_param(instance,"harm_play_advance","Next");assert(!instance->advance_pending);
    API.set_param(instance,"approach_control_1","LatchOn");
    midi(instance,1,60);advance(instance,1,64);event=instance->approach_rows.event;
    API.set_param(instance,"motion_gesture_32","Knob,1000");
    assert(instance->approach_rows.performance&&instance->approach_rows.event==event);
    API.set_param(instance,"motion_gesture_32","Up,40,1040");
    midi(instance,0,60);advance(instance,1,64);assert(!instance->player.repeat_override&&!instance->player.sounding_count);
    int shared_scale=hb_shared_follower_scale();
    for(int track=0;track<16;track++){
        instance->movy_track=track;API.set_param(instance,"render_channel","12");
        API.set_param(instance,"dominant_color","LatchOn");
        char number[16];snprintf(number,sizeof(number),"%d",track);
        API.set_param(instance,"track_defaults_reset",number);
        assert(instance->movy_track==track);
        assert(instance->role==(track>=12?3:track%4==0?0:1));
        assert(instance->render_channel==(track%4==0?2:track%4));assert(instance->source_channel==-1);
        assert(!instance->dominant_color_latched&&!instance->policy_overrides);
        assert(instance->player.config.mode==(instance->role==0?1:0));
        assert(other->render_channel==7&&other->role==1&&hb_shared_follower_scale()==shared_scale);
        for(int lane=0;lane<16;lane++)assert(instance->motion.lanes[lane].operation==HB_MO_OFF);
    }
    API.set_param(instance,"track_defaults_reset","2");assert(instance->movy_track==15&&instance->role==3);
    API.destroy_instance(other);API.destroy_instance(instance);
}
static void release_operation(void){
    for(int mode=0;mode<5;mode++){
        Inst *instance=fixture();API.set_param(instance,"harm_play_release","500 ms");
        arm_target_once(instance);
        if(mode){
            API.set_param(instance,"harm_play_release_control","Down");
            if(mode==1||mode==4)API.set_param(instance,"harm_play_release_control","Up,40");
            if(mode==3){API.set_param(instance,"harm_play_release_control","LatchOn");API.set_param(instance,"harm_play_release_control","Up,40");}
            if(mode==4){API.set_param(instance,"harm_play_release_control","Down");API.set_param(instance,"harm_play_release_control","Up,40");}
        }
        midi(instance,1,60);advance(instance,1,64);
        assert(!instance->player.release_used);
        midi(instance,0,61);assert(!instance->player.release_used); /* Unrelated OFF cannot spend it. */
        midi(instance,0,60);advance(instance,1,64);
        int tails=0;for(int k=0;k<HB_CP_KEYS;k++)tails+=instance->player.keys[k].used&&instance->player.keys[k].release_end>0;
        assert(!!tails==(mode>0&&mode<4));assert(!instance->player.release_armed);
        if(mode==2){API.set_param(instance,"harm_play_release_control","Up,40");assert(!instance->player.release_held&&!instance->player.release_armed);}
        assert(instance->player.release_latched==(mode==3));
        API.destroy_instance(instance);
    }
    Inst *instance=fixture();
    API.set_param(instance,"harm_play_release_control","Down");API.set_param(instance,"harm_play_release_control","Up,1000");assert(!instance->player.release_armed);
    API.set_param(instance,"harm_play_release_control","Down");API.set_param(instance,"harm_play_release_control","Cancel");assert(!instance->player.release_held&&!instance->player.release_armed);
    API.destroy_instance(instance);
}
static void release_envelope(void){
    Inst *synced=fixture();API.set_param(synced,"harm_play_release","1/4");API.set_param(synced,"harm_play_release_control","LatchOn");
    arm_target_once(synced);midi(synced,1,60);advance(synced,1,64);midi(synced,0,60);advance(synced,1,64);
    advance(synced,100,64);tempo=60;advance(synced,600,64);assert(synced->player.repeat_override);
    advance(synced,210,64);advance(synced,1,64);assert(!synced->player.repeat_override&&!synced->player.sounding_count);
    char sync_state[16384],sync_label[32];API.get_param(synced,"state",sync_state,sizeof(sync_state));
    API.set_param(synced,"harm_play_release","0 ms");API.set_param(synced,"state",sync_state);
    API.get_param(synced,"harm_play_release",sync_label,sizeof(sync_label));assert(!strcmp(sync_label,"1/4"));
    API.destroy_instance(synced);

    for(int follow=0;follow<2;follow++)for(int chord_only=0;chord_only<2;chord_only++){
        Inst *instance=fixture();API.set_param(instance,"harm_play_release","500 ms");API.set_param(instance,"harm_play_release_control","LatchOn");
        API.set_param(instance,"harm_play_release_harmony",follow?"Follow Harmony":"Freeze at Release");
        API.set_param(instance,"motion_control_32",chord_only?"Chord Only / Release":"Both / Release");
        arm_target_once(instance);midi(instance,1,60);advance(instance,1,64);
        instance->retrigger_held=1;uint8_t changed[]={64,68,71,74};hb_commit_observed_harmony(hb_infer_harmony(changed,4));
        advance(instance,1,64);
        for(int key=0;key<HB_CP_KEYS;key++)if(instance->player.keys[key].used){
            assert(instance->player.keys[key].harmony_root==4);
            assert(instance->player.keys[key].held&&!instance->player.keys[key].release_end);
        }
        midi(instance,0,60);
        assert(instance->player.repeat_override); /* Final release owns its audible tail. */
        int soft=0;double release_deadline=0;
        for(int step=0;step<51;step++){
            if(step==5){
                for(int key=0;key<HB_CP_KEYS;key++)if(instance->player.keys[key].used)release_deadline=instance->player.keys[key].release_end;
                instance->retrigger_held=1;
                uint8_t notes[]={62,65,69,72};hb_commit_observed_harmony(hb_infer_harmony(notes,4));
            }
            int count=advance(instance,10,64);
            if(step==5)for(int key=0;key<HB_CP_KEYS;key++)if(instance->player.keys[key].used)
                {assert(instance->player.keys[key].release_end==release_deadline);assert(instance->player.keys[key].harmony_root==(follow?2:4));} /* Released harmony and deadline stay frozen. */
            for(int event=0;event<count;event++)if((output[event][0]&0xf0)==0x90&&output[event][2]>0&&output[event][2]<100)soft++;
        }
        advance(instance,1,64);
        assert(!instance->player.repeat_override&&!instance->player.sounding_count);
        if(!chord_only)assert(soft>0);
        char state[16384],value[32];API.get_param(instance,"state",state,sizeof(state));API.destroy_instance(instance);
        instance=API.create_instance("",0);API.set_param(instance,"state",state);
        API.get_param(instance,"harm_play_release",value,sizeof(value));assert(!strcmp(value,"500 ms"));
        assert(instance->player.release_follow_harmony==follow);
        API.destroy_instance(instance);
    }
    /* A spatial re-strike interrupts its own tail, keeps the alias, and attacks
       at the new velocity instead of inheriting the fade. */
    Inst *instance=fixture();instance->approach_layout=1;instance->preview_count=32;
    API.set_param(instance,"harm_play_release","500 ms");API.set_param(instance,"harm_play_release_control","LatchOn");arm_target_once(instance);
    for(int repeat=0;repeat<4;repeat++){
        API.set_param(instance,"hb_movy_input_approach","96,-36,3");midi(instance,1,96);
        int count=advance(instance,1,64),full=0;
        for(int event=0;event<count;event++)full+=(output[event][0]&0xf0)==0x90&&output[event][2]==100;
        assert(full);midi(instance,0,96);advance(instance,20,64);
    }
    midi(instance,1,60);advance(instance,1,64);midi(instance,0,60);advance(instance,20,64);
    API.set_param(instance,"hb_movy_input_approach","96,-36,3");midi(instance,1,96);advance(instance,1,64);
    assert(instance->movy_pad_shift[96]==-36&&!instance->player.repeat_override);
    midi(instance,0,96);uint8_t stop=0xfc;API.process_midi(instance,&stop,1,output,lengths,64);advance(instance,1,64);
    assert(!instance->player.sounding_count);API.destroy_instance(instance);
}
static void arp_relative_release(void){
    const char *labels[]={"Arp Note 1/2","Arp Note 1","Arp Note 2","Arp Note 3","Arp Note 4","Arp Note 8","Arp Cycle 1/4","Arp Cycle 1/2","Arp Cycle 1","Arp Cycle 2","Arp Cycle 4"};
    const double factors[]={0.5,1,2,3,4,8,0.25,0.5,1,2,4};
    for(int option=0;option<11;option++){
        Inst *instance=fixture();char label[32],state[16384];
        API.set_param(instance,"harm_play_release",labels[option]);
        API.get_param(instance,"state",state,sizeof(state));
        API.set_param(instance,"harm_play_release","0 ms");API.set_param(instance,"state",state);
        API.get_param(instance,"harm_play_release",label,sizeof(label));assert(!strcmp(label,labels[option]));
        assert(instance->player.release_ms==-10-option);API.destroy_instance(instance);
        for(int order=0;order<=2;order+=2)for(int range=1;range<=2;range++)for(int cycle_rate=0;cycle_rate<=1;cycle_rate++)for(int gate=0;gate<3;gate++){
            hb_chord_player player={0};hb_cp_defaults(&player.config);
            player.config.rate=cycle_rate?11:2;player.config.order=order;player.config.gate=gate;
            player.repeat_override=1;player.release_latched=1;player.release_ms=-10-option;player.release_beats=3;
            const int notes[]={60,64,67};assert(hb_cp_on(&player,60,0,100,notes,3));player.keys[0].range=range;
            hb_cp_off(&player,60,0);
            double steps=order==2?2*(3*range)-2:3*range;
            double expected=0.25*factors[option]*(option>=6?steps:1)/(cycle_rate?steps:1);
            hb_cp_key *key=&player.keys[0];assert(key->release_synced);
            assert(fabs(key->release_end-key->release_start-expected)<1e-9);
            double deadline=key->release_end;
            player.config.rate=8;player.config.order=0;key->range=4;
            hb_cp_off(&player,60,0);assert(key->release_end==deadline); /* Settings edits and duplicate OFF leave duration captured. */
            player.release_beats=deadline-0.0001;hb_cp_tick(&player,output,lengths,64);assert(key->used);
            player.release_beats=deadline;hb_cp_tick(&player,output,lengths,64);assert(!key->used);
        }
    }
    /* Existing saved values omitted from the shorter menu still round-trip. */
    Inst *instance=fixture();char state[16384],label[32];API.set_param(instance,"harm_play_release","300 ms");
    API.get_param(instance,"state",state,sizeof(state));API.set_param(instance,"harm_play_release","0 ms");API.set_param(instance,"state",state);
    API.get_param(instance,"harm_play_release",label,sizeof(label));assert(!strcmp(label,"300 ms"));API.destroy_instance(instance);
}
int main(void){arp_relative_release();release_operation();release_envelope();target_after_approach_attack();spatial_release_cycles();reset_track_and_advance();activate_existing_hold();rapid_same_pad();six_resolution_modes();selectable_modes();target_release_once();automatic_gate();release_and_restore();latch_settings_and_overlap();puts("Auto Chord Repeat: repeated MIDI, hold/release, latch, overlap, base settings, persistence and no stuck notes pass");}
