/* Chord identity, onset ownership and section-form regressions. */
#define main chord_player_legacy_main
#include "chord_player_test.c"
#undef main
static void shell_forms(void){
    hb_cp_config config;hb_cp_defaults(&config);config.mode=1;
    const unsigned expected[]={0x811,0x815,0x215,0x810,0x814};
    for(int minor=0;minor<2;minor++)for(int form=12;form<HB_CP_FOLLOW_DETECTED;form++){
        config.size=form;unsigned semantic=0,actual=0;int notes[12];
        int count=hb_cp_voice_semantic(config,60,0,0,minor?0x5ad:0xab5,notes,&semantic);
        for(int index=0;index<count;index++)actual|=1u<<(notes[index]%12);
        unsigned wanted=expected[form-12];
        if(minor){if(wanted&0x10)wanted=(wanted&~0x10)|8;if(wanted&0x800)wanted=(wanted&~0x800)|0x400;if(wanted&0x200)wanted=(wanted&~0x200)|0x100;}
        assert(actual==wanted);
        assert(semantic==(wanted|1|0x80|(minor?8:0x10)));
    }
    Inst *instance=fixture();API.set_param(instance,"role","Conductor");
    API.set_param(instance,"chord_mode","Scale Degree");API.set_param(instance,"chord_form","Rootless 9");
    midi(instance,1,60);assert(advance(instance,0,64)==3);
    hb_harmony_t harmony=bus_read();assert(harmony.root_pc==0);
    assert((hb_harmony_chord_mask(harmony)&0x895)==0x895);
    uint8_t shown[128];assert(local_conductor_notes(instance,shown,128)==3);
    assert(!instance->held_count[60]&&!instance->held_count[67]);
    API.destroy_instance(instance);
}
static void generated_semantics_all_scales(void){
    for(int scale_index=1;scale_index<16;scale_index++){
        Inst *instance=fixture();API.set_param(instance,"role","Conductor");
        API.set_param(instance,"follower_scale",FOLLOWER_SCALE_OPTS[scale_index]);
        API.set_param(instance,"chord_mode","Scale Degree");
        unsigned scale=hb_follower_input_scale(instance,0);
        /* Dynamic Follow Detected is covered separately with a source harmony. */
        for(int form=12;form<HB_CP_FOLLOW_DETECTED;form++)for(int root=0;root<12;root++){
            API.set_param(instance,"chord_form",CP_CHORD_FORM[form]);
            unsigned expected=0;int notes[12];
            hb_cp_voice_semantic(instance->player.config,60+root,root,0,scale,notes,&expected);
            midi(instance,1,60+root);advance(instance,0,64);
            hb_harmony_t harmony=bus_read();
            assert(harmony.root_pc==root);assert(hb_harmony_chord_mask(harmony)==expected);
            midi(instance,0,60+root);advance(instance,0,64);
        }
        API.destroy_instance(instance);
    }
}
static void next_onset_and_operations(void){
    Inst *instance=fixture();API.set_param(instance,"chord_mode","Scale Degree");
    midi(instance,1,60);assert(advance(instance,0,64)==3);
    API.set_param(instance,"chord_form","Ninth");assert(advance(instance,0,64)==0);
    midi(instance,0,60);assert(advance(instance,0,64)==3);
    midi(instance,1,60);assert(advance(instance,0,64)==5);
    midi(instance,0,60);assert(advance(instance,0,64)==5);
    API.set_param(instance,"motion_operation","Chord Form");
    API.set_param(instance,"motion_amount","Shell 7");
    API.set_param(instance,"motion_every","2");API.set_param(instance,"motion_from","2");
    position=0;midi(instance,1,60);assert(advance(instance,0,64)==5);
    midi(instance,0,60);advance(instance,0,64);
    position=4;midi(instance,1,60);assert(advance(instance,0,64)==3);
    g_movy_present=1;g_movy_tick=384;g_movy_ppqn=96;g_movy_running=1;
    assert(hb_motion_condition_position()==4.0);
    assert(hb_chord_config_at(instance,60,4.0,hb_motion_condition_position()).size==12);
    g_movy_present=0;
    char value[64];API.get_param(instance,"motion_amount",value,sizeof(value));assert(!strcmp(value,"Shell 7"));
    API.destroy_instance(instance);
}
static void follow_detected_forms(void){
    hb_cp_config config;hb_cp_defaults(&config);config.mode=1;config.size=HB_CP_FOLLOW_DETECTED;config.inversion=1;
    const unsigned detected[]={0x91,0x891,0x895,0x291,0x293,0x85,0xa1,0x249};
    const unsigned roles[]={0x15,0x55,0x57,0x35,0x37,0x13,0x19,0x55};
    for(int source_root=0;source_root<12;source_root++)for(int form=0;form<8;form++){
        unsigned chord_mask=hb_transpose_mask(detected[form],source_root);
        assert(hb_cp_detected_roles(source_root,chord_mask)==roles[form]);
        for(int target=60;target<72;target++){
            int actual[12],baseline[12];
            int count=hb_cp_voice(config,target,source_root,chord_mask,0xab5,actual);
            hb_cp_config explicit=config;explicit.size=2;
            if(form==1||form==7)explicit.size=3;
            if(form==2)explicit.size=4;
            if(form==3)explicit.size=6;
            if(form==4)explicit.size=7;
            if(form==5)explicit.size=10;
            if(form==6)explicit.size=11;
            int expected=hb_cp_voice(explicit,target,source_root,chord_mask,0xab5,baseline);
            assert(count==expected);expect_notes(actual,baseline,count);
        }
    }
    config.mode=2;
    for(int form=0;form<8;form++){
        int actual[12];unsigned rendered=0;
        int count=hb_cp_voice(config,60,0,detected[form],0xab5,actual);
        for(int index=0;index<count;index++)rendered|=1u<<mod12(actual[index]);
        assert(rendered==detected[form]);
    }
    Inst *instance=fixture();API.set_param(instance,"role","Follower");
    API.set_param(instance,"track_chord_form","Role Default");
    assert(hb_policy_value(instance,HB_P_FORM)==HB_CP_FOLLOW_DETECTED);
    char value[128];API.get_param(instance,"track_chord_form",value,sizeof(value));assert(!strcmp(value,"Follow Role"));
    API.set_param(instance,"track_chord_form","Ninth");assert(hb_policy_value(instance,HB_P_FORM)==4);
    API.set_param(instance,"conductor_default_chord_form","Sixth");assert(hb_policy_value(instance,HB_P_FORM)==4);
    API.set_param(instance,"track_chord_form","Role Default");assert(hb_policy_value(instance,HB_P_FORM)==HB_CP_FOLLOW_DETECTED);
    hb_harmony_t harmony=hb_infer_harmony((uint8_t[]){60,64,67,71},4);hb_effective_write(harmony);
    API.get_param(instance,"detected_chord_form",value,sizeof(value));assert(strstr(value,"1357"));
    API.get_param(instance,"pad_chord_form",value,sizeof(value));assert(!strcmp(value,"Follow Detected"));
    API.set_param(instance,"track_chord_form","Rootless 7");
    assert(hb_harmony_equal_effective(bus_read(),harmony));
    API.get_param(instance,"detected_chord_form",value,sizeof(value));assert(strstr(value,"1357"));
    harmony=hb_infer_harmony((uint8_t[]){60,64,67,71,74},5);hb_effective_write(harmony);
    API.get_param(instance,"detected_chord_form",value,sizeof(value));assert(strstr(value,"13579"));
    API.destroy_instance(instance);
}
static void detected_source_forms(void){
    /* Recorded evidence remains independent of the selected conductor form. */
    for(int form=0;form<4;form++){
        Inst *i=fixture();API.set_param(i,"role","Conductor");
        API.set_param(i,"chord_mode","Scale Degree");API.set_param(i,"track_chord_form","Power");
        i->movy_playback=i->movy_passthrough=1;
        static const int notes[4][4]={{60,67,-1,-1},{60,64,70,-1},{64,70,-1,-1},{60,64,67,71}};
        unsigned expected=0;
        for(int n=0;n<4&&notes[form][n]>=0;n++){expected|=1u<<mod12(notes[form][n]);midi(i,1,notes[form][n]);}
        for(int n=0;n<100;n++)advance(i,10,64);
        hb_harmony_t detected=bus_read();assert(detected.valid);
        if(hb_harmony_detected_mask(detected)!=expected)fprintf(stderr,"source form %d got %x expected %x\n",form,hb_harmony_detected_mask(detected),expected);
        assert(hb_harmony_detected_mask(detected)==expected);
        assert(hb_pad_chord_mask(i,detected)==expected);
        API.destroy_instance(i);
    }
    /* Generated forms retain richer harmonic identity but report their form. */
    Inst *i=fixture();API.set_param(i,"role","Conductor");API.set_param(i,"chord_mode","Scale Degree");
    const char *forms[]={"Triad","Power","Shell 7","Rootless 7"};
    unsigned masks[]={0x91,0x81,0x811,0x810};
    for(int f=0;f<4;f++){
        API.set_param(i,"track_chord_form",forms[f]);midi(i,1,60);advance(i,0,64);
        hb_harmony_t detected=bus_read();
        assert(hb_harmony_detected_mask(detected)==masks[f]);
        assert(hb_harmony_chord_mask(detected)&0x10); /* Keep known major quality. */
        Inst *follower=API.create_instance("",0);API.set_param(follower,"role","Follower");
        hb_cp_config config;hb_cp_defaults(&config);config.mode=2;config.size=HB_CP_FOLLOW_DETECTED;
        hb_player_note_on_config(follower,60,0,100,&config);
        unsigned sounded=0;for(int n=0;n<follower->player.keys[0].count;n++)sounded|=1u<<mod12(follower->player.keys[0].notes[n]);
        assert(sounded==masks[f]);API.destroy_instance(follower);

        char text[128];API.get_param(i,"detected_chord_form",text,sizeof(text));
        if(f==1)assert(strstr(text," 15")&&!strstr(text,"135"));
        hb_harmony_t shifted=hb_transpose_harmony(detected,2);
        assert(hb_harmony_detected_mask(shifted)==hb_transpose_mask(masks[f],2));
        midi(i,0,60);advance(i,0,64);
    }
    API.destroy_instance(i);
}
static void quality_beyond_form(void){
    Inst *i=fixture();API.set_param(i,"role","Conductor");
    API.set_param(i,"chord_mode","Scale Degree");API.set_param(i,"chord_quality","Min7");
    i->movy_playback=1; /* Unbaked recorded scale-degree input uses current forms. */
    for(int form=0;form<2;form++){
        API.set_param(i,"track_chord_form",form?"Seventh":"Power");
        midi(i,1,62);advance(i,0,64);
        hb_harmony_t h=bus_read();
        assert(h.root_pc==2&&hb_harmony_chord_mask(h)==0x225);
        assert(hb_harmony_detected_mask(h)==(form?0x225:0x204));
        midi(i,0,62);advance(i,0,64);
    }
    API.destroy_instance(i);
}
static void role_form_scope(void){
    Inst *i=fixture();API.set_param(i,"role","Conductor");API.set_param(i,"chord_mode","Scale Degree");
    API.set_param(i,"track_chord_form","Follow Role");API.set_param(i,"conductor_default_chord_form","Triad");
    midi(i,1,60);advance(i,1,64);assert(i->player.keys[0].count==3);midi(i,0,60);advance(i,1,64);
    API.set_param(i,"conductor_default_chord_form","Seventh");midi(i,1,60);advance(i,1,64);assert(i->player.keys[0].count==4);midi(i,0,60);advance(i,1,64);
    API.set_param(i,"track_chord_form","Ninth");API.set_param(i,"conductor_default_chord_form","Power");
    char text[64];API.get_param(i,"conductor_default_chord_form",text,sizeof(text));assert(!strcmp(text,"Power"));
    API.get_param(i,"track_chord_form",text,sizeof(text));assert(!strcmp(text,"Ninth"));
    midi(i,1,60);advance(i,1,64);assert(i->player.keys[0].count==5);midi(i,0,60);advance(i,1,64);
    API.set_param(i,"motion_lane","1");API.set_param(i,"motion_operation","Chord/Arp State");API.set_param(i,"chord_edit_target","Lane 1");
    int lane_form=i->motion.lanes[0].chord_state.size;
    API.set_param(i,"track_chord_form","Follow Role");assert(!(i->policy_overrides&1));
    API.get_param(i,"track_chord_form",text,sizeof(text));assert(!strcmp(text,"Follow Role"));
    API.set_param(i,"track_chord_form","Sixth");assert(i->policy_values[HB_P_FORM]==6&&i->motion.lanes[0].chord_state.size==lane_form);
    API.get_param(i,"conductor_default_chord_form",text,sizeof(text));assert(!strcmp(text,"Power"));API.destroy_instance(i);
}
int main(void){quality_beyond_form();detected_source_forms();role_form_scope();follow_detected_forms();shell_forms();generated_semantics_all_scales();next_onset_and_operations();puts("chord forms: semantic shells, rootless identity, unchanged held notes and cycle overrides pass");return 0;}
