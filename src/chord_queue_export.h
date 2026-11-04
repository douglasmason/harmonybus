/* Explicit Copy reads the native retained descriptors, never mutable UI banks.
   This getter is bounded by the destination clip format and does not service,
   rewind or otherwise change the live queue. */
#define HB_CQ_COPY_NOTES 512
#define HB_CQ_COPY_TICKS (16 * 4 * 96)
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
    double origin=hb_cq_next_boundary(hb_cq_beat());
    /* Play restarts a stopped retained phrase at the clip's first phase.
       Fixed grids use their full cadence, including with anticipation. */
    if(!queue->running&&hb_chord_grid_beats()<=0&&g_bus.next_model_locked&&g_bus.next_model_count>0)
        origin=hb_current_beat()-hb_next_phase(hb_clip_playhead());
    if(queue->running){
        origin=queue->count&&queue->pending[0].at>=0?queue->pending[0].at:hb_cq_next_boundary(hb_cq_beat());
        int next=queue->cycle_position>=0?(queue->cycle_position+1)%queue->stored_count:0;
        for(int index=next;index>0&&index<queue->stored_count;index++)origin=hb_cq_next_boundary(origin);
    }
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
