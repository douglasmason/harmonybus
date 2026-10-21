#ifndef HB_MOTIF_CHOICE_H
#define HB_MOTIF_CHOICE_H
/* Stable slot identities: slot N always samples field channel N. Display
 * reads neither advance time nor consume a random draw. */
static unsigned hb_mt_choice_members(void){
    unsigned mask=0;
    for(int slot=0;slot<16;slot++)if(hb_mt_occupied(&g_motifs[slot]))mask|=1u<<slot;
    return mask;
}
static int hb_mt_choice_view(Inst *instance,int slots[16],double result[16]){
    hb_mt_choice *choice=&instance->motif.choice;
    unsigned members=hb_mt_choice_members(),pins=choice->held|choice->latched,compact_pins=0;
    if(members!=choice->members)pins=0;
    double weights[16],values[16];int count=0;
    for(int slot=0;slot<16;slot++)if(members&(1u<<slot)){
        const hb_motion_lane *lane=&instance->motion.lanes[slot];
        slots[count]=slot;
        weights[count]=hb_nf_output(slot,hb_nf_lane_sample(slot,lane->field_x*.01,lane->field_y*.01,lane->field_z*.01));
        values[count]=choice->values[slot];
        if(pins&(1u<<slot))compact_pins|=1u<<count;
        count++;
    }
    if(!count)return 0;
    if(!hb_probability_constrain(weights,count,compact_pins,values,result))return 0;
    if(choice->release_mask==members&&g_hb_field.glide>0){
        double amount=(choice->seconds-choice->release_at)*1000/g_hb_field.glide;
        if(amount<1){
            double previous[16],blended[16];
            for(int index=0;index<count;index++)previous[index]=choice->release_from[slots[index]];
            hb_probability_blend(previous,result,count,amount,blended);
            /* Reapply current pins, including new gestures during a release. */
            hb_probability_constrain(blended,count,compact_pins,values,result);
        }
    }
    return count;
}
static void hb_mt_choice_release(Inst *instance,unsigned mask,int commit){
    hb_mt_choice *choice=&instance->motif.choice;int slots[16];double values[16];
    int count=hb_mt_choice_view(instance,slots,values);
    if(commit&&g_hb_field_latch_mode)choice->latched|=choice->held&mask;
    else choice->latched&=~mask;
    choice->held&=~mask;
    choice->release_mask=hb_mt_choice_members();choice->release_at=choice->seconds;
    for(int index=0;index<count;index++)choice->release_from[slots[index]]=values[index];
}
static int hb_mt_choice_edit(Inst *instance,int slot,double requested){
    hb_mt_choice *choice=&instance->motif.choice;int slots[16],selected=-1;double current[16],pinned[16],next[16];
    unsigned members=hb_mt_choice_members();
    if(members!=choice->members){choice->held=choice->latched=choice->release_mask=0;choice->members=members;}
    int count=hb_mt_choice_view(instance,slots,current);unsigned compact_pins=0;
    for(int index=0;index<count;index++){
        int candidate=slots[index];pinned[index]=choice->values[candidate];
        if(candidate==slot)selected=index;
        if((choice->held|choice->latched)&(1u<<candidate))compact_pins|=1u<<index;
    }
    if(selected<0||!hb_probability_edit(current,count,compact_pins,pinned,selected,requested,next))return 0;
    /* This snapshot makes the edit exact even during an earlier release. */
    choice->release_mask=hb_mt_choice_members();choice->release_at=choice->seconds;
    for(int index=0;index<count;index++)choice->release_from[slots[index]]=next[index];
    choice->values[slot]=next[selected];choice->held|=1u<<slot;choice->latched&=~(1u<<slot);
    choice->expires[slot]=choice->seconds+1.5;
    return 1;
}
static void hb_mt_choice_advance(Inst *instance,double seconds){
    hb_mt_choice *choice=&instance->motif.choice;
    if(seconds>0)choice->seconds+=seconds;
    if(!(choice->held|choice->latched))return;
    unsigned expired=0,members=hb_mt_choice_members();
    if(members!=choice->members){choice->held=choice->latched=choice->release_mask=0;choice->members=members;}
    choice->held&=members;choice->latched&=members;
    if(!g_hb_field_latch_mode)expired|=choice->latched;
    for(int slot=0;slot<16;slot++)if((choice->held&(1u<<slot))&&choice->seconds>=choice->expires[slot])expired|=1u<<slot;
    if(expired)hb_mt_choice_release(instance,expired,0);
}
/* Only the operation path calls this wrapper; editor auditions keep their
 * selected motif. A tap phrase retains its materialized snapshot. */
static int hb_mt_choice_launch(Inst *instance,int lane,int input,int velocity,int channel){
    hb_mt_runtime *runtime=&instance->motif;hb_mt_recorder *editor=&runtime->editor;
    if(instance->motion.lanes[lane].field_target!=3)return hb_mt_launch(instance,input,velocity,channel);
    int fresh=!editor->playback||!runtime->tap_active||runtime->tap_input!=input||runtime->tap_channel!=channel||runtime->cancel;
    if(!fresh)return hb_mt_launch(instance,input,velocity,channel);
    int slots[16];double probabilities[16];int count=hb_mt_choice_view(instance,slots,probabilities);
    if(!count){editor->error=10;return 0;}
    unsigned seed=hb_nf_hash(runtime->choice.draws+1)^hb_nf_hash((unsigned)g_hb_field.seed)^hb_nf_hash((unsigned)hb_override_index(instance)+lane*17u);
    double draw=(hb_nf_hash(seed)+.5)/4294967296.;int selected=count-1;
    for(int index=0;index<count;index++){draw-=probabilities[index];if(draw<0){selected=index;break;}}
    int old_preset=editor->preset,old_selected=editor->selected;
    editor->preset=0;editor->selected=slots[selected];
    int succeeded=hb_mt_launch(instance,input,velocity,channel);
    editor->preset=old_preset;editor->selected=old_selected;
    if(succeeded){runtime->choice.draws++;runtime->choice.last_slot=slots[selected];}
    else runtime->tap_active=0; /* A failed first tap must retry as a new draw. */
    return succeeded;
}
#endif
