/* Reuse the production host fixture; exercise real MIDI processing and state. */
#define main chord_regression_main
#include "chord_player_test.c"
#undef main

static void expect_param(Inst *instance,const char *key,const char *expected){
    char actual[1024];assert(API.get_param(instance,key,actual,sizeof(actual))>=0);
    if(strcmp(actual,expected)){fprintf(stderr,"%s: %s != %s\n",key,actual,expected);assert(0);}
}
static int send_note(Inst *instance,int on,int pitch){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)};
    int count=API.process_midi(instance,message,3,output,lengths,64);
    return count+API.tick(instance,128,48000,output+count,lengths+count,64-count);
}
static double motion_test_beat(void){return transport==MOVE_CLOCK_STATUS_STOPPED?-1.0:position;}
static Inst *motion_fixture(void){
    host.get_beat_position=motion_test_beat;
    Inst *instance=fixture();API.set_param(instance,"boundary_buffer_ms","0 ms");
    return instance;
}
static void controls_and_state(void){
    Inst *instance=motion_fixture();char legacy[1024],saved[1024],after[1024];
    API.get_param(instance,"state",legacy,sizeof(legacy));
    for(int lane=1;lane<=4;lane++){
        char number[16];snprintf(number,sizeof(number),"%d",lane);
        API.set_param(instance,"motion_lane",number);
        API.set_param(instance,"motion_operation",lane==1?"Octave":"Velocity");
        API.set_param(instance,"motion_amount",number);
        API.set_param(instance,"motion_pattern","Alternate");
        API.set_param(instance,"motion_probability","37");
        API.set_param(instance,"motion_grid","1/16");
        API.set_param(instance,"motion_cycle","3 Bars");
        API.set_param(instance,"motion_evolve","Evolve");
    }
    API.set_param(instance,"motion_bypass","On");
    API.get_param(instance,"state",saved,sizeof(saved));
    API.set_param(instance,"motion_hold_2","On");
    API.get_param(instance,"state",after,sizeof(after));
    assert(!strcmp(saved,after)); /* runtime holds are excluded */
    API.set_param(instance,"state",saved);assert(instance->motion.held==0);
    for(int lane=1;lane<=4;lane++){
        char number[16];snprintf(number,sizeof(number),"%d",lane);
        API.set_param(instance,"motion_lane",number);expect_param(instance,"motion_amount",number);
        expect_param(instance,"motion_grid","1/16");expect_param(instance,"motion_cycle","3 Bars");
        expect_param(instance,"motion_probability","37");expect_param(instance,"motion_evolve","Evolve");
    }
    Inst *other=API.create_instance("",NULL);expect_param(other,"motion_operation","Off");
    API.set_param(instance,"state",legacy);assert(!hb_mo_enabled(&instance->motion));
    expect_param(instance,"motion_lane","1");expect_param(instance,"motion_bypass","Off");
    API.set_param(instance,"state",saved);API.get_param(instance,"state",after,sizeof(after));assert(!strcmp(saved,after));
    char small[16];assert(API.get_param(instance,"state",small,sizeof(small))>=16);assert(small[15]==0);
    hb_mo_restore(&instance->motion,";mo1,0,99,0,0,0,1,3,3,0,100,0,0");
    assert(!hb_mo_enabled(&instance->motion));
}
static void punch_and_release(void){
    Inst *instance=motion_fixture();
    API.set_param(instance,"motion_operation","Octave");
    API.set_param(instance,"motion_enabled","Off");API.set_param(instance,"motion_probability","0");
    API.set_param(instance,"motion_bypass","On");
    assert(send_note(instance,1,60)==1&&output[0][1]==60);send_note(instance,0,60);
    API.set_param(instance,"motion_hold_1","On");
    assert(send_note(instance,1,60)==1&&output[0][1]==72);
    assert(rendered[render_count-1][2]==72);
    API.set_param(instance,"motion_lane","2");API.set_param(instance,"motion_hold_1","Off");
    assert(send_note(instance,0,60)==1&&output[0][1]==72); /* release uses original pitch */
    assert(rendered[render_count-1][2]==72&&rendered[render_count-1][1]==0x83);
    assert(!instance->motion_local.owned&&!instance->motion_render.owned);
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_enabled","On");
    API.set_param(instance,"motion_bypass","Off");API.set_param(instance,"motion_probability","100");
    send_note(instance,1,60);API.set_param(instance,"motion_operation","Off");
    assert(send_note(instance,0,60)==1&&output[0][1]==72);
}
static void lanes_and_patterns(void){
    Inst *instance=motion_fixture();
    API.set_param(instance,"motion_operation","Octave");
    API.set_param(instance,"motion_lane","2");API.set_param(instance,"motion_operation","Octave");
    assert(send_note(instance,1,60)==1&&output[0][1]==84);send_note(instance,0,60);
    API.set_param(instance,"motion_operation","Velocity");API.set_param(instance,"motion_amount","-50");
    assert(send_note(instance,1,60)==1&&output[0][1]==72&&output[0][2]==50);send_note(instance,0,60);
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Rotate");
    assert(send_note(instance,1,60)==1&&output[0][1]==64);send_note(instance,0,60);
    API.set_param(instance,"motion_operation","Skip");assert(send_note(instance,1,60)==0);assert(send_note(instance,0,60)==0);
    hb_motion_config config;hb_mo_defaults(&config);config.lanes[0].operation=HB_MO_OCTAVE;config.lanes[0].amount=1;config.lanes[0].pattern=1;
    double first,second;
    assert(hb_mo_value(&config,0,0,60,&first)&&first==-1);
    assert(hb_mo_value(&config,0,0.5,60,&second)&&second==1);
    config.lanes[0].pattern=6;config.lanes[0].probability=47;
    for(int step=0;step<100;step++){
        int accepted=hb_mo_value(&config,0,step*0.5,60,&first);
        assert(accepted==hb_mo_value(&config,0,step*0.5+4,72,&second));
        if(accepted)assert(first==second); /* repeated cycle; chord grouping */
    }
}
static void gate_pan_and_stop(void){
    Inst *instance=motion_fixture();API.set_param(instance,"motion_operation","Gate");
    API.set_param(instance,"motion_amount","50");
    assert(send_note(instance,1,60)==1);position=0.26;
    int count=API.tick(instance,6240,48000,output,lengths,64);
    assert(count==1&&output[0][0]==0x80&&output[0][1]==60);
    assert(send_note(instance,0,60)==0);
    /* A live note must also close its gate when transport is stopped. */
    instance=motion_fixture();transport=1;position=-1;API.set_param(instance,"motion_operation","Gate");
    send_note(instance,1,60);count=API.tick(instance,7000,48000,output,lengths,64);
    assert(count==1&&output[0][0]==0x80);send_note(instance,0,60);
    instance=motion_fixture();API.set_param(instance,"motion_operation","Pan");
    API.set_param(instance,"motion_enabled","Off");API.set_param(instance,"motion_hold_1","On");
    count=send_note(instance,1,60);assert(count==2&&output[0][0]==0xb0&&output[0][1]==10&&output[0][2]>64);
    API.set_param(instance,"motion_hold_1","Off");count=API.tick(instance,128,48000,output,lengths,64);
    assert(count==1&&output[0][0]==0xb0&&output[0][2]==64);send_note(instance,0,60);
    API.set_param(instance,"motion_operation","Octave");API.set_param(instance,"motion_hold_1","On");send_note(instance,1,60);
    uint8_t stop=0xfc;API.process_midi(instance,&stop,1,output,lengths,64);
    assert(instance->motion.held==0);
    API.tick(instance,128,48000,output,lengths,64);
    assert(!instance->motion_local.owned&&!instance->motion_render.owned);
}
static void harmony_choice(void){
    Inst *instance=motion_fixture();hb_harmony_t current=bus_read();
    uint8_t notes[3]={65,69,72};hb_harmony_t upcoming=hb_infer_harmony(notes,3);
    g_bus.clip_loop_start=0;g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0].phase=0;g_bus.next_model[0].harmony=current;
    g_bus.next_model[1].phase=2;g_bus.next_model[1].harmony=upcoming;
    g_bus.next_lookahead=4;position=1.5;
    API.set_param(instance,"motion_operation","Harmony");API.set_param(instance,"motion_amount","100");
    assert(hb_render_harmony(instance).root_pc==upcoming.root_pc);
    API.set_param(instance,"motion_amount","0");assert(hb_render_harmony(instance).root_pc==current.root_pc);
    assert(bus_read().root_pc==current.root_pc); /* lane never mutates the shared bus */
    g_bus.next_model_locked=0;API.set_param(instance,"motion_amount","100");
    assert(hb_render_harmony(instance).root_pc==current.root_pc);
}
static void ownership_and_recording(void){
    hb_motion_route route;hb_mo_route_init(&route);uint8_t message[3]={0x90,61,100},event[3];
    hb_mo_event(&route,message,61,100,-1,-1,0);
    message[1]=60;hb_mo_event(&route,message,72,100,-1,-1,0);
    message[0]=0x80;message[1]=61;hb_mo_event(&route,message,61,0,-1,-1,0);
    message[0]=0x90;message[1]=60;hb_mo_event(&route,message,84,100,-1,-1,0);
    while(hb_mo_pop(&route,event)){}
    message[0]=0x80;hb_mo_event(&route,message,60,0,-1,-1,0);
    assert(hb_mo_pop(&route,event)&&event[1]==72); /* reused slot cannot steal older OFF */
    hb_mo_event(&route,message,60,0,-1,-1,0);assert(hb_mo_pop(&route,event)&&event[1]==84);
    assert(!route.owned);
    message[0]=0x90;hb_mo_event(&route,message,72,100,-1,-1,0);
    message[1]=61;hb_mo_event(&route,message,72,100,-1,-1,0);
    while(hb_mo_pop(&route,event)){}
    message[0]=0x80;hb_mo_event(&route,message,61,0,-1,-1,0);assert(!hb_mo_pop(&route,event));
    message[1]=60;hb_mo_event(&route,message,60,0,-1,-1,0);assert(hb_mo_pop(&route,event)&&event[1]==72);
    Inst *instance=motion_fixture();API.set_param(instance,"role","Conductor");instance->movy_track=0;
    API.set_param(instance,"chord_mode","Scale Degree");API.set_param(instance,"motion_operation","Octave");
    send_note(instance,1,60);
    assert(recorded_count==3&&recorded[0][2]==60); /* source capture is before output lanes */
    assert(render_count>=3&&rendered[0][2]==72);
    send_note(instance,0,60);
    assert(recorded_count==6&&recorded[3][2]==60);
    assert(!instance->motion_local.owned&&!instance->motion_render.owned);
}
static void performance_buttons(void){
    Inst *instance=motion_fixture();
    API.set_param(instance,"performance_below","On");
    assert(send_note(instance,1,60)==1&&output[0][1]==59);
    API.set_param(instance,"performance_below","Off");
    assert(send_note(instance,0,60)==1&&output[0][1]==59);
    API.set_param(instance,"performance_below","On");API.set_param(instance,"performance_above","On");
    assert(send_note(instance,1,60)==1&&output[0][1]==62);send_note(instance,0,60);
    API.set_param(instance,"performance_above","Off");
    assert(send_note(instance,1,60)==1&&output[0][1]==59);send_note(instance,0,60);
    API.set_param(instance,"performance_below","Off");
    assert(send_note(instance,1,60)==1&&output[0][1]==60);send_note(instance,0,60);
    for(int order=0;order<2;order++){
        const char *key=order?"performance_enclose_ba":"performance_enclose_ab";
        API.set_param(instance,key,"On");API.set_param(instance,key,"Off");
        for(int step=0;step<4;step++){
            int expected=step>=2?60:((step==0)==(order==0)?62:59);
            assert(send_note(instance,1,60)==1&&output[0][1]==expected);
            assert(rendered[render_count-1][2]==expected);
            assert(send_note(instance,0,60)==1&&output[0][1]==expected);
            assert(rendered[render_count-1][2]==expected);
        }
        assert(!instance->motion.enclosure&&!instance->motion_local.owned&&!instance->motion_render.owned);
    }
    API.set_param(instance,"performance_enclose_ab","On");
    send_note(instance,1,60);send_note(instance,0,60);
    API.set_param(instance,"performance_enclose_ab","On"); /* retrigger starts again */
    assert(send_note(instance,1,60)==1&&output[0][1]==62);send_note(instance,0,60);
    API.set_param(instance,"performance_below","On"); /* explicit hold replaces sequence */
    assert(!instance->motion.enclosure);
    char state[1024];API.get_param(instance,"state",state,sizeof(state));
    API.set_param(instance,"state",state);assert(!instance->motion.pitch_held&&!instance->motion.enclosure);
    API.set_param(instance,"performance_enclose_ab","On");
    API.set_param(instance,"performance_reset","1");assert(!hb_mo_enabled(&instance->motion));
    /* A simultaneous three-voice chord advances once, on both output routes. */
    instance=motion_fixture();API.set_param(instance,"performance_enclose_ab","On");
    const int pitches[3]={60,64,67};
    const int expected[3][3]={{62,65,69},{59,63,66},{60,64,67}};
    for(int step=0;step<3;step++){
        position=step*0.5;
        for(int voice=0;voice<3;voice++)midi(instance,1,pitches[voice]);
        int count=API.tick(instance,128,48000,output,lengths,64);assert(count==3);
        for(int voice=0;voice<3;voice++)assert(output[voice][1]==expected[step][voice]);
        for(int voice=0;voice<3;voice++)midi(instance,0,pitches[voice]);
        API.tick(instance,128,48000,output,lengths,64);
    }
    assert(!instance->motion.enclosure);
}
static void sixteen_slots_and_capabilities(void){
    Inst *instance=motion_fixture();char saved[8192],after[8192],metadata[65536];
    expect_param(instance,"motion_row","0,0,0,0,0,0,0,0,0,0,0,0,0,8,9,10,11");
    for(int lane=13;lane<=16;lane++){
        char value[16];snprintf(value,sizeof(value),"%d",lane);API.set_param(instance,"motion_lane",value);
        assert(instance->motion.lanes[lane-1].operation==HB_MO_BELOW+lane-13);
        expect_param(instance,"motion_enabled","Off");
    }
    API.set_param(instance,"motion_hold_13","On");
    assert(send_note(instance,1,60)==1&&output[0][1]==59);send_note(instance,0,60);
    API.set_param(instance,"motion_hold_14","On");
    assert(send_note(instance,1,60)==1&&output[0][1]==62);send_note(instance,0,60);
    API.set_param(instance,"motion_hold_14","Off");
    assert(send_note(instance,1,60)==1&&output[0][1]==59);send_note(instance,0,60);
    API.set_param(instance,"motion_hold_13","Off");
    for(int lane=15;lane<=16;lane++){
        char key[32];snprintf(key,sizeof(key),"motion_hold_%d",lane);
        API.set_param(instance,key,"On");unsigned revision=instance->motion.enclosure_revision;
        API.set_param(instance,key,"On");assert(instance->motion.enclosure_revision==revision);
        API.set_param(instance,key,"Off");assert(instance->motion.enclosure);
        int expected[3]={lane==15?62:59,lane==15?59:62,60};
        for(int step=0;step<3;step++){assert(send_note(instance,1,60)==1&&output[0][1]==expected[step]);send_note(instance,0,60);}
        assert(!instance->motion.enclosure);
    }
    API.set_param(instance,"motion_operation","Clip Reverse");
    API.set_param(instance,"motion_enabled","On");expect_param(instance,"motion_enabled","Off");
    API.set_param(instance,"motion_hold_16","On");assert(!hb_mo_enabled(&instance->motion));
    expect_param(instance,"motion_punch","Requires Movy");
    API.get_param(instance,"state",saved,sizeof(saved));
    assert(API.get_param(instance,"chain_params",metadata,sizeof(metadata))>0);
    assert(strstr(metadata,"Clip Reverse")&&!strstr(metadata,"Clip Repeat"));
    API.set_param(instance,"motion_host","movy-clip-v1");
    assert(API.get_param(instance,"chain_params",metadata,sizeof(metadata))>0&&strstr(metadata,"Clip Repeat"));
    API.get_param(instance,"state",after,sizeof(after));assert(!strcmp(saved,after));
    API.set_param(instance,"state",saved);assert(instance->motion.host_capabilities&&!instance->motion.held);
    Inst *standalone=API.create_instance("",NULL);API.set_param(standalone,"state",saved);
    assert(!standalone->motion.host_capabilities);expect_param(standalone,"motion_operation","Clip Reverse");
    expect_param(standalone,"motion_punch","Requires Movy");
    char tiny[16];assert(API.get_param(instance,"chain_params",tiny,sizeof(tiny))<0&&tiny[15]==0);
    // Every numeric slot parses independently; the older first-four state still loads.
    hb_mo_restore(&instance->motion,";mo1,0,3,0,2,0,0,3,3,0,100,0,0");
    assert(instance->motion.lanes[0].operation==HB_MO_OCTAVE&&instance->motion.lanes[0].amount==2);
    assert(instance->motion.lanes[15].operation==HB_MO_ENCLOSE_BA);
    API.set_param(instance,"motion_lane","16");API.set_param(instance,"motion_operation","Off");
    API.get_param(instance,"state",saved,sizeof(saved));API.set_param(instance,"state",saved);
    assert(instance->motion.lanes[15].operation==HB_MO_OFF);
}

static void advancement_modes(void){
    Inst *instance=motion_fixture();
    API.set_param(instance,"motion_operation","Transpose");API.set_param(instance,"motion_amount","12");
    API.set_param(instance,"motion_pattern","Alternate");API.set_param(instance,"motion_advance","Note");
    assert(send_note(instance,1,60)==1&&output[0][1]==48);send_note(instance,0,60);
    assert(send_note(instance,1,60)==1&&output[0][1]==72);send_note(instance,0,60);
    assert(instance->motion.events[0]==2); /* local/render copies do not double count */
    double value;for(int index=0;index<10;index++)hb_mo_value(&instance->motion,0,100,60,&value);
    assert(instance->motion.events[0]==2);
    uint8_t ignored[3]={0x91,62,100};API.process_midi(instance,ignored,3,output,lengths,64);
    assert(instance->motion.events[0]==2);
    API.set_param(instance,"motion_advance","Chord");
    assert(send_note(instance,1,60)==1&&output[0][1]==48);
    assert(send_note(instance,1,64)==1&&output[0][1]==52);
    assert(instance->motion.events[0]==1);
    send_note(instance,0,60);send_note(instance,0,64);
    assert(send_note(instance,1,60)==1&&output[0][1]==72);send_note(instance,0,60);
    char saved[4096];API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";ma1,0,2"));
    API.set_param(instance,"state",saved);expect_param(instance,"motion_advance","Chord");assert(!instance->motion.events[0]);
    API.set_param(instance,"motion_advance","Clock");expect_param(instance,"motion_advance","Clock");
}
static void buffered_advancement(void){
    Inst *instance=motion_fixture();
    API.set_param(instance,"motion_operation","Transpose");API.set_param(instance,"motion_amount","12");
    API.set_param(instance,"motion_pattern","Alternate");API.set_param(instance,"motion_advance","Note");
    uint8_t first[3]={0x90,60,100},second[3]={0x90,64,100};
    assert(API.process_midi(instance,first,3,output,lengths,64)==0);
    assert(API.process_midi(instance,second,3,output,lengths,64)==0);
    int count=advance(instance,10,64);assert(count==2);
    assert(output[0][1]==48&&output[1][1]==76);
    assert(render_count==2&&rendered[0][2]==48&&rendered[1][2]==76);
    first[0]=second[0]=0x80;API.process_midi(instance,first,3,output,lengths,64);API.process_midi(instance,second,3,output,lengths,64);
    count=advance(instance,10,64);assert(count==2&&output[0][1]==48&&output[1][1]==76);
    /* Host Stop cancels repeats before the scheduler gets a due callback. */
    API.set_param(instance,"motion_operation","MIDI Echo");send_note(instance,1,60);send_note(instance,0,60);
    transport=MOVE_CLOCK_STATUS_STOPPED;count=advance(instance,250,64);assert(count==0);
    for(int index=0;index<HB_MOTION_BURSTS;index++)assert(!instance->motion_local.bursts[index].used&&!instance->motion_render.bursts[index].used);
}
static int burst_count(hb_motion_route *route){int count=0;for(int index=0;index<HB_MOTION_BURSTS;index++)count+=route->bursts[index].used;return count;}
static void repeat_idle_lifecycle(void){
    Inst *instance=motion_fixture();transport=MOVE_CLOCK_STATUS_STOPPED;
    API.set_param(instance,"motion_operation","Velocity");
    send_note(instance,1,60);
    assert(!instance->motion_local.repeat_pending&&!instance->motion_render.repeat_pending);
    send_note(instance,0,60);
    API.set_param(instance,"motion_operation","MIDI Echo");
    API.set_param(instance,"motion_amount","1");
    send_note(instance,1,60);send_note(instance,0,60);
    assert(instance->motion_local.repeat_pending&&instance->motion_render.repeat_pending);
    assert(advance(instance,250,64)==1&&output[0][0]==0x90);
    assert(advance(instance,125,64)==1&&output[0][0]==0x80);
    advance(instance,1,64);
    assert(!instance->motion_local.repeat_pending&&!instance->motion_render.repeat_pending);
    send_note(instance,1,62);send_note(instance,0,62);
    assert(instance->motion_local.repeat_pending);
    API.set_param(instance,"motion_bypass","On");
    assert(!instance->motion_local.repeat_pending&&!instance->motion_render.repeat_pending);
}
static void repeats_production(void){
    Inst *instance=motion_fixture();transport=MOVE_CLOCK_STATUS_STOPPED;
    API.set_param(instance,"motion_operation","Ratchet");API.set_param(instance,"motion_advance","Note");
    assert(send_note(instance,1,60)==1&&output[0][2]==100);
    assert(burst_count(&instance->motion_local)==1&&burst_count(&instance->motion_render)==1);
    assert(advance(instance,32,64)==1&&output[0][0]==0x80);
    for(int index=0;index<3;index++){
        assert(advance(instance,32,64)==1&&output[0][0]==0x90&&output[0][1]==60);
        assert(advance(instance,32,64)==1&&output[0][0]==0x80);
    }
    assert(instance->motion.events[0]==1&&!burst_count(&instance->motion_local));
    assert(render_count==8); /* original + three attacks, each paired */
    assert(send_note(instance,0,60)==0&&!instance->motion_local.owned&&!instance->motion_render.owned);
    API.set_param(instance,"motion_operation","MIDI Echo");expect_param(instance,"motion_offset","25");
    API.set_param(instance,"motion_offset","50");
    assert(send_note(instance,1,60)==1);assert(send_note(instance,0,60)==1);
    for(int index=0;index<3;index++){
        int count=advance(instance,index?125:250,64);assert(count==1&&output[0][0]==0x90);
        assert(output[0][2]==(index==0?50:index==1?25:13));
        assert(advance(instance,125,64)==1&&output[0][0]==0x80);
    }
    assert(!instance->motion_local.owned&&!instance->motion_render.owned&&!burst_count(&instance->motion_local));
    /* Manual release closes generated voices; a new press cannot revive old work. */
    API.set_param(instance,"motion_enabled","Off");API.set_param(instance,"motion_hold_1","On");
    send_note(instance,1,60);send_note(instance,0,60);assert(advance(instance,250,64)==1&&output[0][0]==0x90);
    API.set_param(instance,"motion_hold_1","Off");API.set_param(instance,"motion_hold_1","On");
    assert(advance(instance,1,64)==1&&output[0][0]==0x80);
    assert(!burst_count(&instance->motion_local)&&advance(instance,2000,64)==0);
    send_note(instance,1,60);send_note(instance,0,60);
    uint8_t stop=0xfc;API.process_midi(instance,&stop,1,output,lengths,64);
    assert(!burst_count(&instance->motion_local)&&!burst_count(&instance->motion_render));
    assert(advance(instance,2000,64)==0&&!instance->motion.events[0]);
    /* State restoration cancels queued repeats without stealing source note-offs. */
    API.set_param(instance,"motion_hold_1","On");send_note(instance,1,60);
    char saved[4096];API.get_param(instance,"state",saved,sizeof(saved));API.set_param(instance,"state",saved);
    assert(!burst_count(&instance->motion_local));assert(send_note(instance,0,60)==1&&output[0][0]==0x80);
}
static void repeat_capacity_and_collisions(void){
    hb_motion_config config;hb_mo_defaults(&config);config.lanes[0].operation=HB_MO_ECHO;config.lanes[0].amount=3;
    hb_motion_route route;hb_mo_route_init(&route);uint8_t message[3]={0x90,60,100},event[3];
    hb_mo_event(&route,message,60,100,-1,-1,0);hb_mo_repeat_schedule(&route,&config,message,60,100,0,0,0);
    while(hb_mo_pop(&route,event)){};
    hb_mo_repeat_tick(&route,&config,0.5);assert(hb_mo_pop(&route,event)&&event[0]==0x90);
    config.bypass=1;hb_mo_repeat_cancel(&route,&config,0);
    assert(route.refs[0][60]==1&&!hb_mo_pop(&route,event)); /* original held owner survives */
    message[0]=0x80;assert(hb_mo_event(&route,message,60,0,-1,-1,0));assert(hb_mo_pop(&route,event)&&event[0]==0x80);
    config.bypass=0;message[0]=0x90;
    for(int index=0;index<HB_MOTION_BURSTS+5;index++){
        assert(hb_mo_event(&route,message,60,100,-1,-1,0));hb_mo_repeat_schedule(&route,&config,message,60,100,0,0,0);
    }
    assert(burst_count(&route)==HB_MOTION_BURSTS);
    while(hb_mo_pop(&route,event)){};
    hb_mo_repeat_tick(&route,&config,100); /* delayed block drops missed repeats */
    assert(route.count==HB_MOTION_BURSTS&&!burst_count(&route));
    hb_mo_panic(&route);assert(!route.owned&&!route.refs[0][60]);
}


static void cycle_conditions(void){
    Inst *instance=motion_fixture();
    API.set_param(instance,"motion_operation","Octave");
    API.set_param(instance,"motion_every","4");API.set_param(instance,"motion_from","4");
    expect_param(instance,"motion_through","4");expect_param(instance,"motion_condition_range","4 of 4");
    char row[256];
    const double beats[]={0,4,8,11.999,12,15.999,16,28};
    for(int index=0;index<8;index++){
        position=beats[index];int eligible=(index==4||index==5||index==7);
        assert(send_note(instance,1,60)==1&&output[0][1]==(eligible?72:60));
        assert(rendered[render_count-1][2]==output[0][1]);
        assert(send_note(instance,0,60)==1&&output[0][1]==(eligible?72:60));
        API.get_param(instance,"motion_row",row,sizeof(row));assert((strtoul(row,0,10)&1u)==(unsigned)eligible);
    }
    position=8;expect_param(instance,"motion_condition_status","3/4 Waiting");
    position=12;expect_param(instance,"motion_condition_status","4/4 Ready");
    /* Phase and input advancement do not move the transport condition window. */
    API.set_param(instance,"motion_phase","64");API.set_param(instance,"motion_advance","Note");
    position=0;assert(send_note(instance,1,60)==1&&output[0][1]==60);send_note(instance,0,60);
    position=12;assert(send_note(instance,1,60)==1&&output[0][1]==72);send_note(instance,0,60);
    API.set_param(instance,"motion_probability","0");assert(send_note(instance,1,60)==1&&output[0][1]==60);send_note(instance,0,60);
    position=0;API.set_param(instance,"motion_enabled","Off");API.set_param(instance,"motion_bypass","On");API.set_param(instance,"motion_hold_1","On");
    expect_param(instance,"motion_condition_status","1/4 Held");
    assert(send_note(instance,1,60)==1&&output[0][1]==72);
    API.set_param(instance,"motion_hold_1","Off");assert(send_note(instance,0,60)==1&&output[0][1]==72);
    API.set_param(instance,"motion_bypass","Off");API.set_param(instance,"motion_enabled","On");API.set_param(instance,"motion_probability","100");
    /* Closed window must not cancel a note's captured pitch/off pairing. */
    position=12;assert(send_note(instance,1,60)==1&&output[0][1]==72);
    position=16;assert(send_note(instance,0,60)==1&&output[0][1]==72);
    /* Ranges, shorter cycle lengths, independent lanes and seeks. */
    API.set_param(instance,"motion_every","8");API.set_param(instance,"motion_from","7");API.set_param(instance,"motion_through","8");
    expect_param(instance,"motion_condition_range","7-8 of 8");
    API.set_param(instance,"motion_cycle","1/4");
    for(int cycle=0;cycle<16;cycle++){
        position=cycle;assert(send_note(instance,1,60)==1&&output[0][1]==(cycle%8>=6?72:60));send_note(instance,0,60);
    }
    API.set_param(instance,"motion_lane","2");expect_param(instance,"motion_every","1");
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_every","2");
    expect_param(instance,"motion_from","2");expect_param(instance,"motion_through","2");
    API.set_param(instance,"motion_through","1");expect_param(instance,"motion_from","1");
    API.set_param(instance,"motion_every","99");expect_param(instance,"motion_every","16");
    API.set_param(instance,"motion_from","99");expect_param(instance,"motion_from","16");expect_param(instance,"motion_through","16");
    /* Stopped conditional lanes wait, but held and unrestricted lanes still work. */
    transport=MOVE_CLOCK_STATUS_STOPPED;advance(instance,1,64);expect_param(instance,"motion_condition_status","-/16 Stopped");
    assert(send_note(instance,1,60)==1&&output[0][1]==60);send_note(instance,0,60);
    API.set_param(instance,"motion_hold_1","On");assert(send_note(instance,1,60)==1&&output[0][1]==72);send_note(instance,0,60);
    API.set_param(instance,"motion_hold_1","Off");API.set_param(instance,"motion_every","1");
    assert(send_note(instance,1,60)==1&&output[0][1]==72);send_note(instance,0,60);
}
static void condition_state_and_repeat_tails(void){
    Inst *instance=motion_fixture();char legacy[4096],saved[4096],restored[4096];
    API.get_param(instance,"state",legacy,sizeof(legacy));assert(!strstr(legacy,";mcond1,"));
    for(int lane=1;lane<=16;lane++){
        char number[16];snprintf(number,sizeof(number),"%d",lane);API.set_param(instance,"motion_lane",number);
        API.set_param(instance,"motion_every","16");API.set_param(instance,"motion_from",number);
    }
    API.get_param(instance,"state",saved,sizeof(saved));API.set_param(instance,"state",saved);
    API.get_param(instance,"state",restored,sizeof(restored));assert(!strcmp(saved,restored));
    for(int lane=0;lane<16;lane++)assert(instance->motion.lanes[lane].every==16&&instance->motion.lanes[lane].from==lane+1&&instance->motion.lanes[lane].through==lane+1);
    API.set_param(instance,"state",legacy);API.get_param(instance,"state",restored,sizeof(restored));assert(!strcmp(legacy,restored));
    hb_mo_restore(&instance->motion,";mcond1,0,4,4,3;mcond1,1,17,1,1;mcond1,2,4,1,5;mcond1,3,4,1,2garbage;mcond1,15,8,7,8");
    for(int lane=0;lane<4;lane++)assert(instance->motion.lanes[lane].every==1);
    assert(instance->motion.lanes[15].every==8&&instance->motion.lanes[15].from==7);
    API.set_param(instance,"state",legacy);API.set_param(instance,"motion_operation","MIDI Echo");
    API.set_param(instance,"motion_amount","2");API.set_param(instance,"motion_every","4");API.set_param(instance,"motion_from","4");
    position=8;send_note(instance,1,60);send_note(instance,0,60);assert(!burst_count(&instance->motion_local));
    position=15.9;send_note(instance,1,60);send_note(instance,0,60);assert(burst_count(&instance->motion_local)==1);
    assert(advance(instance,250,64)==1&&output[0][0]==0x90); /* tail crosses cycle boundary */
    API.set_param(instance,"motion_hold_1","On");send_note(instance,1,64);send_note(instance,0,64);
    API.set_param(instance,"motion_hold_1","Off");
    assert(burst_count(&instance->motion_local)==1); /* only original automatic tail remains */
    uint8_t stop=0xfc;API.process_midi(instance,&stop,1,output,lengths,64);assert(!burst_count(&instance->motion_local));
}

static void automatic_clip_config(void){
    Inst *instance=motion_fixture();char config[2048],saved[4096];
    API.set_param(instance,"motion_operation","Clip Repeat");API.set_param(instance,"motion_enabled","On");
    expect_param(instance,"motion_enabled","Off");
    API.set_param(instance,"motion_host","movy-clip-v2");API.set_param(instance,"motion_enabled","On");
    API.set_param(instance,"motion_every","4");API.set_param(instance,"motion_from","4");
    expect_param(instance,"motion_clip_config","mca1;0,12,1,3,3,4,4,4,100,0");
    API.get_param(instance,"state",saved,sizeof(saved));API.set_param(instance,"state",saved);
    expect_param(instance,"motion_enabled","On");
    API.set_param(instance,"motion_bypass","On");expect_param(instance,"motion_clip_config","mca1");
    API.set_param(instance,"motion_bypass","Off");
    API.set_param(instance,"motion_host","movy-clip-v1");expect_param(instance,"motion_clip_config","mca1");
    assert(!hb_mo_lane_active(&instance->motion,0));API.set_param(instance,"motion_hold_1","On");assert(hb_mo_lane_active(&instance->motion,0));
    API.set_param(instance,"motion_host","movy-clip-v2");
    for(int lane=1;lane<=16;lane++){
        char number[16];snprintf(number,sizeof(number),"%d",lane);API.set_param(instance,"motion_lane",number);
        API.set_param(instance,"motion_operation","Clip Speed");API.set_param(instance,"motion_enabled","On");
    }
    assert(API.get_param(instance,"motion_clip_config",config,sizeof(config))>0);assert(strstr(config,";15,15,"));
    char tiny[8];assert(API.get_param(instance,"motion_clip_config",tiny,sizeof(tiny))<0&&tiny[7]==0);
}
static void gesture_tap(Inst *instance,const char *key,int milliseconds){
    char release[32];snprintf(release,sizeof(release),"Up,%d",milliseconds);
    API.set_param(instance,key,"Down");API.set_param(instance,key,release);
}
static void ordered_gestures(void){
    for(int reverse=0;reverse<2;reverse++){
        Inst *instance=motion_fixture();
        const char *first=reverse?"motion_gesture_13":"performance_gesture_above";
        const char *second=reverse?"performance_gesture_above":"motion_gesture_13";
        gesture_tap(instance,first,80);gesture_tap(instance,second,90);
        assert(instance->motion.enclosure==(reverse?2:1));
        const int notes[3]={reverse?59:62,reverse?62:59,60};
        for(int step=0;step<3;step++){
            assert(send_note(instance,1,60)==1&&output[0][1]==notes[step]);
            assert(send_note(instance,0,60)==1&&output[0][1]==notes[step]);
        }
        assert(!instance->motion.enclosure);
        API.destroy_instance(instance);
    }
    Inst *instance=motion_fixture();
    // Press order wins even when fingers release in the opposite order.
    API.set_param(instance,"performance_gesture_above","Down");
    API.set_param(instance,"performance_gesture_below","Down");
    API.set_param(instance,"performance_gesture_below","Up,80");
    API.set_param(instance,"performance_gesture_above","Up,100");
    assert(instance->motion.enclosure==1);
    API.set_param(instance,"performance_reset","1");
    gesture_tap(instance,"performance_gesture_above",80);
    gesture_tap(instance,"performance_gesture_below",80);
    gesture_tap(instance,"performance_gesture_above",80); /* remove only above */
    assert(instance->motion.enclosure==4);
    gesture_tap(instance,"performance_gesture_above",80); /* now below then above */
    assert(instance->motion.enclosure==2);
    gesture_tap(instance,"performance_gesture_below",80);
    gesture_tap(instance,"performance_gesture_above",80);assert(!instance->motion.enclosure);
    API.set_param(instance,"performance_gesture_below","Down");
    for(int index=0;index<3;index++){assert(send_note(instance,1,60)==1&&output[0][1]==59);send_note(instance,0,60);}
    API.set_param(instance,"performance_gesture_below","Up,400");
    assert(send_note(instance,1,60)==1&&output[0][1]==60);send_note(instance,0,60);
    // A short press already used while down must not arm an extra note.
    API.set_param(instance,"performance_gesture_above","Down");
    assert(send_note(instance,1,60)==1&&output[0][1]==62);send_note(instance,0,60);
    API.set_param(instance,"performance_gesture_above","Up,80");
    assert(send_note(instance,1,60)==1&&output[0][1]==60);send_note(instance,0,60);
    // Exactly the threshold is a hold, and Cancel never arms a pending note.
    gesture_tap(instance,"performance_gesture_above",350);assert(!instance->motion.enclosure);
    API.set_param(instance,"performance_gesture_above","Down");API.set_param(instance,"performance_gesture_above","Cancel");assert(!instance->motion.enclosure);
    gesture_tap(instance,"motion_gesture_15",80);assert(instance->motion.enclosure==1);
    gesture_tap(instance,"motion_gesture_15",80);assert(!instance->motion.enclosure);
    API.set_param(instance,"motion_lane","15");API.set_param(instance,"motion_touch_mode","Hold");
    gesture_tap(instance,"motion_gesture_15",80);assert(!instance->motion.enclosure);
    // Continuous operations latch on short taps and release on long holds.
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Octave");API.set_param(instance,"motion_enabled","Off");
    gesture_tap(instance,"motion_gesture_1",80);assert(instance->motion.held&1);
    assert(send_note(instance,1,60)==1&&output[0][1]==72);send_note(instance,0,60);
    gesture_tap(instance,"motion_gesture_1",80);assert(!(instance->motion.held&1));
    gesture_tap(instance,"motion_gesture_1",400);assert(!(instance->motion.held&1));
    API.set_param(instance,"motion_touch_mode","Toggle");gesture_tap(instance,"motion_gesture_1",400);assert(instance->motion.held&1);
    gesture_tap(instance,"motion_gesture_1",400);assert(!(instance->motion.held&1));
    API.set_param(instance,"motion_touch_mode","Hold");gesture_tap(instance,"motion_gesture_1",80);assert(!(instance->motion.held&1));
    API.set_param(instance,"touch_hold_ms","350");
    char saved[16384];API.get_param(instance,"state",saved,sizeof(saved));
    API.set_param(instance,"motion_touch_mode","Tap/Hold");API.set_param(instance,"state",saved);
    assert(instance->motion.lanes[0].touch_mode==0&&g_hb_hold_ms==350);
    gesture_tap(instance,"performance_gesture_above",300);assert(instance->motion.enclosure==3);
    API.set_param(instance,"performance_reset","1");assert(!instance->motion.enclosure&&!instance->motion.gesture_down);
    API.destroy_instance(instance);
}
static void source_gesture_consumption(void){
    Inst *instance=motion_fixture();
    gesture_tap(instance,"performance_gesture_above",80);
    gesture_tap(instance,"performance_gesture_below",80);
    expect_param(instance,"approach_reset","Above > Below > Target");
    /* Three presses queued before a tick retain three independent choices. */
    for(int step=0;step<3;step++){midi(instance,1,60);midi(instance,0,60);}
    int count=API.tick(instance,128,48000,output,lengths,64);
    assert(count==6);
    const int expected[]={62,62,59,59,60,60};
    for(int index=0;index<count;index++)assert(output[index][1]==expected[index]);
    expect_param(instance,"approach_reset","Off");
    API.destroy_instance(instance);

    /* Generated voices and time-separated arp steps all belong to one press. */
    instance=motion_fixture();
    API.set_param(instance,"chord_mode","Conductor Chord");
    API.set_param(instance,"arp_playback","Repeat Arp");
    API.set_param(instance,"arp_start","Immediate");
    gesture_tap(instance,"performance_gesture_above",80);
    gesture_tap(instance,"performance_gesture_below",80);
    midi(instance,1,60);advance(instance,0,64);
    assert(instance->motion.enclosure_step==1);
    expect_param(instance,"approach_reset","Below > Target");
    for(int tick=0;tick<16;tick++)advance(instance,125,64);
    assert(instance->motion.enclosure_step==1);
    expect_param(instance,"approach_reset","Below > Target");
    for(int key=0;key<HB_CP_KEYS;key++)if(instance->player.keys[key].used){
        assert(instance->player.keys[key].notes[0]==62);
        assert(instance->motion_player_events[key][HB_MOTION_LANES]==2);
    }
    midi(instance,0,60);advance(instance,0,64);
    midi(instance,1,60);advance(instance,0,64);
    assert(instance->motion.enclosure_step==2);
    expect_param(instance,"approach_reset","Armed Target");
    for(int key=0;key<HB_CP_KEYS;key++)if(instance->player.keys[key].used)assert(instance->player.keys[key].notes[0]==59);
    API.destroy_instance(instance);

    instance=motion_fixture();
    API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Triad");
    API.set_param(instance,"arp_playback","Once");API.set_param(instance,"strum_spread","150 ms");
    gesture_tap(instance,"performance_gesture_above",80);gesture_tap(instance,"performance_gesture_below",80);
    midi(instance,1,60);assert(advance(instance,0,64)==1&&output[0][1]==62);
    assert(advance(instance,75,64)==1&&output[0][1]==65);
    assert(advance(instance,75,64)==1&&output[0][1]==69);
    assert(instance->motion.enclosure_step==1);
    API.destroy_instance(instance);

    instance=motion_fixture();
    API.set_param(instance,"motion_lane","13");API.set_param(instance,"motion_touch_mode","Arm");
    expect_param(instance,"motion_touch_mode","Arm");
    gesture_tap(instance,"motion_gesture_13",80);expect_param(instance,"motion_punch","Armed Below");
    gesture_tap(instance,"motion_gesture_13",80);expect_param(instance,"motion_punch","Off");
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Octave");
    API.set_param(instance,"motion_enabled","Off");API.set_param(instance,"motion_touch_mode","Latch");
    expect_param(instance,"motion_touch_mode","Latch");
    API.set_param(instance,"motion_gesture_1","Down");expect_param(instance,"motion_punch","Held");
    API.set_param(instance,"motion_gesture_1","Up,80");expect_param(instance,"motion_punch","Latched");
    gesture_tap(instance,"motion_gesture_1",80);expect_param(instance,"motion_punch","Off");
    API.set_param(instance,"motion_gesture_1","Down");API.set_param(instance,"approach_reset","Reset");
    API.set_param(instance,"motion_gesture_1","Up,80");
    assert(!instance->motion.held&&!instance->motion.gesture_latched);
    API.destroy_instance(instance);
}
int main(void){source_gesture_consumption();ordered_gestures();
    repeat_idle_lifecycle();
    automatic_clip_config();
    cycle_conditions();condition_state_and_repeat_tails();
    buffered_advancement();advancement_modes();repeats_production();repeat_capacity_and_collisions();
    sixteen_slots_and_capabilities();controls_and_state();punch_and_release();lanes_and_patterns();gate_pan_and_stop();harmony_choice();ownership_and_recording();performance_buttons();
    puts("motion: persistence, isolation, punch ownership, stacked lanes, patterns, gate, pan, stop and harmony pass");
    return 0;
}
