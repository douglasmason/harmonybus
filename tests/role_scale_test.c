#define HB_PARALLEL_FIXTURE
#include "parallel_harmony_test.c"
#undef main
static void inheritance(void){
    Inst *first=fixture(),*second=API.create_instance("",0);API.set_param(second,"role","Follower");
    API.set_param(first,"follower_default_chord_form","Ninth");
    char value[256];API.get_param(second,"chord_form",value,sizeof(value));assert(!strcmp(value,"Ninth"));
    API.set_param(second,"chord_form","Seventh");API.set_param(first,"follower_default_chord_form","Triad");
    API.get_param(second,"chord_form",value,sizeof(value));assert(!strcmp(value,"Seventh"));
    API.get_param(first,"chord_form",value,sizeof(value));assert(!strcmp(value,"Triad"));
    API.get_param(second,"chord_scope",value,sizeof(value));assert(strstr(value,"Form"));
    API.set_param(second,"chord_form","Role Default");API.get_param(second,"chord_form",value,sizeof(value));assert(!strcmp(value,"Triad"));
    API.set_param(first,"conductor_default_chord_form","Eleventh");API.set_param(second,"role","Conductor");
    API.get_param(second,"chord_form",value,sizeof(value));assert(!strcmp(value,"Eleventh"));
    API.set_param(second,"chord_inversion","First");API.set_param(second,"gap_scale","Strict Local");
    char saved[16384];API.get_param(second,"state",saved,sizeof(saved));
    API.destroy_instance(first);API.destroy_instance(second);second=API.create_instance("",0);API.set_param(second,"state",saved);
    API.get_param(second,"chord_form",value,sizeof(value));assert(!strcmp(value,"Eleventh"));
    API.get_param(second,"chord_inversion",value,sizeof(value));assert(!strcmp(value,"First"));
    API.get_param(second,"gap_scale",value,sizeof(value));assert(!strcmp(value,"Strict Local"));
    API.set_param(second,"chord_reset_overrides","Reset");API.get_param(second,"gap_scale",value,sizeof(value));assert(!strcmp(value,"Parent"));
    API.destroy_instance(second);
}
static void collections(void){
    Inst *instance=fixture();instance->content_map=1;
    API.set_param(instance,"gap_scale","Strict Local");
    for(int root=0;root<12;root++){
        hb_harmony_t harmony=extended(root,0);
        assert(hb_follower_scale_target(instance,harmony).pitch_mask==hb_explicit_scale_mask(root,3));
    }
    hb_harmony_t dminor=extended(2,0),gdom=extended(7,2),cmajor=extended(0,1);
    g_bus.next_model_locked=1;g_bus.next_model_count=3;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=dminor};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=gdom};
    g_bus.next_model[2]=(hb_loop_harmony_event_t){.phase=8,.harmony=cmajor};
    API.set_param(instance,"gap_scale","Auto Local");API.set_param(instance,"track_dominant_scale","Harmonic Minor");
    assert(hb_follower_scale_target(instance,dminor).pitch_mask==hb_explicit_scale_mask(0,1));
    /* A dominant override colors gaps while explicit G9's A remains present. */
    unsigned gaps=hb_follower_scale_target(instance,gdom).pitch_mask;
    assert((gaps&hb_harmony_chord_mask(gdom))==hb_harmony_chord_mask(gdom));
    hb_harmony_t ddom=extended(2,2);ddom.intent_kind=2;ddom.intent_target=7;ddom.intent_minor=0;
    assert(hb_context_destination(instance,ddom,&(int){0})==7);
    API.set_param(instance,"gap_scale","Strict Local");API.set_param(instance,"local_minor","Aeolian");
    assert(hb_follower_scale_target(instance,dminor).pitch_mask==hb_explicit_scale_mask(2,2));
    assert(hb_follower_input_scale(instance,0)==hb_explicit_scale_mask(0,1));
    API.destroy_instance(instance);
}
static void context_scopes(void){
    Inst *instance=fixture();
    hb_harmony_t dminor=extended(2,0),gdom=extended(7,2),cminor=extended(0,0);
    g_bus.next_model_locked=1;g_bus.next_model_count=3;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=dminor};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=gdom};
    g_bus.next_model[2]=(hb_loop_harmony_event_t){.phase=8,.harmony=cminor};
    API.set_param(instance,"gap_scale","Auto Local");
    API.set_param(instance,"scale_context","Current Harm");
    int minor=-1;char value[64],saved[16384];
    assert(hb_context_destination(instance,dminor,&minor)==-1);
    assert(hb_local_output_scale(instance,dminor)==hb_local_recipe(instance,dminor));
    API.set_param(instance,"scale_context","Current + Next");
    assert(hb_context_destination(instance,dminor,&minor)==0&&minor==0);
    /* This mode cannot use the third chord to choose the minor destination. */
    API.set_param(instance,"scale_context","Full Loop");
    assert(hb_context_destination(instance,dminor,&minor)==0&&minor==1);
    for(int scope=0;scope<3;scope++){
        API.set_param(instance,"scale_context",HB_CONTEXT_OPTIONS[scope]);
        API.get_param(instance,"state",saved,sizeof(saved));
        API.set_param(instance,"scale_context",HB_CONTEXT_OPTIONS[(scope+1)%3]);
        API.set_param(instance,"state",saved);
        API.get_param(instance,"scale_context",value,sizeof(value));
        assert(!strcmp(value,HB_CONTEXT_OPTIONS[scope]));
    }
    API.set_param(instance,"scale_context","Role Default");
    API.set_param(instance,"follower_default_scale_context","Current Harm");
    API.get_param(instance,"scale_context",value,sizeof(value));assert(!strcmp(value,"Current Harm"));
    API.set_param(instance,"follower_default_scale_context","Current + Next");
    API.get_param(instance,"scale_context",value,sizeof(value));assert(!strcmp(value,"Current + Next"));
    API.set_param(instance,"follower_default_scale_context","Full Loop");
    API.get_param(instance,"scale_context",value,sizeof(value));assert(!strcmp(value,"Full Loop"));
    API.set_param(instance,"scale_context","Current Harm");
    dminor.intent_kind=1;dminor.intent_target=9;dminor.intent_minor=1;
    g_bus.next_model_locked=0;
    assert(hb_context_destination(instance,dminor,&minor)==9&&minor==1);
    API.destroy_instance(instance);
}
static void display_snapshots(void){
    Inst *instance=fixture();
    static const char *const names[]={"next_harm_snapshot","grid_timing_snapshot","follower_root_snapshot"};
    static const char *const keys[3][8]={
        {"next_lookahead","next_anti_buffer_ms","boundary_buffer_ms","next_model","next_shift","next_loop_length","next_position","next_harmony"},
        {"chord_timing","anticipation","chord_grid_status","timing_position","timing_last_at","timing_next_at","timing_last_chord","timing_next_chord"},
        {"follower_root_policy","follower_explicit_root","follower_scale","inferred_root","used_root","used_scale"}
    };
    for(int page=0;page<3;page++){
        char snapshot[2048],expected[192];
        API.get_param(instance,names[page],snapshot,sizeof(snapshot));
        assert(!strncmp(snapshot,"dp1|",4));char *field=snapshot+4;
        for(int slot=0;slot<(page==2?6:8);slot++){
            char *end=strchr(field,'|');assert((slot<(page==2?5:7))==(end!=0));if(end)*end=0;
            API.get_param(instance,keys[page][slot],expected,sizeof(expected));
            assert(!strcmp(field,*expected?expected:"--"));field=end?end+1:field+strlen(field);
        }
    }
    API.destroy_instance(instance);
}

static void functional_families(void){
    Inst *instance=fixture();
    API.set_param(instance,"dominant_scale","Harmonic Major");
    API.set_param(instance,"dominant_minor_scale","Harmonic Minor");
    for(int target=0;target<12;target++){
        unsigned major=hb_explicit_scale_mask(target,1),minor=hb_explicit_scale_mask(target,2);
        hb_cadence_step ii={.kind=HB_CAD_DEGREE,.degree=2};
        hb_cadence_step dominant={.kind=HB_CAD_DOMINANT};
        hb_cadence_result pre=hb_resolve_cadence(instance,&ii,60+target,major);
        hb_cadence_result five=hb_resolve_cadence(instance,&dominant,60+target,major);
        assert(pre.root==62+target&&pre.scale==hb_explicit_scale_mask(target,19));
        assert(five.root==55+target&&five.scale==pre.scale&&!five.minor);
        pre=hb_resolve_cadence(instance,&ii,60+target,minor);
        five=hb_resolve_cadence(instance,&dominant,60+target,minor);
        assert(pre.scale==hb_explicit_scale_mask(target,8)&&five.scale==pre.scale&&five.minor);
        /* Dominant preparation and V share the selected Altered collection. */
        API.set_param(instance,"dominant_scale","Altered V");
        pre=hb_resolve_cadence(instance,&ii,60+target,major);
        five=hb_resolve_cadence(instance,&dominant,60+target,major);
        assert(pre.scale==five.scale&&five.scale==hb_explicit_scale_mask(mod12(target+7),15));
        API.set_param(instance,"dominant_scale","Harmonic Major");
    }
    /* The override must not change the identity of the major resolution target. */
    API.set_param(instance,"dominant_scale","Harmonic Minor");
    hb_cp_config config=instance->player.config;
    hb_approach_result result=hb_resolve_chord_approach(instance,60,hb_explicit_scale_mask(0,1),config,chord(0,0,0),2,0,0,0);
    assert(result.intent_target==0&&!result.intent_minor&&result.scale==hb_explicit_scale_mask(0,8));
    /* Existing explicit chord tones win over a family substitution. */
    API.set_param(instance,"gap_scale","Auto Local");
    API.set_param(instance,"dominant_scale","Harmonic Major");
    hb_harmony_t dminor=chord(2,1,1);dminor.intent_kind=1;dminor.intent_target=0;dminor.intent_minor=0;
    assert(hb_local_output_scale(instance,dminor)&(1u<<9));
    /* Preferences survive a state save independently. */
    char saved[16384],value[64];API.get_param(instance,"state",saved,sizeof(saved));
    API.destroy_instance(instance);instance=API.create_instance("",0);API.set_param(instance,"state",saved);
    API.get_param(instance,"track_dominant_scale",value,sizeof(value));assert(!strcmp(value,"Harmonic Major"));
    API.get_param(instance,"track_dominant_minor_scale",value,sizeof(value));assert(!strcmp(value,"Harmonic Minor"));
    API.destroy_instance(instance);
}

int main(void){functional_families();inheritance();collections();context_scopes();display_snapshots();puts("role defaults, visible overrides, migration, local parallel scales and contextual ii-V selection pass");return 0;}
