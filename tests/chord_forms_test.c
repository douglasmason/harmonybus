/* Chord identity, onset ownership and section-form regressions. */
#define main chord_player_legacy_main
#include "chord_player_test.c"
#undef main
static void shell_forms(void){
    hb_cp_config config;hb_cp_defaults(&config);config.mode=1;
    const unsigned expected[]={0x811,0x815,0x215,0x810,0x814};
    for(int minor=0;minor<2;minor++)for(int form=12;form<HB_CP_FORMS;form++){
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
        for(int form=12;form<HB_CP_FORMS;form++)for(int root=0;root<12;root++){
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
int main(void){shell_forms();generated_semantics_all_scales();next_onset_and_operations();puts("chord forms: semantic shells, rootless identity, unchanged held notes and cycle overrides pass");return 0;}
