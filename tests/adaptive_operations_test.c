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
static void install_export_actions(Inst *instance,int target,const unsigned long long *words){
    char payload[4096];int used=snprintf(payload,sizeof(payload),"%d",target);
    for(int lane=0;lane<=HB_MOTION_LANES;lane++)used+=snprintf(payload+used,sizeof(payload)-used,",%llu",words[lane]);
    API.set_param(instance,"hb_movy_actions",payload);assert(instance->recorded_action_valid[target]);
}
static void adaptive_clip_replay(void){
    for(int role=0;role<2;role++)for(int mode=0;mode<2;mode++)for(int arp=0;arp<2;arp++)for(int side=0;side<2;side++)for(int mapped=0;mapped<2;mapped++){
        Inst *instance=approach_fixture();API.set_param(instance,"role",role?"Follower":"Conductor");API.set_param(instance,"source_channel","1");
        instance->player.config.mode=mode;instance->player.config.playback=arp;
        hb_mt_phrase phrase={.count=1,.anchor=0};
        phrase.events[0]=(hb_mt_event){.duration=24,.count=2,.relation=HB_MT_CHROMATIC,.chord_mode=3,.scale=0x9ad,
            .selector=4096+side,.notes={{65,100},{68,90}}};
        assert(hb_mt_normalize(&phrase,&g_motifs[0]));
        instance->motif_load[0]=27;instance->motif_load[1]=65;instance->motif.editor.placement=4;
        API.set_param(instance,"motif_load_write","Adaptive");API.set_param(instance,"motif_load_prepare","Preview");
        assert(g_ml_plan.count==1&&g_ml_plan.events[0].event.count==2);
        hb_mt_event exported=g_ml_plan.events[0].event;
        assert(hb_mo_word_operation(exported.actions[0])==HB_MO_SMART_BELOW+side);
        assert(!exported.secondary&&!exported.modifier&&!exported.actions[HB_MOTION_LANES]);
        char label[64];API.get_param(instance,"motif_load_labels",label,sizeof(label));assert(!strcmp(label,side?"S Above":"S Below"));
        if(mapped)g_key_context=(hb_key_context){.active=1,.source_root=9,.target_root=11,.source_mask=hb_explicit_scale_mask(9,8),.target_mask=hb_explicit_scale_mask(11,8)};
        instance->movy_playback=1;
        for(int high=0;high<2;high++){
            int target=high?68:65,root=(high?67:66)+2*mapped;
            install_export_actions(instance,target,exported.actions);note(instance,target,1);
            unsigned feedback=(unsigned)instance->relative_feedback_word;
            if((feedback&63)!=(unsigned)(high?14:60))fprintf(stderr,"adaptive role%d mode%d arp%d side%d mapped%d target%d feedback%x\n",role,mode,arp,side,mapped,target,feedback);
            assert((feedback&63)==(unsigned)(high?14:60));assert(((feedback>>10)&15)==(unsigned)mod12(root));
            assert(((feedback>>14)&4095)==(mode?dim_mask(root):1u<<mod12(root)));
            assert(hb_mo_word_operation(instance->recorded_actions[target][0])==HB_MO_SMART_BELOW+side);
            note(instance,target,0);assert(!instance->player.sounding_count);
        }
        API.destroy_instance(instance);
    }
}
static void adaptive_borrowing_and_capacity(void){
    Inst *instance=approach_fixture();API.set_param(instance,"role","Conductor");API.set_param(instance,"source_channel","1");
    hb_mt_phrase phrase={.count=1,.anchor=0};phrase.events[0]=(hb_mt_event){.duration=24,.count=1,.chord_mode=3,.scale=0x9ad,.selector=4096,.notes={{68,100}}};
    assert(hb_mt_normalize(&phrase,&g_motifs[0]));instance->motif_load[0]=27;instance->motif_load[1]=68;instance->motif.editor.placement=4;
    instance->motif_load[2]=instance->motif_load[6]=1;g_parallel_scale=1;
    hb_mc_plan plan;int root,scale;assert(hb_ml_build(instance,&plan,&root,&scale));
    assert(((plan.events[0].event.actions[HB_MOTION_LANES]>>21)&3)==3);
    instance->movy_playback=1;install_export_actions(instance,68,plan.events[0].event.actions);note(instance,68,1);
    assert(((unsigned)instance->relative_feedback_word&63)==60);assert((((unsigned)instance->relative_feedback_word>>10)&15)==9);
    note(instance,68,0);
    hb_mt_event *event=&g_motifs[0].body.events[0];
    for(int lane=0;lane<HB_MOTION_LANES;lane++)event->actions[lane]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_TRANSPOSE)|1000;
    assert(!hb_ml_build(instance,&plan,&root,&scale));assert(!strcmp(g_ml_error,"No room for Smart operation"));
    event->actions[17]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_LEADING_TONE)|1000;
    assert(hb_ml_build(instance,&plan,&root,&scale));assert(hb_mo_word_operation(plan.events[0].event.actions[17])==HB_MO_SMART_BELOW);
    assert(plan.events[0].event.actions[0]==event->actions[0]);
    API.destroy_instance(instance);
}
static void adaptive_changes_and_fixed_stays(void){
    Inst *instance=approach_fixture();API.set_param(instance,"role","Conductor");API.set_param(instance,"source_channel","1");
    hb_mt_phrase phrase={.count=1,.anchor=0};
    phrase.events[0]=(hb_mt_event){.duration=24,.count=1,.chord_mode=3,.scale=0x9ad,.selector=4096,.notes={{65,100}}};
    assert(hb_mt_normalize(&phrase,&g_motifs[0]));instance->motif_load[0]=27;instance->motif_load[1]=65;instance->motif.editor.placement=4;
    hb_mc_plan fixed,adaptive;int root,scale;
    assert(hb_ml_build(instance,&fixed,&root,&scale));assert(fixed.events[0].event.secondary==13);
    API.set_param(instance,"motif_load_write","Adaptive");assert(hb_ml_build(instance,&adaptive,&root,&scale));
    instance->movy_playback=1;
    for(int round=0;round<2;round++){
        hb_set_shared_follower_scale(round?1:8);
        g_bus.observed_harmony=chord(9,round?0:1,0);hb_effective_write(g_bus.observed_harmony);
        install_export_actions(instance,65,adaptive.events[0].event.actions);note(instance,65,1);
        assert(((unsigned)instance->relative_feedback_word&63)==(round?14u:60u));note(instance,65,0);
        install_export_actions(instance,65,fixed.events[0].event.actions);note(instance,65,1);
        assert(((unsigned)instance->relative_feedback_word&63)==60);note(instance,65,0);
    }
    char state[256],value[64];assert(hb_ml_save(instance,state,sizeof(state),0)>0);
    API.set_param(instance,"motif_load_write","Fixed");hb_ml_restore(instance,state);
    API.get_param(instance,"motif_load_write",value,sizeof(value));assert(!strcmp(value,"Adaptive"));
    hb_ml_restore(instance,";ml1,27,65,0,12,0,0");assert(!instance->motif_load[6]);
    hb_ml_restore(instance,";ml2,27,65,0,12,0,0,2");assert(!instance->motif_load[6]);
    /* Unsupported high operation ids cannot alias an old pitch operation. */
    unsigned long long bad[HB_MOTION_LANES+1]={0};bad[0]=HB_MO_RECORDED|hb_mo_operation_word(69)|1000;
    instance->recorded_action_valid[65]=0;char payload[4096];int used=snprintf(payload,sizeof(payload),"65");
    for(int lane=0;lane<=HB_MOTION_LANES;lane++)used+=snprintf(payload+used,sizeof(payload)-used,",%llu",bad[lane]);
    API.set_param(instance,"hb_movy_actions",payload);assert(!instance->recorded_action_valid[65]);
    API.destroy_instance(instance);
}
int main(void){adaptive_borrowing_and_capacity();adaptive_clip_replay();adaptive_changes_and_fixed_stays();lane_capture();adaptive_play_and_storage();export_polyphonic_choices();adaptive_destination_key();puts("Smart lanes, concrete capture, adaptive motifs, mapped destination, mf3 atomic storage and polyphonic clip export pass");}
