#ifndef HB_MOTIF_OPERATIONS_H
#define HB_MOTIF_OPERATIONS_H
/* Authored operations are reversible overrides. Only rendering/export copies
   are rewritten; the saved captured event remains available via Captured. */
static int hb_mt_relative_motion(int operation){
    return hb_ro_motion(operation)||hb_mo_mixed(operation)||
        operation==HB_MO_CADENCE_II_V||operation==HB_MO_CADENCE_BACKDOOR||
        operation==HB_MO_CADENCE_TRITONE||operation==HB_MO_ENCLOSE_AB||operation==HB_MO_ENCLOSE_BA;
}
static void hb_mt_apply_operation(hb_mt_event *event,const hb_ro_definition *operation){
    event->secondary=operation->role;event->modifier=operation->modifier;event->cadence=0;
    event->flags&=~(HB_MO_SIMPLE|HB_MO_SIMPLE_SCALE|HB_MO_CONNECTOR_ABOVE|HB_MO_LEGACY_II);
    if(operation->connector_above)event->flags|=HB_MO_CONNECTOR_ABOVE;
    unsigned long long *intent=&event->actions[HB_MOTION_LANES];
    *intent&=~(HB_AR_MASK|HB_MO_CADENCE_MASK|0x573ULL|HB_MO_SIMPLE|HB_MO_SIMPLE_SCALE|HB_MO_LEGACY_II|(1ULL<<63));
    *intent|=hb_ar_intent(operation->token)|((unsigned long long)operation->token<<HB_AR_SHIFT);
    for(int lane=0;lane<HB_MOTION_LANES;lane++)
        if((event->actions[lane]&HB_MO_RECORDED)&&hb_mt_relative_motion(hb_mo_word_operation(event->actions[lane])))
            event->actions[lane]=HB_MO_RECORDED;
}
/* A captured label must describe the whole recipe, not just its displacement.
   Conflicting identities and scale-policy variants remain visibly composite. */
static const char *hb_mt_operation_label(const hb_mt_event *event){
    const hb_ro_definition *operation=hb_ro_token(event->selector);
    if(operation)return operation->short_name;
    if(event->kind)return event->kind==1?"Rest":"Tie";
    if(event->cadence)return "Cadence";
    unsigned long long intent=event->actions[HB_MOTION_LANES];
    if((event->flags|intent)&(HB_MO_SIMPLE|HB_MO_SIMPLE_SCALE|HB_MO_LEGACY_II))return "Composite";
    if((event->secondary&&event->secondary!=3&&event->modifier)||(intent&HB_MO_CADENCE_MASK))return "Composite";
    if(event->secondary&&event->secondary!=3&&!hb_ro_role(event->secondary))return "Composite";
    operation=hb_ro_resolved(event->secondary,event->modifier,(event->flags&HB_MO_CONNECTOR_ABOVE)!=0);
    int role=(int)(((intent>>4)&7)|((intent>>5)&8)|((intent>>59)&16));
    int modifier=(intent&3)==1?-1:(intent&3)==2?1:(intent&3)==3?2:0;
    if(role&&role!=3&&modifier)return "Composite";
    if(role&&role!=3&&!hb_ro_role(role))return "Composite";
    const hb_ro_definition *source=hb_ro_resolved(role,modifier,(intent&HB_MO_CONNECTOR_ABOVE)!=0);
    unsigned token=(unsigned)((intent>>HB_AR_SHIFT)&2047);
    const hb_ro_definition *encoded=hb_ro_token(token);
    if(token&&!encoded)return "Composite";
    if(source&&operation&&source!=operation)return "Composite";
    if(source)operation=source;
    if(encoded&&operation&&encoded!=operation)return "Composite";
    if(encoded)operation=encoded;
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        unsigned long long word=event->actions[lane];
        if(!(word&HB_MO_RECORDED)||(int32_t)(uint32_t)word<=0)continue;
        int motion=hb_mo_word_operation(word);
        if(!hb_mt_relative_motion(motion))continue;
        const hb_ro_definition *recorded=hb_ro_motion(motion);
        if(!recorded||(operation&&recorded!=operation))return "Composite";
        operation=recorded;
    }
    return operation?operation->short_name:event->secondary==3||role==3?"T":"Input";
}
#endif
