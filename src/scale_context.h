/* Output gap collections never replace the harmonic parent or input keyboard. */
static int hb_chord_minor(hb_harmony_t harmony){
    unsigned mask=hb_harmony_chord_mask(harmony);return (mask&(1u<<mod12(harmony.root_pc+3)))&&!(mask&(1u<<mod12(harmony.root_pc+4)));
}
static int hb_chord_dominant(hb_harmony_t harmony){
    unsigned mask=hb_harmony_chord_mask(harmony);return (mask&(1u<<mod12(harmony.root_pc+4)))&&(mask&(1u<<mod12(harmony.root_pc+10)))&&!(mask&(1u<<mod12(harmony.root_pc+11)));
}
static unsigned hb_local_recipe(Inst *instance,hb_harmony_t harmony){
    int root=harmony.root_pc;unsigned mask=hb_harmony_chord_mask(harmony);
    int minor=hb_chord_minor(harmony),flat_fifth=(mask&(1u<<mod12(root+6)))&&!(mask&(1u<<mod12(root+7)));
    int index=hb_policy_value(instance,HB_P_MAJOR)?5:1;
    if(minor){
        index=hb_policy_value(instance,HB_P_MINOR)?2:3;
        if(flat_fifth){
            if(mask&(1u<<mod12(root+9)))return hb_transpose_mask(0xb6du,root); /* whole-half diminished */
            index=hb_policy_value(instance,HB_P_HALFDIM)?14:7;
        }else if(mask&(1u<<mod12(root+11)))index=9;
    }else if(hb_chord_dominant(harmony))index=6;
    else if((mask&(1u<<mod12(root+8)))&&!(mask&(1u<<mod12(root+7))))index=11;
    return hb_accommodate_chord(hb_explicit_scale_mask(root,index),harmony,root);
}
/* Match every occurrence of a chord; disagreeing repeated contexts stay local.
   This is independent of the track's playback lookahead setting. */
static int hb_context_destination(Inst *instance,hb_harmony_t harmony,int *minor_out){
    if(harmony.intent_kind&&harmony.intent_kind<6){*minor_out=harmony.intent_minor;return harmony.intent_target;}
    if(!g_bus.next_model_locked||g_bus.next_model_count<2)return -1;
    int destination=-1,minor=0,matched=0;
    for(int event=0;event<g_bus.next_model_count;event++){
        hb_harmony_t current=g_bus.next_model[event].harmony;
        if(current.root_pc!=harmony.root_pc||hb_harmony_chord_mask(current)!=hb_harmony_chord_mask(harmony))continue;
        hb_harmony_t next=g_bus.next_model[(event+1)%g_bus.next_model_count].harmony;
        hb_harmony_t after=g_bus.next_model[(event+2)%g_bus.next_model_count].harmony;
        hb_harmony_t previous=g_bus.next_model[(event+g_bus.next_model_count-1)%g_bus.next_model_count].harmony;
        int found=-1,family=0;
        if(hb_policy_value(instance,HB_P_CONTEXT)&&hb_chord_minor(harmony)&&hb_chord_dominant(next)&&mod12(next.root_pc-harmony.root_pc)==5&&mod12(after.root_pc-next.root_pc)==2){
            found=after.root_pc;family=1;
        }else if(hb_policy_value(instance,HB_P_CONTEXT)&&hb_chord_dominant(harmony)&&mod12(next.root_pc-harmony.root_pc)==2&&hb_chord_minor(previous)&&mod12(harmony.root_pc-previous.root_pc)==5){
            found=next.root_pc;family=1;
        }else if(hb_chord_minor(harmony)&&(hb_harmony_chord_mask(harmony)&(1u<<mod12(harmony.root_pc+6)))&&mod12(next.root_pc-harmony.root_pc)==1){
            found=next.root_pc;family=hb_chord_minor(next);
        }else if(hb_chord_dominant(harmony)&&mod12(next.root_pc-harmony.root_pc)==5){found=next.root_pc;family=hb_chord_minor(next);}
        else if(hb_chord_minor(harmony)&&hb_chord_dominant(next)&&mod12(next.root_pc-harmony.root_pc)==5){
            found=mod12(next.root_pc+5);
            family=!!(hb_harmony_chord_mask(harmony)&(1u<<mod12(harmony.root_pc+6)));
            if(hb_policy_value(instance,HB_P_CONTEXT)&&after.root_pc==found)family=hb_chord_minor(after);
        }else if(hb_policy_value(instance,HB_P_CONTEXT)&&hb_chord_dominant(previous)&&mod12(harmony.root_pc-previous.root_pc)==5){
            found=harmony.root_pc;family=hb_chord_minor(harmony);
        }
        if(found<0)return -1;
        if(matched&&(destination!=found||minor!=family))return -1;
        destination=found;minor=family;matched=1;
    }
    *minor_out=minor;return destination;
}
static unsigned hb_local_output_scale(Inst *instance,hb_harmony_t harmony){
    unsigned local=hb_local_recipe(instance,harmony);
    if(hb_policy_value(instance,HB_P_GAP)==2&&harmony.intent_kind==6&&harmony.intent_scale&&!(hb_harmony_chord_mask(harmony)&~harmony.intent_scale))
        return hb_accommodate_chord(harmony.intent_scale,harmony,harmony.intent_target);
    int minor=0,destination=-1;
    if(harmony.intent_kind&&harmony.intent_kind<6)destination=hb_context_destination(instance,harmony,&minor);
    else if(hb_policy_value(instance,HB_P_GAP)==2)destination=hb_context_destination(instance,harmony,&minor);
    if(destination<0)return local;
    if(harmony.intent_kind==4)return hb_accommodate_chord(hb_explicit_scale_mask(harmony.root_pc,12),harmony,harmony.root_pc);
    /* Connector identity is explicit but does not imply a dominant cadence. */
    if(harmony.intent_kind==5)return local;
    unsigned collection=hb_explicit_scale_mask(destination,minor?2:1);
    unsigned dominant=hb_dominant_scale_mask(instance,harmony,destination);
    if(dominant)collection=dominant;
    else {unsigned borrowed=hb_borrowed_scale_mask(instance,harmony,destination,collection);if(borrowed)collection=borrowed;}
    return hb_accommodate_chord(collection,harmony,destination);
}
