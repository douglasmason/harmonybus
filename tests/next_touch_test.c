#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
static void assignments(void){
    Inst *a=fixture(),*b=API.create_instance("",0);char saved[8192],value[512];
    API.get_param(a,"follow_touch_labels",value,sizeof(value));assert(strstr(value,"5:Next Once|6:Next Latch"));
    API.set_param(a,"motion_hold_6","On");API.get_param(a,"motion_hold_6",value,sizeof(value));assert(!strcmp(value,"On"));
    API.set_param(a,"motion_hold_6","Off");API.get_param(a,"motion_hold_6",value,sizeof(value));assert(!strcmp(value,"Off"));
    API.set_param(a,"motion_lane","6");API.set_param(a,"motion_operation","Velocity");
    API.get_param(a,"follow_touch_labels",value,sizeof(value));assert(strstr(value,"6:Velocity"));
    assert(a->touch_lanes[8]==5&&a->motion.lanes[4].operation==HB_MO_HARMONY);
    assert(a->motion.lanes[4].amount==100&&!a->motion.lanes[4].enabled);
    API.get_param(a,"state",saved,sizeof(saved));
    for(int k=1;k<=9;k++){
        char key[32],number[8];snprintf(key,sizeof(key),"follow_touch_%d",k);snprintf(number,sizeof(number),"%d",17-k);
        API.set_param(a,key,number);API.get_param(b,key,value,sizeof(value));assert(!strcmp(number,value));
    }
    API.set_param(b,"state",saved); /* stale per-track state must not reset global edits */
    for(int k=0;k<9;k++)assert(a->touch_lanes[k]==16-k&&b->touch_lanes[k]==16-k);
    Inst *c=API.create_instance("",0);for(int k=0;k<9;k++)assert(c->touch_lanes[k]==16-k);
    API.get_param(c,"state",saved,sizeof(saved));
    a=fixture();API.set_param(a,"state",saved);for(int k=0;k<9;k++)assert(a->touch_lanes[k]==16-k);
    b=API.create_instance("",0);for(int k=0;k<9;k++)assert(b->touch_lanes[k]==16-k);
}
static void mapping(void){
    Inst *i=fixture();i->chromatic_map=1;i->content_map=0;i->boundary_buffer_ms=0;
    hb_set_shared_follower_scale(1);position=0.25;g_bus.clip_loop_end=4;
    g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,0)};
    g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    for(int look=0;look<3;look++){
        i->next_lookahead=look;
        assert(hb_map_follower_note_now(i,60)==60);
        API.set_param(i,"motion_gesture_5","Touch");
        assert(hb_render_harmony(i).root_pc==2&&hb_map_follower_note_now(i,60)==62);
        assert(hb_map_follower_note_now(i,61)==hb_map_follower_note_now(i,62)-1);
        i->movy_pad_shift[96]=-36;assert(hb_map_follower_note_now(i,96)==61);i->movy_pad_shift[96]=0;
        char view[4096];unsigned current,effective,scale;API.get_param(i,"pad_render",view,sizeof(view));assert(sscanf(view,"%u,%u,%u",&current,&effective,&scale)==3);
        unsigned expected_current=0,expected_next=0;
        unsigned now_mask=hb_harmony_chord_mask(g_bus.next_model[0].harmony),next_mask=hb_harmony_chord_mask(g_bus.next_model[1].harmony);
        for(int n=0;n<12;n++){unsigned pitch=1u<<mod12(hb_map_follower_note_now(i,60+n));if(pitch&now_mask)expected_current|=1u<<n;if(pitch&next_mask)expected_next|=1u<<n;}
        assert(current==expected_current&&effective==expected_next);
        int known;unsigned full;assert(sscanf(strstr(view,"|full1,"),"|full1,%d,%u",&known,&full)==2&&known&&full==expected_next);
        API.set_param(i,"motion_gesture_5","Up,500");assert(hb_render_harmony(i).root_pc==0);
        API.set_param(i,"motion_gesture_5","Touch");API.set_param(i,"motion_gesture_5","Up,50");assert(hb_render_harmony(i).root_pc==2);
        API.set_param(i,"motion_gesture_5","Touch");API.set_param(i,"motion_gesture_5","Up,50");assert(hb_render_harmony(i).root_pc==0);
    }
    API.set_param(i,"motion_gesture_5","Touch");g_bus.next_model_locked=0;assert(hb_render_harmony(i).root_pc==0);
    API.set_param(i,"motion_gesture_5","Cancel");assert(!i->motion.gesture_down);
}
static void expiry(void){
    Inst *i=fixture();i->content_map=0;position=1.9;g_bus.clip_loop_end=4;
    g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,0)};
    g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    for(int held=0;held<2;held++){
        position=1.9;API.set_param(i,"motion_gesture_5","Touch");
        if(!held)API.set_param(i,"motion_gesture_5","Up,20");
        assert(hb_render_harmony(i).root_pc==2&&i->next_touch_mask);
        position=2;g_bus.observed_harmony=g_bus.next_model[1].harmony;hb_effective_write(g_bus.observed_harmony);
        assert(hb_render_harmony(i).root_pc==2); /* not the chord after the arrival */
        char view[2048];unsigned current,effective,scale,expected=0;
        for(int n=0;n<12;n++)if(hb_harmony_chord_mask(g_bus.observed_harmony)&(1u<<mod12(hb_map_follower_note_now(i,60+n))))expected|=1u<<n;
        Inst unchanged=*i;API.get_param(i,"pad_render",view,sizeof(view));
        assert(!memcmp(&unchanged,i,sizeof(unchanged)));
        assert(sscanf(view,"%u,%u,%u",&current,&effective,&scale)==3&&effective==expected);
        hb_next_touch_clear_expired(i);assert(!(i->motion.held&(1u<<4))&&!(i->motion.gesture_latched&(1u<<4)));
        API.set_param(i,"motion_gesture_5","Up,40");assert(!(i->motion.held&(1u<<4)));
        position=2.1;API.set_param(i,"motion_gesture_5","Touch");assert(hb_render_harmony(i).root_pc==0);
        API.set_param(i,"motion_gesture_5","Cancel");
    }
    API.set_param(i,"motion_gesture_6","Touch");API.set_param(i,"motion_gesture_6","Up,30");
    assert(hb_render_harmony(i).root_pc==0);position=4;hb_effective_write(g_bus.next_model[0].harmony);
    hb_next_touch_clear_expired(i);assert(i->motion.gesture_latched&(1u<<5));assert(hb_render_harmony(i).root_pc==2);
    API.set_param(i,"motion_gesture_6","Touch");API.set_param(i,"motion_gesture_6","Up,30");assert(!(i->motion.held&(1u<<5)));
    hb_mo_defaults(&i->motion);i->motion.lanes[14].auto_off=1;i->motion.lanes[15].auto_off=2;
    position=1.9;g_bus.observed_harmony=g_bus.next_model[0].harmony;
    API.set_param(i,"motion_gesture_15","Touch");API.set_param(i,"motion_gesture_16","Touch");
    API.set_param(i,"motion_gesture_15","Up,30");API.set_param(i,"motion_gesture_16","Up,30");
    assert(i->motion.enclosure==1&&i->next_touch_mask==(1u<<14));
    position=2;g_bus.observed_harmony=g_bus.next_model[1].harmony;hb_next_touch_clear_expired(i);
    assert(i->motion.enclosure==4&&i->motion.tap_mask==1); /* only manual Below remains */
    API.set_param(i,"motion_gesture_15","Up,40");assert(i->motion.tap_mask==1);
    API.set_param(i,"motion_lane","6");API.set_param(i,"motion_auto_off","Chord Change");
    char saved[8192];API.get_param(i,"state",saved,sizeof(saved));
    i=fixture();API.set_param(i,"state",saved);assert(i->motion.lanes[5].auto_off==1);
}
static void approach_policies(void){
    Inst *i=fixture();
    for(int mode=0;mode<3;mode++){
        hb_mo_defaults(&i->motion);i->motion.lanes[14].auto_off=mode;i->motion.lanes[15].auto_off=mode;
        API.set_param(i,"motion_gesture_15","Touch");API.set_param(i,"motion_gesture_15","Up,20");
        for(int n=0;n<3;n++){hb_mo_input(&i->motion,60,n,0.01);assert(i->motion.events[16]==(unsigned)(n==0||mode?2:0));}
        API.set_param(i,"performance_reset","1");
        API.set_param(i,"motion_gesture_15","Touch");API.set_param(i,"motion_gesture_16","Touch");
        API.set_param(i,"motion_gesture_16","Up,20");API.set_param(i,"motion_gesture_15","Up,30");
        for(int n=0;n<6;n++){hb_mo_input(&i->motion,60,n,0.01);int step=n%3;int expected=n>=3&&!mode?0:step==0?2:step==1?1:0;assert(i->motion.events[16]==(unsigned)expected);}
        if(mode){hb_mo_end_lanes(&i->motion,(1u<<14)|(1u<<15));assert(!i->motion.enclosure);}
        hb_mo_defaults(&i->motion);i->motion.lanes[12].auto_off=mode;
        API.set_param(i,"motion_gesture_13","Touch");API.set_param(i,"motion_gesture_13","Up,20");
        for(int n=0;n<6;n++){hb_mo_input(&i->motion,60,n,0.01);int step=n%3;int expected=n>=3&&!mode?0:step==0?2:step==1?1:0;assert(i->motion.events[16]==(unsigned)expected);}
    }
    /* A mixed enclosure finishes once, then retains only its persistent side. */
    hb_mo_defaults(&i->motion);i->motion.lanes[14].auto_off=2;
    API.set_param(i,"motion_gesture_15","Touch");API.set_param(i,"motion_gesture_15","Up,20");
    API.set_param(i,"motion_gesture_16","Touch");API.set_param(i,"motion_gesture_16","Up,20");
    for(int n=0;n<5;n++){hb_mo_input(&i->motion,60,n,0.01);assert(i->motion.events[16]==(unsigned)(n==1?1:n==2?0:2));}
}
static void immediate_lookahead(void){
    Inst *i=fixture();char value[8192],view[4096];position=0.25;g_bus.clip_loop_end=8;
    g_bus.next_model_locked=1;g_bus.next_model_count=3;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=1,.harmony=chord(2,1,0)};
    g_bus.next_model[2]=(hb_loop_harmony_event_t){.phase=5,.harmony=chord(7,0,7)};
    g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    API.get_param(i,"next_lookahead",value,sizeof(value));assert(!strcmp(value,"Off"));
    API.set_param(i,"next_lookahead","Immediate");
    API.get_param(i,"next_lookahead",value,sizeof(value));assert(!strcmp(value,"Immediate"));
    assert(hb_render_harmony(i).root_pc==2);
    assert(!i->motion.held); /* selector does not activate an operation lane */
    API.set_param(i,"motion_hold_6","On");assert(hb_render_harmony(i).root_pc==2);
    API.set_param(i,"motion_hold_6","Off");assert(hb_render_harmony(i).root_pc==2);
    for(int guard=0;guard<2;guard++){
        i->next_anti_buffer_ms=guard?1000:0;
        assert(hb_effective_follower_buffer_for(i)==0&&!hb_next_allows_precapture_for(i));
    }
    const double positions[]={1-1e-7,1,4.9,5,7.99,0};
    const int roots[]={2,7,7,0,0,2};
    for(int n=0;n<6;n++){
        position=positions[n];assert(hb_render_harmony(i).root_pc==roots[n]);
        API.get_param(i,"pad_harmony",view,sizeof(view));
        unsigned current,effective,scale;assert(sscanf(view,"%u,%u,%u",&current,&effective,&scale)==3);
        assert(effective==hb_harmony_chord_mask(hb_render_harmony(i)));
    }
    API.get_param(i,"state",value,sizeof(value));
    Inst *other=API.create_instance("",0);API.set_param(other,"state",value);
    assert(other->next_lookahead==HB_LOOKAHEAD_IMMEDIATE);
    API.set_param(other,"next_lookahead","Off");assert(i->next_lookahead==HB_LOOKAHEAD_IMMEDIATE);
    API.set_param(i,"next_lookahead","Off");assert(hb_render_harmony(i).root_pc==0);
    API.set_param(i,"next_lookahead","Immediate");g_bus.next_model_locked=0;
    assert(hb_render_harmony(i).root_pc==0); /* safe fallback during learning */
}
static void after_lookahead(void){
    Inst *i=fixture();char value[8192],view[4096];position=0;g_bus.clip_loop_end=8;
    g_bus.next_model_locked=1;g_bus.next_model_count=3;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=chord(2,1,0)};
    g_bus.next_model[2]=(hb_loop_harmony_event_t){.phase=5,.harmony=chord(7,0,7)};
    g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    API.set_param(i,"next_lookahead","After 1/4");
    assert(i->next_lookahead==29&&hb_next_after_beats_for(i)==1.0);
    const double positions[]={0,1-1e-7,1,3.9999,4,4.9999,5,5.9999,6,7.9999,0};
    const int roots[]={0,0,2,2,2,2,7,7,0,0,0};
    for(int n=0;n<11;n++){
        position=positions[n];assert(hb_render_harmony(i).root_pc==roots[n]);
        API.get_param(i,"pad_harmony",view,sizeof(view));
        unsigned current,effective,scale;assert(sscanf(view,"%u,%u,%u",&current,&effective,&scale)==3);
        assert(effective==hb_harmony_chord_mask(hb_render_harmony(i)));
    }
    position=0.5;assert(hb_next_effective_boundary_for(i,0.5)==1.0);
    position=4;assert(hb_next_effective_boundary_for(i,4)==5.0); // delay coincides with next chord: reset
    API.set_param(i,"next_lookahead","After 1/2");position=4.5;
    assert(hb_render_harmony(i).root_pc==2); // chord shorter than delay: no stale activation
    for(int guard=0;guard<2;guard++){
        i->next_anti_buffer_ms=guard?1000:0;position=2;
        assert(hb_render_harmony(i).root_pc==2&&hb_effective_follower_buffer_for(i)==0&&!hb_next_allows_precapture_for(i));
    }
    // Current/full-next pad targets stay fixed within a chord while the effective
    // renderer moves from current to next. Compare every follower travel mode.
    for(int travel=0;travel<7;travel++){
        i->travel_map=travel;unsigned baseline_current=0,baseline_full=0;
        const char *choices[]={"Off","After 1/4","After 1/4","Immediate","Before 1/4","Late 1/4"};
        for(int choice=0;choice<6;choice++){
            position=choice==2?2:0.5;API.set_param(i,"next_lookahead",choices[choice]);
            API.get_param(i,"pad_render",view,sizeof(view));
            unsigned current,effective,scale,full;int enabled;
            assert(sscanf(view,"%u,%u,%u",&current,&effective,&scale)==3);
            char *section=strstr(view,"|full1,");assert(section&&sscanf(section,"|full1,%d,%u",&enabled,&full)==2);
            if(choice==0){baseline_current=current;baseline_full=full;}
            else assert(current==baseline_current&&full==baseline_full);
        }
    }
    for(int option=0;option<HB_LOOKAHEAD_COUNT;option++){
        API.set_param(i,"next_lookahead",NEXT_LOOKAHEAD_OPTS[option]);
        assert(i->next_lookahead==NEXT_LOOKAHEAD_IDS[option]);
        API.get_param(i,"next_lookahead",value,sizeof(value));assert(!strcmp(value,NEXT_LOOKAHEAD_OPTS[option]));
        API.get_param(i,"state",value,sizeof(value));Inst *restored=API.create_instance("",0);
        API.set_param(restored,"state",value);assert(restored->next_lookahead==i->next_lookahead);API.destroy_instance(restored);
    }
    API.set_param(i,"next_lookahead","1/4");assert(i->next_lookahead==4);
    API.set_param(i,"next_lookahead","-1/4");assert(i->next_lookahead==10);
    API.set_param(i,"next_lookahead","19");assert(i->next_lookahead==19);
    API.set_param(i,"next_lookahead","After 1/4");g_bus.next_model_locked=0;assert(hb_render_harmony(i).root_pc==0);
}
int main(void){after_lookahead();immediate_lookahead();assignments();mapping();expiry();approach_policies();puts("Next Touch: global assignments, first restore, next mapping, chromatic and piano approaches, matching previews and tap/hold pass");}
