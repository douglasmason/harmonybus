/* Exercise editor changes through native playback and exported recorded notes. */
#define HB_RELATIVE_OPERATIONS_FIXTURE
#include "relative_operations_test.c"

static hb_mt_event captured_event(void){
    hb_mt_event event={.duration=18,.count=2,.relation=HB_MT_CHROMATIC,.secondary=12,
        .chord_mode=1,.scale=0x9ad,.notes={{65,93},{68,71}}};
    event.actions[HB_MOTION_LANES]=hb_mo_role_word(12)|(14ULL<<HB_AR_SHIFT);
    event.actions[0]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_LEADING_TONE)|1000;
    event.actions[1]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_VELOCITY)|100000;
    event.actions[2]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_ROTATE)|1000;
    event.actions[3]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_GATE)|50000;
    return event;
}
static void edit_restore_and_undo(void){
    Inst *instance=approach_fixture();hb_mt_recorder *editor=&instance->motif.editor;
    hb_mt_event original=captured_event();
    hb_mt_begin(editor,0);editor->draft.count=1;editor->draft.anchor=0;editor->draft.events[0]=original;
    char label[64];API.get_param(instance,"motif_status",label,sizeof(label));assert(!strcmp(label,"1 Captured: LT"));
    for(int choice=1;choice<HB_RO_MOTIF_COUNT;choice++){
        API.set_param(instance,"motif_operation",HB_RO_MOTIF_NAMES[choice]);
        hb_mt_event expected=original;expected.selector=HB_RO_MOTIF_TOKENS[choice];
        assert(!memcmp(&expected,&editor->draft.events[0],sizeof(expected)));
        API.get_param(instance,"motif_operation",label,sizeof(label));assert(!strcmp(label,HB_RO_MOTIF_NAMES[choice]));
        /* Invalid/no-op edits must not consume the undo checkpoint. */
        API.set_param(instance,"motif_operation","not an operation");
        API.set_param(instance,"motif_operation",HB_RO_MOTIF_NAMES[choice]);
        API.set_param(instance,"motif_undo","Undo");assert(!memcmp(&original,&editor->draft.events[0],sizeof(original)));
        API.set_param(instance,"motif_step","1");
        API.set_param(instance,"motif_operation",HB_RO_MOTIF_NAMES[choice]);
        API.set_param(instance,"motif_operation","Captured");assert(!memcmp(&original,&editor->draft.events[0],sizeof(original)));
    }
    API.set_param(instance,"motif_operation","CCB");assert(editor->draft.events[0].selector==1);
    API.set_param(instance,"motif_record","Done");
    char saved[16384];assert(hb_mt_save(instance,saved,sizeof(saved),0)>0);assert(strstr(saved,";mf3,"));
    hb_mt_definition definition=g_motifs[0];memset(g_motifs,0,sizeof(g_motifs));g_motifs_restored=0;
    hb_mt_restore(instance,saved);assert(!memcmp(&definition,&g_motifs[0],sizeof(definition)));
    API.set_param(instance,"motif_record","Edit");API.set_param(instance,"motif_operation","Captured");
    assert(!memcmp(&original,&editor->draft.events[0],sizeof(original)));
    /* Rest/tie destinations change the next-entry choice, never the rest/tie. */
    editor->draft.events[0]=(hb_mt_event){.kind=1,.duration=24};hb_mt_event rest=editor->draft.events[0];
    API.set_param(instance,"motif_operation","Upper Dim");assert(!memcmp(&rest,&editor->draft.events[0],sizeof(rest)));
    hb_mt_add(editor,0,65,101,&original);assert(editor->draft.events[0].selector==60);
    assert(editor->draft.events[0].notes[0].velocity==101);hb_mt_release(editor,0,65);
    API.destroy_instance(instance);
}
static void identity_and_replacement(void){
    hb_mt_event event=captured_event();assert(!strcmp(hb_mt_operation_label(&event),"LT"));
    event.modifier=-1;assert(!strcmp(hb_mt_operation_label(&event),"Composite"));event.modifier=0;
    event.flags=HB_MO_SIMPLE;assert(!strcmp(hb_mt_operation_label(&event),"Composite"));event.flags=0;
    event.cadence=1;assert(!strcmp(hb_mt_operation_label(&event),"Cadence"));
    event.actions[4]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_CADENCE_II_V)|1000;
    event.actions[5]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_MIXED_FIRST)|1000;
    event.actions[6]=HB_MO_RECORDED|hb_mo_operation_word(HB_MO_ENCLOSE_AB)|1000;
    event.actions[HB_MOTION_LANES]|=HB_MO_SIMPLE|HB_MO_SIMPLE_SCALE|HB_MO_LEGACY_II|(1ULL<<13)|HB_MO_RHYTHM_BAKED;
    hb_mt_event original=event;
    for(int choice=1;choice<HB_RO_MOTIF_COUNT;choice++){
        event=original;
        unsigned selected=hb_ro_resolve_selector(HB_RO_MOTIF_TOKENS[choice],65,0x9ad);
        const hb_ro_definition *operation=hb_ro_token(selected);hb_mt_apply_operation(&event,operation);
        assert(!strcmp(hb_mt_operation_label(&event),operation->short_name));
        assert(!event.cadence&&event.secondary==operation->role&&event.modifier==operation->modifier);
        assert(event.count==original.count&&event.duration==original.duration&&event.chord_mode==original.chord_mode&&event.relation==original.relation);
        assert(!memcmp(event.notes,original.notes,sizeof(event.notes)));
        for(int lane=1;lane<=3;lane++)assert(event.actions[lane]==original.actions[lane]);
        for(int lane=4;lane<=6;lane++)assert(event.actions[lane]==HB_MO_RECORDED);
        assert(event.actions[HB_MOTION_LANES]&HB_MO_RHYTHM_BAKED);
    }
}
static void install_actions(Inst *instance,int target,const unsigned long long *words){
    char payload[4096];int used=snprintf(payload,sizeof(payload),"%d",target);
    for(int lane=0;lane<=HB_MOTION_LANES;lane++)used+=snprintf(payload+used,sizeof(payload)-used,",%llu",words[lane]);
    API.set_param(instance,"hb_movy_actions",payload);assert(instance->recorded_action_valid[target]);
}
static void polyphonic_storage(void){
    Inst *instance=approach_fixture();
    for(int choice=1;choice<HB_RO_MOTIF_COUNT;choice++){
        hb_mt_phrase phrase={.count=1,.anchor=0};phrase.events[0]=captured_event();
        phrase.events[0].selector=HB_RO_MOTIF_TOKENS[choice];
        assert(hb_mt_normalize(&phrase,&g_motifs[0]));
        hb_mt_definition original=g_motifs[0];
        char saved[16384];assert(hb_mt_save(instance,saved,sizeof(saved),0)>0);
        memset(g_motifs,0,sizeof(g_motifs));g_motifs_restored=0;hb_mt_restore(instance,saved);
        assert(!memcmp(&original,&g_motifs[0],sizeof(original)));
        instance->motif_load[0]=27;instance->motif_load[1]=65;instance->motif.editor.placement=4;
        for(int adaptive=0;adaptive<2;adaptive++){
            instance->motif_load[6]=adaptive;hb_mc_plan plan;int root,scale;
            assert(hb_ml_build(instance,&plan,&root,&scale));
            int split=!adaptive&&hb_ro_adaptive(phrase.events[0].selector);
            assert(plan.count==(split?2:1));
            for(int index=0;index<plan.count;index++){
                hb_mt_event *event=&plan.events[index].event;
                assert(event->count==(split?1:2)&&event->duration==18&&event->chord_mode==1);
                for(int voice=0;voice<event->count;voice++)assert(!memcmp(&event->notes[voice],&phrase.events[0].notes[split?index:voice],sizeof(hb_mt_note)));
                for(int lane=1;lane<=3;lane++)assert(event->actions[lane]==phrase.events[0].actions[lane]);
            }
        }
        /* Unknown override IDs cannot partially replace a saved library. */
        g_motifs[0].body.events[0].selector=0xfffe;
        assert(hb_mt_save(instance,saved,sizeof(saved),0)>0);g_motifs[0]=original;g_motifs_restored=0;
        hb_mt_restore(instance,saved);assert(!memcmp(&original,&g_motifs[0],sizeof(original)));
    }
    API.destroy_instance(instance);
}
static void playback_and_export(void){
    int mismatches=0;
    for(int mapped=0;mapped<2;mapped++)for(int role=0;role<2;role++)for(int mode=0;mode<2;mode++)for(int choice=1;choice<HB_RO_MOTIF_COUNT;choice++){
        Inst *instance=approach_fixture();API.set_param(instance,"role",role?"Follower":"Conductor");API.set_param(instance,"source_channel","1");
        instance->player.config.mode=mode;instance->motif.editor.arrival=0;
        /* Deliberately non-diminished connector policy distinguishes CCB/CCA
           from LT/UD even though their roots move the same semitone. */
        API.set_param(instance,"chromatic_quality","Major / Maj7");
        if(mapped)g_key_context=(hb_key_context){.active=1,.source_root=9,.target_root=11,
            .source_mask=hb_explicit_scale_mask(9,8),.target_mask=hb_explicit_scale_mask(11,8)};
        hb_mt_phrase phrase={.count=1,.anchor=0};phrase.events[0]=captured_event();
        phrase.events[0].count=1;phrase.events[0].chord_mode=mode;
        memset(phrase.events[0].actions+1,0,3*sizeof(unsigned long long));
        phrase.events[0].selector=HB_RO_MOTIF_TOKENS[choice];
        assert(hb_mt_normalize(&phrase,&g_motifs[0]));instance->motif_load[0]=27;instance->motif_load[1]=65;instance->motif.editor.placement=4;
        assert(hb_mt_schedule(instance,&phrase,65,100,0,0,1,position,position,0));
        unsigned scheduled_mask=0,scheduled_feedback=0;
        for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used){
            scheduled_mask|=1u<<mod12(instance->motif.events[index].pitch);
            if(instance->motif.events[index].relative_feedback)scheduled_feedback=instance->motif.events[index].relative_feedback;
        }
        assert(scheduled_mask&&scheduled_feedback);
        if(mode&&!mapped){
            unsigned token=phrase.events[0].selector;
            if(token==14||token==60||hb_ro_adaptive(token))assert(scheduled_mask==hb_transpose_mask(0x249,token==14?4:6));
            if(token==1||token==2)assert(scheduled_mask==hb_transpose_mask(0x891,token==1?4:6));
            if(token==10)assert(scheduled_mask==hb_transpose_mask(0x491,6));
        }
        /* Scheduling is immutable; both export modes preserve the same event. */
        assert(!memcmp(&phrase.events[0],&g_motifs[0].body.events[0],sizeof(hb_mt_event)));
        memset(instance->motif.events,0,sizeof(instance->motif.events));instance->motif.pending=0;
        for(int adaptive=0;adaptive<2;adaptive++){
            g_bus.observed_harmony=chord(9,1,0);hb_effective_write(g_bus.observed_harmony);
            instance->motif_load[6]=adaptive;hb_mc_plan plan;int root,scale;
            assert(hb_ml_build(instance,&plan,&root,&scale));assert(plan.count==1);
            install_actions(instance,65,plan.events[0].event.actions);instance->movy_playback=1;
            note(instance,65,1);unsigned feedback=(unsigned)instance->relative_feedback_word;
            if((feedback&0x7ffffff)!=(scheduled_feedback&0x7ffffff))fprintf(stderr,"choice%s mapped%d role%d mode%d adaptive%d scheduled%x actual%x mask%x\n",HB_RO_MOTIF_NAMES[choice],mapped,role,mode,adaptive,scheduled_feedback,feedback,scheduled_mask);
            mismatches+=(feedback&0x7ffffff)!=(scheduled_feedback&0x7ffffff);
            note(instance,65,0);assert(!instance->player.sounding_count);
        }
        API.destroy_instance(instance);
    }
    assert(!mismatches);
}
int main(void){edit_restore_and_undo();identity_and_replacement();polyphonic_storage();playback_and_export();puts("Motif RPO editor: all catalog recipes, captured restoration, undo, mf3, note entry, composite identity, unrelated actions, native and Fixed/Adaptive playback pass");}
