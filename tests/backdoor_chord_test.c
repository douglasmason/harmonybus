#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static void backdoor_cadences(void){
    for(int mode=0;mode<3;mode++)for(int reverse=0;reverse<2;reverse++)for(int transpose=0;transpose<=2;transpose+=2){
        Inst *instance=setup();instance->player.config.mode=mode;hb_set_master_transpose(transpose);
        API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Backdoor II");
        API.set_param(instance,"motion_lane","2");API.set_param(instance,"motion_operation","Backdoor V");
        assert(instance->motion.lanes[0].operation==HB_MO_BACKDOOR_II);
        assert(instance->motion.lanes[1].operation==HB_MO_BACKDOOR_V);
        tap(instance,reverse?2:1);tap(instance,reverse?1:2);
        for(int step=0;step<3;step++){
            int target=60+transpose,role=step==2?0:((step==0)!=reverse)?5:6;
            int note=target+(role==5?5:role==6?-2:0);
            unsigned expected=mode?role==5?tones(note,3,7,10):role==6?tones(note,4,7,10):tones(note,4,7,11):1u<<mod12(note);
            unsigned actual=played(instance,60);
            if(actual!=expected)fprintf(stderr,"backdoor mode%d reverse%d transpose%d step%d got%x expected%x\n",mode,reverse,transpose,step,actual,expected);
            assert(actual==expected);release(instance,60);
        }
        assert(!instance->motion.enclosure);API.destroy_instance(instance);
    }
}
static void chromatic_target(void){
    for(int mode=0;mode<3;mode++)for(int top=0;top<2;top++)for(int transpose=0;transpose<=2;transpose+=2){
        Inst *instance=setup();instance->player.config.mode=mode;instance->player.config.inversion=top?8:0;instance->chromatic_map=1;
        hb_set_master_transpose(transpose);tap(instance,1);tap(instance,2);
        /* Eb is the chromatic approach pad to E, with None travel. */
        for(int step=0;step<3;step++){
            int note=(step==0?65:step==1?58:63)+transpose;
            unsigned expected=mode?step==0?tones(note,3,7,10):step==1?tones(note,4,7,10):tones(note,3,6,9):1u<<mod12(note);
            unsigned actual=played(instance,63);
            if(actual!=expected)fprintf(stderr,"nested mode%d top%d transpose%d step%d got%x expected%x\n",mode,top,transpose,step,actual,expected);
            assert(actual==expected);release(instance,63);
        }
        API.destroy_instance(instance);
    }
}
static void backdoor_recording(void){
    Inst *instance=setup();instance->player.config.mode=1;instance->movy_playback=1;
    for(int role=5;role<=6;role++){
        char message[512];int used=snprintf(message,sizeof(message),"60");
        for(int lane=0;lane<16;lane++)used+=snprintf(message+used,sizeof(message)-(size_t)used,",0");
        snprintf(message+used,sizeof(message)-(size_t)used,",%d",role<<4);
        API.set_param(instance,"hb_movy_actions",message);
        assert(instance->recorded_action_valid[60]);
        assert(played(instance,60)==(role==5?tones(65,3,7,10):tones(58,4,7,10)));release(instance,60);
    }
    instance->movy_playback=0;
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Backdoor II");
    API.set_param(instance,"motion_gesture_1","Touch,1000");API.set_param(instance,"motion_gesture_1","Up,50,1050");
    API.set_param(instance,"motion_gesture_1","Touch,1100");API.set_param(instance,"motion_gesture_1","Up,50,1150");
    assert(instance->motion.gesture_persistent&1);
    for(int repeat=0;repeat<2;repeat++){assert(played(instance,60)==tones(65,3,7,10));release(instance,60);}
    API.set_param(instance,"motion_gesture_1","Touch,2000");API.set_param(instance,"motion_gesture_1","Up,50,2050");
    assert(!instance->motion.enclosure);
    char state[16384];API.get_param(instance,"state",state,sizeof(state));API.destroy_instance(instance);
    instance=API.create_instance("",0);API.set_param(instance,"state",state);assert(instance->motion.lanes[0].operation==HB_MO_BACKDOOR_II);API.destroy_instance(instance);
}
static void dominant_output_colors(void){
    for(int choice=0;choice<4;choice++){
        Inst *instance=setup();instance->player.config.mode=1;
        const char *names[]={"Off","Harmonic Minor","Melodic Minor","Altered V"};
        API.set_param(instance,"dominant_scale",names[choice]);API.set_param(instance,"chord_form","Ninth");
        tap(instance,2);
        /* V of C keeps G-B-D-F; only the ninth changes to Ab for HM/Altered. */
        unsigned expected=tones(55,4,7,10)|(1u<<(choice==1||choice==3?8:9));
        assert(played(instance,60)==expected);release(instance,60);
        /* Dominant colors do not alter the target or the global input scale. */
        assert(hb_follower_input_scale(instance,0)==hb_explicit_scale_mask(0,1));
        assert(played(instance,60)==(tones(60,4,7,11)|(1u<<2)));release(instance,60);
        API.set_param(instance,"chord_form","Seventh");API.set_param(instance,"chromatic_quality","Auto Dim7 / Min7b5");
        tap(instance,16);
        expected=tones(59,3,6,choice==1||choice==3?9:10);
        assert(played(instance,60)==expected);release(instance,60);
        /* Existing follower override preserves all actual dominant chord tones. */
        uint8_t dominant[]={55,59,62,65};hb_harmony_t chord=hb_infer_harmony(dominant,4);
        unsigned collection=hb_follower_scale_target(instance,chord).pitch_mask;
        assert((collection&tones(55,4,7,10))==tones(55,4,7,10));
        if(choice==1||choice==3){assert(collection&(1u<<8));assert(!(collection&(1u<<9)));}
        API.destroy_instance(instance);
    }
}
static void preview_matches_output(void){
    for(int kind=0;kind<3;kind++){
        Inst *instance=setup();instance->player.config.mode=1;instance->chromatic_map=1;
        if(kind){API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation",kind==1?"Backdoor II":"Backdoor V");}
        tap(instance,1);int input=kind?60:63;
        Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
        unsigned long long low=0,high=0;hb_pad_render_mask(preview,instance,bus_read(),input,0,&low,&high);
        unsigned expected=0;
        for(int pitch=0;pitch<128;pitch++)if(pitch<64?(low&(1ULL<<pitch)):(high&(1ULL<<(pitch-64))))expected|=1u<<mod12(pitch);
        assert(played(instance,input)==expected);release(instance,input);free(preview);API.destroy_instance(instance);
    }
}
int main(void){preview_matches_output();dominant_output_colors();backdoor_cadences();chromatic_target();backdoor_recording();puts("backdoor: both orders, raw/chord/top-note, transposition, chromatic targets, recording and persistent gestures pass");}
