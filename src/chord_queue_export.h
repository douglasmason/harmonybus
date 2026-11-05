/* Explicit Copy reads the native retained descriptors, never mutable UI banks.
   This getter is bounded by the destination clip format and does not service,
   rewind or otherwise change the live queue. */
#define HB_CQ_COPY_NOTES 512
#define HB_CQ_COPY_TICKS (16 * 4 * 96)
static double hb_cq_copy_origin(void){
    const hb_chord_queue *queue=&g_chord_queue;
    double origin=hb_cq_next_boundary(hb_cq_beat());
    if(!queue->running&&hb_chord_grid_beats()<=0&&g_bus.next_model_locked&&g_bus.next_model_count>0)
        origin=hb_current_beat()-hb_next_phase(hb_clip_playhead());
    if(queue->running){
        origin=queue->count&&queue->pending[0].at>=0?queue->pending[0].at:hb_cq_next_boundary(hb_cq_beat());
        int next=queue->cycle_position>=0?(queue->cycle_position+1)%queue->stored_count:0;
        for(int index=next;index>0&&index<queue->stored_count;index++)origin=hb_cq_next_boundary(origin);
    }
    return origin;
}
static int hb_cq_export(const Inst *instance,char *buffer,int length){
    if(!buffer||length<=0)return -1;
    const hb_chord_queue *queue=&g_chord_queue;
    if(!hb_cq_owned(instance)||!hb_cq_owner()||!queue->stored_count)
        return snprintf(buffer,(size_t)length,"error:No chord queue");
    if(queue->phrase_held||queue->cycle_count!=queue->stored_count)
        return snprintf(buffer,(size_t)length,"error:Finish the queued phrase");
    if(queue->stored_count>HB_CQ_COPY_NOTES)
        return snprintf(buffer,(size_t)length,"error:Queue exceeds clip capacity");
    /* Capture one complete upcoming cycle in the existing scheduler's order.
       Observed timing uses the same learned-boundary resolver as playback. */
    double origin=hb_cq_copy_origin();
    /* Play restarts a stopped retained phrase at the clip's first phase.
       Fixed grids use their full cadence, including with anticipation. */
    unsigned gates[HB_CQ_COPY_NOTES];
    unsigned total=0;double boundary=origin;
    unsigned long long signature=14695981039346656037ULL;
    for(int index=0;index<queue->stored_count;index++){
        const hb_cq_event *event=&queue->cycle[index];
        boundary=hb_cq_next_boundary(boundary);
        double ticks=(boundary-origin)*96.0;
        if(!(ticks>=0)||ticks>HB_CQ_COPY_TICKS+0.0001)
            return snprintf(buffer,(size_t)length,"error:Queue exceeds 16-bar clip");
        unsigned end=(unsigned)(ticks+0.5);
        if(end<=total||ticks-end>0.0001||end-ticks>0.0001||event->note<0||event->note>127||!hb_selection_word(event->word))
            return snprintf(buffer,(size_t)length,"error:Queue timing cannot be copied exactly");
        gates[index]=end-total;total=end;
        /* Playback cursor and wall time are excluded. Retained musical edits
           and a changed observed rhythm invalidate a destination review. */
        unsigned long long values[3]={event->word,(unsigned)event->note,gates[index]};
        for(int value=0;value<3;value++)for(int byte=0;byte<8;byte++){
            signature^=(values[value]>>(8*byte))&255u;signature*=1099511628211ULL;
        }
    }
    if(total%24)return snprintf(buffer,(size_t)length,"error:Queue cycle does not fit clip grid");
    int used=snprintf(buffer,(size_t)length,"qx1|%016llx|%d|%u",signature,queue->stored_count,total);
    for(int index=0;index<queue->stored_count;index++){
        int available=used<length?length-used:0;
        used+=snprintf(available?buffer+used:0,(size_t)available,"|%d,%016llx,%u",
            queue->cycle[index].note,queue->cycle[index].word,gates[index]);
    }
    return used;
}

static int hb_cq_handoff_identity(Inst *instance,unsigned long long token){
    /* The header contains the complete fingerprint even in this small buffer.
       Export remains allocation-free and is checked only on arm/transfer. */
    char header[64],expected[32];
    hb_cq_export(instance,header,sizeof(header));
    snprintf(expected,sizeof(expected),"qx1|%016llx|",token);
    return !strncmp(header,expected,21);
}
static void hb_cq_handoff_arm(Inst *instance,const char *parameter){
    if(!hb_cq_owned(instance))return;
    g_chord_queue.handoff_armed=0;
    if(!strcmp(parameter,"off"))return;
    unsigned long long token=0;
    if(strlen(parameter)!=16)return;
    for(int index=0;index<16;index++){
        char value=parameter[index];
        int digit=value>='0'&&value<='9'?value-'0':value>='a'&&value<='f'?value-'a'+10:-1;
        if(digit<0)return;
        token=(token<<4)|(unsigned)digit;
    }
    hb_cq_sync();
    if(!g_chord_queue.running||!g_chord_queue.enabled||g_chord_queue.muted||
       !hb_cq_handoff_identity(instance,token))return;
    g_chord_queue.handoff_token=token;
    g_chord_queue.handoff_at=hb_cq_copy_origin();
    g_chord_queue.handoff_timing=g_bus.chord_timing;
    g_chord_queue.handoff_anticipation=g_bus.anticipation;
    g_chord_queue.handoff_armed=1;
}
static int hb_cq_handoff_ready(Inst *instance){
    if(!hb_cq_owned(instance)||!hb_cq_owner()||!g_chord_queue.handoff_armed)return -1;
    hb_cq_sync();
    if(!g_chord_queue.handoff_armed||!g_chord_queue.running||!g_chord_queue.enabled||g_chord_queue.muted||
       g_chord_queue.phrase_held||g_chord_queue.handoff_timing!=g_bus.chord_timing||
       g_chord_queue.handoff_anticipation!=g_bus.anticipation){
        g_chord_queue.handoff_armed=0;return -1;
    }
    if(hb_cq_beat()+1e-6<g_chord_queue.handoff_at)return 0;
    if(g_chord_queue.count&&(g_chord_queue.pending!=g_chord_queue.cycle||
       g_chord_queue.pending[0].at>hb_cq_beat()+1e-6)){
        g_chord_queue.handoff_armed=0;return -1;
    }
    if(!hb_cq_handoff_identity(instance,g_chord_queue.handoff_token)){
        g_chord_queue.handoff_armed=0;return -1;
    }
    return 1;
}
