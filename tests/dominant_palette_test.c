#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"

static unsigned expected_notes(int root,const int *intervals,int count){
    unsigned mask=0;
    for(int index=0;index<count;index++)mask|=1u<<mod12(root+intervals[index]);
    return mask;
}
static void palettes(void){
    static const int intervals[5][8]={{0,2,4,5,7,9,10},{0,2,4,6,7,9,10},
        {0,1,3,4,6,8,10},{0,1,3,4,6,7,9,10},{0,2,4,6,8,10}};
    static const int counts[]={7,7,7,8,6};
    static const char *local[]={"Mixolydian","Lydian Dominant","Altered","Half-Whole Dim","Whole Tone"};
    static const char *families[]={"Major","Lydian Dominant","Altered V","Half-Whole Dim","Whole Tone"};
    Inst *instance=setup();
    for(int root=0;root<12;root++)for(int palette=0;palette<5;palette++){
        unsigned expected=expected_notes(root,intervals[palette],counts[palette]);
        assert(hb_dominant_palette(root,palette)==expected);
        for(int mode=1;mode<=2;mode++){
            hb_cp_config config;hb_cp_defaults(&config);config.mode=mode;config.size=9;
            int notes[12];unsigned semantic=0;
            unsigned core=tones(root,4,palette==2?6:palette==4?8:7,10);
            int count=hb_cp_voice_semantic(config,60+root,root,core,expected,notes,&semantic);
            unsigned voiced=0;for(int index=0;index<count;index++)voiced|=1u<<mod12(notes[index]);
            unsigned expected_voicing=expected;
            if(palette==2||palette==3)expected_voicing&=~(1u<<mod12(root+3));
            assert(voiced==expected_voicing&&semantic==expected_voicing);
        }
        int target=mod12(root+5);
        for(int minor=0;minor<2;minor++){
            API.set_param(instance,minor?"track_dominant_minor_scale":"track_dominant_scale",families[palette]);
            for(int kind=0;kind<3;kind++)
                assert(hb_function_family(instance,target,hb_explicit_scale_mask(target,minor?2:1),minor,kind)==expected);
        }
        hb_harmony_t harmony={.valid=1,.root_pc=root,.chord_index=HB_HARMONY_EXPLICIT_TONES,
            .pitch_mask=tones(root,4,palette==2||palette==4?8:7,10)};
        API.set_param(instance,"local_dominant",local[palette]);
        API.set_param(instance,"gap_scale","Strict Local");
        assert(hb_local_output_scale(instance,harmony)==expected);
        assert(hb_follower_scale_target(instance,harmony).pitch_mask==expected);
        API.set_param(instance,"gap_scale","Auto Local");
        g_bus.next_model_locked=0;
        assert(hb_local_output_scale(instance,harmony)==expected);
        /* Explicit intent takes precedence over the local fallback. */
        harmony.intent_kind=2;harmony.intent_target=target;harmony.intent_scale=hb_explicit_scale_mask(target,1);
        API.set_param(instance,"track_dominant_scale","Half-Whole Dim");
        unsigned resolved=hb_local_output_scale(instance,harmony);
        assert((resolved&harmony.pitch_mask)==harmony.pitch_mask);
        assert(resolved&(1u<<mod12(root+9)));
        /* Whole tone plus a voiced natural fifth must keep #11 and b13. */
        if(palette==4){
            harmony.intent_kind=0;harmony.pitch_mask=tones(root,4,7,10);
            assert(hb_local_output_scale(instance,harmony)==(expected|harmony.pitch_mask));
        }
    }
    API.destroy_instance(instance);
}
static void persistence(void){
    Inst *instance=setup(),*other=API.create_instance("",0);
    API.set_param(other,"role","Follower");
    API.set_param(instance,"follower_default_local_dominant","Lydian Dominant");
    assert(hb_policy_value(other,HB_P_LOCAL_DOMINANT)==1);
    API.set_param(other,"local_dominant","Whole Tone");
    API.set_param(other,"track_dominant_scale","Half-Whole Dim");
    API.set_param(other,"track_dominant_minor_scale","Whole Tone");
    API.set_param(other,"dominant_color_family","Lydian Dominant");
    char state[32768];API.get_param(other,"state",state,sizeof(state));
    API.set_param(other,"local_dominant","Mixolydian");
    API.set_param(other,"state",state);
    assert(hb_policy_value(other,HB_P_LOCAL_DOMINANT)==4);
    assert(hb_policy_value(other,HB_P_DOMINANT)==9);
    assert(hb_policy_value(other,HB_P_DOMINANT_MINOR)==10);
    assert(other->dominant_color_family==8);
    API.set_param(other,"local_dominant","Follow Role");
    assert(hb_policy_value(other,HB_P_LOCAL_DOMINANT)==1);
    /* Both older role-state formats retain every old field and default new ones. */
    for(int stride=13;stride<=14;stride++){
        int used=snprintf(state,sizeof(state),";rp%d,%u",stride==13?1:2,1u<<HB_P_DOMINANT);
        for(int group=0;group<3;group++)for(int field=0;field<stride;field++)
            used+=snprintf(state+used,sizeof(state)-(size_t)used,",%d",field==HB_P_DOMINANT?3:HB_POLICY_DEFAULTS[field]);
        g_role_restored=0;hb_role_restore(other,state);
        assert(hb_policy_value(other,HB_P_DOMINANT)==3);
        assert(hb_policy_value(other,HB_P_LOCAL_DOMINANT)==0);
        assert(hb_policy_value(other,HB_P_DOMINANT_MINOR)==(stride==13?3:6));
    }
    API.destroy_instance(other);API.destroy_instance(instance);
}
int main(void){palettes();persistence();puts("Dominant palettes: all roots, local/context precedence, explicit tones and old/new states pass");return 0;}
