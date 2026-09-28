#define main existing_chord_player_tests
#include "chord_player_test.c"
#undef main
static void tap(Inst *instance,int lane){char key[40];snprintf(key,sizeof(key),"motion_gesture_%d",lane);API.set_param(instance,key,"Touch");API.set_param(instance,key,"Up,50");}
static unsigned played(Inst *instance,int input){
    render_count=0;midi(instance,1,input);advance(instance,1,64);
    unsigned mask=0;for(int event=0;event<render_count;event++)if((rendered[event][1]&0xf0)==0x90&&rendered[event][3])mask|=1u<<mod12(rendered[event][2]);
    return mask;
}
static void release(Inst *instance,int input){midi(instance,0,input);advance(instance,80,64);assert(!instance->player.sounding_count);}
static unsigned tones(int root,int third,int fifth,int seventh){return (1u<<mod12(root))|(1u<<mod12(root+third))|(1u<<mod12(root+fifth))|(1u<<mod12(root+seventh));}
static Inst *setup(void){
    Inst *instance=fixture();instance->travel_map=7;instance->content_map=1;
    API.set_param(instance,"chord_form","Seventh");
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Secondary II");
    API.set_param(instance,"motion_lane","2");API.set_param(instance,"motion_operation","Secondary V");
    assert(instance->motion.lanes[0].operation==HB_MO_SECONDARY_II&&!instance->motion.lanes[0].enabled);
    assert(instance->motion.lanes[1].operation==HB_MO_SECONDARY_V&&!instance->motion.lanes[1].enabled);
    API.set_param(instance,"motion_lane","3");API.set_param(instance,"motion_operation","Secondary VI");
    return instance;
}
static void cadence_orders(void){
    for(int minor=0;minor<2;minor++)for(int reverse=0;reverse<2;reverse++)for(int mode=0;mode<3;mode++)for(int top=0;top<2;top++){
        Inst *instance=setup();instance->player.config.mode=mode;instance->player.config.inversion=top?8:0;
        int target=minor?69:60;
        tap(instance,reverse?2:1);tap(instance,reverse?1:2);
        assert(instance->motion.enclosure==(reverse?6:5));
        for(int step=0;step<3;step++){
            unsigned expected=step==2?tones(target,minor?3:4,7,minor?10:11):
                ((step==0)!=reverse)?tones(target+2,3,minor?6:7,10):tones(target-5,4,7,10);
            if(!mode)expected=1u<<mod12(target+(step==2?0:((step==0)!=reverse)?2:-5));
            unsigned actual=played(instance,target);
            if(actual!=expected)fprintf(stderr,"cadence minor%d reverse%d mode%d top%d step%d got%x expected%x\n",minor,reverse,mode,top,step,actual,expected);
            assert(actual==expected);release(instance,target);
            assert(instance->player.config.mode==mode);
        }
        assert(!instance->motion.enclosure);
        if(!mode){assert(played(instance,target)==(1u<<mod12(target)));release(instance,target);}
        API.destroy_instance(instance);
    }
}
static void automatic_approaches(void){
    for(int minor=0;minor<2;minor++)for(int above=0;above<2;above++)for(int top=0;top<2;top++){
        Inst *instance=setup();instance->player.config.mode=1;instance->player.config.inversion=top?8:0;
        API.set_param(instance,"chromatic_quality","Auto Dim7 / Min7b5");assert(instance->player.config.chromatic_quality==6);
        int target=minor?69:60;
        tap(instance,above?15:16);
        unsigned expected=above?(minor?tones(target+2,3,6,10):tones(target+2,3,7,10)):tones(target-1,3,6,minor?9:10);
        assert(played(instance,target)==expected);release(instance,target);API.destroy_instance(instance);
    }
}
static void held_and_recorded(void){
    Inst *instance=setup();instance->player.config.mode=1;API.set_param(instance,"motion_gesture_1","Touch");
    assert(played(instance,60)==tones(62,3,7,10));release(instance,60);
    API.set_param(instance,"motion_gesture_1","Up,500");assert(!instance->motion.enclosure);
    assert(played(instance,60)==tones(60,4,7,11));release(instance,60);
    instance->movy_playback=1;instance->recorded_action_valid[60]=1;
    instance->recorded_actions[60][HB_MOTION_LANES]=2u<<4;
    assert(played(instance,60)==tones(55,4,7,10));release(instance,60);
    assert(!instance->movy_pad_shift[60]);
    instance->movy_playback=0;
    char state[16384],value[64];API.set_param(instance,"chromatic_quality","Auto Dim7 / Min7b5");API.get_param(instance,"state",state,sizeof(state));
    API.destroy_instance(instance);instance=API.create_instance("",0);API.set_param(instance,"state",state);
    API.get_param(instance,"chromatic_quality",value,sizeof(value));assert(!strcmp(value,"Auto Dim7 / Min7b5"));
    assert(instance->motion.lanes[0].operation==HB_MO_SECONDARY_II&&instance->motion.lanes[1].operation==HB_MO_SECONDARY_V);
    API.destroy_instance(instance);
}

/* All allowed pairs and all six VI/upper/lower touch orders use source gestures. */
static void linked_sequences(void){
    const int permutations[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    for(int upper=0;upper<2;upper++)for(int lower=0;lower<2;lower++)
    for(int triple=0;triple<2;triple++)for(int order=0;order<(triple?6:2);order++)
    for(int minor=0;minor<2;minor++)for(int mode=0;mode<3;mode++){
        Inst *instance=setup();instance->player.config.mode=mode;instance->player.config.chromatic_quality=6;
        /* E minor makes Scale Above Fmaj7 distinct from Secondary II F# halfdim. */
        int target=minor?64:60,lanes[3]={3,upper?15:1,lower?16:2};
        int count=triple?3:2,steps[3];
        for(int index=0;index<count;index++){
            steps[index]=triple?permutations[order][index]:1+(index^order);
            tap(instance,lanes[steps[index]]);
        }
        for(int step=0;step<=count;step++){
            int role=step==count?-1:steps[step];int note=target;unsigned expected=tones(target,minor?3:4,7,minor?10:11);
            if(role==0){note=target-(minor?4:3);expected=tones(note,minor?4:3,7,minor?11:10);}
            if(role==1){note=target+(upper&&minor?1:2);expected=upper&&minor?tones(note,4,7,11):tones(note,3,minor?6:7,10);}
            if(role==2){note=target-(lower?1:5);expected=lower?tones(note,3,6,minor?9:10):tones(note,4,7,10);}
            if(role==-1&&!triple&&upper&&lower&&mode==2)expected=tones(60,4,7,11);
            if(!mode)expected=1u<<mod12(note);
            unsigned actual=played(instance,target);
            if(actual!=expected)fprintf(stderr,"linked u%d l%d triple%d order%d minor%d mode%d step%d got%x expected%x\n",upper,lower,triple,order,minor,mode,step,actual,expected);
            assert(actual==expected);release(instance,target);assert(instance->player.config.mode==mode);
        }
        assert(!instance->motion.enclosure);API.destroy_instance(instance);
    }
}
static void source_sequence_lifecycle(void){
    Inst *instance=setup();instance->player.config.mode=1;
    tap(instance,3);tap(instance,1);tap(instance,2);
    assert(!strcmp(hb_mo_pending_status(&instance->motion),"VI > II > V > Target"));
    hb_mo_input(&instance->motion,60,0,.02);
    assert(hb_mo_source_secondary(&instance->motion)==4);
    hb_mo_input(&instance->motion,64,.001,.02);
    assert(hb_mo_source_secondary(&instance->motion)==4&&instance->motion.enclosure_step==1);
    hb_mo_input(&instance->motion,60,1,.02);assert(hb_mo_source_secondary(&instance->motion)==1);
    hb_mo_input(&instance->motion,60,2,.02);assert(hb_mo_source_secondary(&instance->motion)==2);
    hb_mo_input(&instance->motion,60,3,.02);assert(hb_mo_source_secondary(&instance->motion)==3&&!instance->motion.enclosure);
    /* Real recorded action parser preserves VI alongside piano identity. */
    char message[512];int used=snprintf(message,sizeof(message),"60");
    for(int lane=0;lane<HB_MOTION_LANES;lane++)used+=snprintf(message+used,sizeof(message)-(size_t)used,",0");
    snprintf(message+used,sizeof(message)-(size_t)used,",68");
    API.set_param(instance,"hb_movy_actions",message);
    assert(instance->recorded_action_valid[60]&&instance->recorded_actions[60][16]==68);
    API.destroy_instance(instance);
}

static void pending_policies(void){
    Inst *instance=setup();
    tap(instance,3);tap(instance,1);tap(instance,2);tap(instance,3);
    assert(instance->motion.enclosure==5); /* removing VI retains II-before-V */
    hb_mo_end_lanes(&instance->motion,0xffff);
    for(int lane=0;lane<3;lane++)instance->motion.lanes[lane].auto_off=1;
    tap(instance,3);tap(instance,1);tap(instance,2);
    for(int step=0;step<4;step++)hb_mo_input(&instance->motion,60,step,.02);
    assert(instance->motion.enclosure==13&&instance->motion.enclosure_step==0);
    hb_mo_end_lanes(&instance->motion,7);
    assert(!instance->motion.enclosure&&!hb_mo_pending_lanes(&instance->motion));
    API.set_param(instance,"motion_gesture_3","Touch");
    unsigned long long captured[17];hb_mo_capture(&instance->motion,0,0,60,captured);
    assert(captured[16]==64);
    API.set_param(instance,"motion_gesture_3","Up,500");
    assert(!instance->motion.enclosure);
    API.destroy_instance(instance);
}

static void parent_scale_cadences(void){
    const char *scales[]={"Natural Minor","Harmonic Minor","Melodic Minor","Dorian"};
    const int sixths[]={8,8,9,9},sevenths[]={10,11,11,10};
    for(int family=0;family<4;family++)for(int mode=0;mode<3;mode++)for(int transpose=0;transpose<=2;transpose+=2){
        Inst *instance=setup();API.set_param(instance,"follower_scale",scales[family]);
        uint8_t notes[4]={60,63,67,(uint8_t)(60+sevenths[family])};
        hb_commit_observed_harmony(hb_infer_harmony(notes,4));
        hb_set_master_transpose(transpose);instance->player.config.mode=mode;instance->player.config.chromatic_quality=6;
        int target=60+transpose;
        const unsigned expected[]={
            (1u<<mod12(target+sixths[family]))|(1u<<mod12(target))|(1u<<mod12(target+3))|(1u<<mod12(target+7)),
            (1u<<mod12(target+2))|(1u<<mod12(target+5))|(1u<<mod12(target+sixths[family]))|(1u<<mod12(target)),
            tones(target-5,4,7,10),
            tones(target,3,7,sevenths[family])
        };
        tap(instance,3);tap(instance,1);tap(instance,2);
        for(int step=0;step<4;step++){
            unsigned wanted=mode?expected[step]:1u<<mod12(target+(step==0?sixths[family]-12:step==1?2:step==2?-5:0));
            unsigned actual=played(instance,60);
            if(actual!=wanted)fprintf(stderr,"parent %s mode%d transpose%d step%d got%x expected%x\n",scales[family],mode,transpose,step,actual,wanted);
            assert(actual==wanted);release(instance,60);
        }
        tap(instance,16);
        assert(played(instance,60)==(mode?tones(target-1,3,6,sixths[family]==9?10:9):1u<<mod12(target-1)));
        release(instance,60);API.destroy_instance(instance);
    }
}

static void selected_parent_context(void){
    Inst *instance=setup();instance->player.config.mode=1;
    API.set_param(instance,"follower_scale","Melodic Minor");
    uint8_t major[4]={60,64,67,71},minor[4]={60,63,67,71};
    position=.25;g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=hb_infer_harmony(major,4)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=hb_infer_harmony(minor,4)};
    g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    tap(instance,3);assert(played(instance,60)==tones(57,3,7,10));release(instance,60);
    API.set_param(instance,"motion_gesture_5","Touch");
    tap(instance,3);assert(played(instance,60)==tones(57,3,6,10));release(instance,60);
    API.set_param(instance,"motion_gesture_5","Up,500");
    API.destroy_instance(instance);
}
int main(void){selected_parent_context();parent_scale_cadences();pending_policies();linked_sequences();source_sequence_lifecycle();cadence_orders();automatic_approaches();held_and_recorded();puts("secondary chords: major/minor cadences, both orders, single-note mode, top note, pitch approaches, hold, recording and persistence pass");return 0;}
