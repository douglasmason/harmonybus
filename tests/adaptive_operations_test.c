/* Full production render, snapshots and versioned authored events. */
#define HB_RELATIVE_OPERATIONS_FIXTURE
#include "relative_operations_test.c"

static unsigned dim_mask(int root){return hb_transpose_mask(0x249u,root);}
static void lane_capture(void){
    for(int role=0;role<2;role++)for(int side=0;side<2;side++)for(int target_index=0;target_index<2;target_index++){
        Inst *instance=approach_fixture();API.set_param(instance,"role",role?"Follower":"Conductor");API.set_param(instance,"source_channel","1");
        API.set_param(instance,"motion_operation",side?"Smart Above":"Smart Below");
        API.set_param(instance,"motion_amount","1");API.set_param(instance,"motion_enabled","On");
        int target=target_index?68:65,expected=target_index?67:66;
        assert(hb_secondary_at(instance,target)==(target_index?12:13));
        note(instance,target,1);if(sounding_mask(instance,target)!=dim_mask(expected))fprintf(stderr,"role%d side%d target%d mode%d mask%x expected%x secondary%d\n",role,side,target,hb_cp_mode(&instance->player),sounding_mask(instance,target),dim_mask(expected),hb_secondary_at(instance,target));assert(sounding_mask(instance,target)==dim_mask(expected));
        unsigned long long captured[HB_MOTION_LANES+1];hb_capture_input_intent(instance,target,captured);
        assert(hb_mo_word_operation(captured[0])==(target_index?HB_MO_LEADING_TONE:HB_MO_UPPER_DIM));
        char metadata[65536];API.get_param(instance,"chain_params",metadata,sizeof(metadata));assert(strstr(metadata,"Smart Below")&&strstr(metadata,"Smart Above")&&strstr(metadata,"motif_operation"));
        char saved[65536],label[64];API.get_param(instance,"state",saved,sizeof(saved));
        note(instance,target,0);assert(!instance->player.sounding_count);
        API.set_param(instance,"motion_operation","Off");g_motion_settings_restored=0;API.set_param(instance,"state",saved);
        API.get_param(instance,"motion_operation",label,sizeof(label));assert(!strcmp(label,side?"Smart Above":"Smart Below"));
        char payload[4096];int used=snprintf(payload,sizeof(payload),"%d",target);
        for(int lane=0;lane<=HB_MOTION_LANES;lane++)used+=snprintf(payload+used,sizeof(payload)-used,",%llu",captured[lane]);
        API.set_param(instance,"hb_movy_actions",payload);assert(instance->recorded_action_valid[target]);
        API.set_param(instance,"motion_operation","Off");instance->motion.event_override=captured;
        assert(hb_secondary_at(instance,target)==(target_index?12:13));instance->motion.event_override=0;
        API.destroy_instance(instance);
    }
}
static void adaptive_play_and_storage(void){
    Inst *instance=approach_fixture();instance->motif.editor.arrival=0;
    hb_mt_phrase phrase={.count=1,.anchor=0};
    phrase.events[0]=(hb_mt_event){.duration=24,.count=1,.chord_mode=1,.scale=0xAB5,.selector=4096,.notes={{60,100}}};
    for(int side=0;side<2;side++)for(int index=0;index<2;index++){
        phrase.events[0].selector=4096+side;
        memset(instance->motif.events,0,sizeof(instance->motif.events));instance->motif.pending=0;
        assert(hb_mt_schedule(instance,&phrase,index?68:65,100,0,0,1,position,position,0));
        unsigned mask=0;for(int i=0;i<HB_MT_SCHEDULE;i++)if(instance->motif.events[i].used)mask|=1u<<mod12(instance->motif.events[i].pitch);
        assert(mask==dim_mask(index?67:66));
    }
    assert(hb_mt_normalize(&phrase,&g_motifs[0]));assert(g_motifs[0].body.count==1);
    char state[8192];assert(hb_mt_save(instance,state,sizeof(state),0)>0);assert(!strncmp(state,";mf3,",5));
    hb_mt_definition saved=g_motifs[0];memset(g_motifs,0,sizeof(g_motifs));g_motifs_restored=0;
    hb_mt_restore(instance,state);assert(!memcmp(&saved,&g_motifs[0],sizeof(saved)));
    const char *end=strstr(state,";mp1,");assert(end);
    for(int cut=0;cut<(int)(end-state);cut++){
        char truncated[8192];memcpy(truncated,state,cut);truncated[cut]=0;g_motifs_restored=0;
        hb_mt_restore(instance,truncated);assert(!memcmp(&saved,&g_motifs[0],sizeof(saved)));
    }
    /* Explicit authoring changes a draft step and Undo restores its selector. */
    API.set_param(instance,"motif_record","Edit");
    API.set_param(instance,"motif_operation","Smart Below");
    note(instance,65,1);note(instance,65,0);assert(instance->motif.editor.draft.events[0].selector==4096);
    API.set_param(instance,"motif_step","1");API.set_param(instance,"motif_operation","Captured");
    assert(!instance->motif.editor.draft.events[0].selector);
    API.set_param(instance,"motif_undo","Undo");assert(instance->motif.editor.draft.events[0].selector==4096);
    API.set_param(instance,"motif_record","Cancel");
    API.destroy_instance(instance);
}
static void export_polyphonic_choices(void){
    Inst *instance=approach_fixture();
    hb_mt_phrase phrase={.count=1,.anchor=0};
    phrase.events[0]=(hb_mt_event){.duration=24,.count=2,.relation=HB_MT_CHROMATIC,.chord_mode=1,.scale=0x9ad,
        .selector=4096,.notes={{65,100},{68,90}}};
    assert(hb_mt_normalize(&phrase,&g_motifs[0]));
    instance->motif_load[0]=27;instance->motif_load[1]=65;instance->motif.editor.placement=4;
    hb_mc_plan plan;int root,scale;assert(hb_ml_build(instance,&plan,&root,&scale));
    assert(plan.count==2&&plan.events[0].onset==plan.events[1].onset);
    assert(plan.events[0].event.secondary==13&&plan.events[1].event.secondary==12);
    assert(((plan.events[0].event.actions[HB_MOTION_LANES]>>HB_AR_SHIFT)&2047)==60);
    assert(((plan.events[1].event.actions[HB_MOTION_LANES]>>HB_AR_SHIFT)&2047)==14);
    /* Borrowed F major has E below F, so Smart Below switches to F-sharp;
       borrowed A-flat major has G below A-flat, so it switches to A above. */
    instance->motif_load[2]=1;g_parallel_scale=1;
    assert(hb_ml_build(instance,&plan,&root,&scale));
    assert(plan.events[0].event.secondary==13&&plan.events[1].event.secondary==13);
    API.destroy_instance(instance);
}
static void adaptive_destination_key(void){
    Inst *instance=approach_fixture();API.set_param(instance,"role","Conductor");
    g_key_context=(hb_key_context){.active=1,.source_root=9,.target_root=11,
        .source_mask=hb_explicit_scale_mask(9,8),.target_mask=hb_explicit_scale_mask(11,8)};
    hb_mt_phrase phrase={.count=1,.anchor=0};
    phrase.events[0]=(hb_mt_event){.duration=24,.count=1,.chord_mode=1,.scale=0xAB5,.selector=4096,.notes={{60,100}}};
    for(int index=0;index<2;index++){
        memset(instance->motif.events,0,sizeof(instance->motif.events));instance->motif.pending=0;
        assert(hb_mt_schedule(instance,&phrase,index?68:65,100,0,0,1,position,position,0));
        unsigned mask=0;for(int i=0;i<HB_MT_SCHEDULE;i++)if(instance->motif.events[i].used)mask|=1u<<mod12(instance->motif.events[i].pitch);
        assert(mask==dim_mask(index?69:68));
    }
    API.destroy_instance(instance);
}
int main(void){lane_capture();adaptive_play_and_storage();export_polyphonic_choices();adaptive_destination_key();puts("Smart lanes, concrete capture, adaptive motifs, mapped destination, mf3 atomic storage and polyphonic clip export pass");}
