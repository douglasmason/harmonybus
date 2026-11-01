/* One live conductor timeline. Clip learning remains separate; all consumers
   below read the same scheduled harmony used by the host's ordinary note path. */
#define HB_CQ_CAPACITY 16
#define HB_CQ_HISTORY 65
typedef struct { unsigned long long word; int note; double at; } hb_cq_event;
typedef struct {
    int owner, count, history_count, seed, waiting, phrase_ready, phrase_held, cycle_count, cycle_position, running, timing, anticipation, result, paused;
    double last_beat;
    hb_cq_event pending[HB_CQ_CAPACITY], cycle[HB_CQ_CAPACITY], history[HB_CQ_HISTORY];
} hb_chord_queue;
static hb_chord_queue g_chord_queue;
/* Lifetime totals survive queue clears so an end-of-capture read can explain
   a vanished queue. No allocation, clock reads or logging in the audio path. */
static struct {
    unsigned long long services, activations, recoveries, late, clears, rearms;
    double max_late, last_due, last_actual;
} g_chord_queue_diagnostics;
static int hb_cq_owned(const Inst *instance){return g_chord_queue.owner==(int)(instance-g_pool)+1;}
static Inst *hb_cq_owner(void){
    int index=g_chord_queue.owner-1;
    return index>=0&&index<HB_MAX_INSTANCES&&g_pool[index].used&&g_pool[index].role==0?&g_pool[index]:0;
}
static int hb_cq_ready(void){return hb_cq_owner()&&!g_chord_queue.waiting&&g_chord_queue.count>0;}
static void hb_cq_clear(Inst *instance){if(!instance||hb_cq_owned(instance)){
    if(g_chord_queue.owner)g_chord_queue_diagnostics.clears++;
    memset(&g_chord_queue,0,sizeof(g_chord_queue));
}}
static int hb_selection_parse(const char *parameter,int *fields,int bank);
static int hb_selection_parse_register(const char *parameter,int *fields,int *range);
static unsigned long long hb_selection_with_register(unsigned long long word,int *note,int range);
static unsigned long long hb_selection_resolve(Inst *instance,const int *fields,int *source_note);
static double hb_cq_next_boundary(double after){
    double grid=hb_chord_grid_beats();
    if(grid>0)return hb_ft_next_boundary(after+1e-6,grid,hb_anticipation_beats());
    /* Observed uses the proven clip's changes. Without a proven timeline, use
       the existing one-bar musical fallback rather than inventing a cadence. */
    if(g_bus.next_model_locked&&g_bus.next_model_count>0){
        double phase=hb_next_phase(hb_clip_playhead()+after-hb_current_beat());
        int next=hb_next_upcoming_event(phase);
        if(next>=0){double distance=g_bus.next_model[next].phase-phase;
            if(distance<=1e-6)distance+=hb_next_loop_length();
            return after+distance;
        }
    }
    return hb_ft_next_boundary(after+1e-6,4.0,hb_anticipation_beats());
}
static void hb_cq_schedule(double after){
    for(int index=0;index<g_chord_queue.count;index++){
        after=hb_cq_next_boundary(after);g_chord_queue.pending[index].at=after;
    }
}
static void hb_cq_repeat(double after){
    memcpy(g_chord_queue.pending,g_chord_queue.cycle,(size_t)g_chord_queue.cycle_count*sizeof(hb_cq_event));
    g_chord_queue.count=g_chord_queue.cycle_count;g_chord_queue.waiting=0;
    if(g_chord_queue.running)hb_cq_schedule(after);
    else for(int index=0;index<g_chord_queue.count;index++)g_chord_queue.pending[index].at=-1;
}
/* Called by the host only at its existing sequencer boundary, never by UI
   redraws. Editing timing re-arms future entries, without changing the current. */
static void hb_cq_sync(void){
    if(!hb_cq_owner())return;
    double now=hb_current_beat();int running=hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING;
    if(g_chord_queue.running&&(!running||now+1e-6<g_chord_queue.last_beat)){
        if(!hb_cq_owner()->chord_queue_loop||!g_chord_queue.cycle_count){hb_cq_clear(0);return;}
        g_chord_queue.paused=1;g_chord_queue.running=0;
        for(int index=0;index<g_chord_queue.count;index++)g_chord_queue.pending[index].at=-1;
    }
    if(g_chord_queue.paused){
        if(!running)return;
        /* Transport Play starts the retained loop at its first chord. Do not
           restore a stopped voice, or fall through to a recorded clip loop. */
        g_chord_queue.running=1;hb_cq_repeat(now);
        g_chord_queue.seed=1;g_chord_queue.history_count=0;g_chord_queue.cycle_position=-1;
        double at=now;
        for(int index=0;index<g_chord_queue.count;index++){
            g_chord_queue.pending[index].at=at;at=hb_cq_next_boundary(at);
        }
        g_chord_queue.paused=0;g_chord_queue.last_beat=now;
        g_chord_queue.timing=g_bus.chord_timing;g_chord_queue.anticipation=g_bus.anticipation;
        g_chord_queue_diagnostics.rearms++;return;
    }
    if(running&&!g_chord_queue.running&&g_chord_queue.history_count){
        hb_cq_event current=g_chord_queue.history[g_chord_queue.history_count-1];current.at=now;
        g_chord_queue.history[0]=current;g_chord_queue.history_count=1;
    }
    int armed=0;
    if(hb_cq_owner()->chord_queue_loop&&g_chord_queue.waiting&&!g_chord_queue.count&&g_chord_queue.cycle_count){hb_cq_repeat(now);armed=1;}
    if(g_chord_queue.waiting&&!g_chord_queue.phrase_held&&g_chord_queue.count>0&&(g_chord_queue.phrase_ready||(hb_cq_owner()->chord_queue_depth>0&&g_chord_queue.count>=hb_cq_owner()->chord_queue_depth))){
        g_chord_queue.cycle_count=g_chord_queue.count;
        g_chord_queue.cycle_position=-1;
        memcpy(g_chord_queue.cycle,g_chord_queue.pending,(size_t)g_chord_queue.count*sizeof(hb_cq_event));
        g_chord_queue.waiting=0;g_chord_queue.phrase_ready=0;armed=1;
        if(!running&&g_chord_queue.seed)g_chord_queue.pending[0].at=now;
    }
    if(running&&!g_chord_queue.waiting&&(armed||!g_chord_queue.running||g_chord_queue.timing!=g_bus.chord_timing||g_chord_queue.anticipation!=g_bus.anticipation)){
        g_chord_queue_diagnostics.rearms++;hb_cq_schedule(now);
    }
    g_chord_queue.running=running;g_chord_queue.last_beat=now;
    g_chord_queue.timing=g_bus.chord_timing;g_chord_queue.anticipation=g_bus.anticipation;
}
/* A held entry gesture delays only the arming of a new batch. Running chords
   retain their clock; a connected touch phrase can override the refill depth. */
static void hb_cq_begin(Inst *instance){
    if(instance->role!=0||(hb_cq_owner()&&!hb_cq_owned(instance)))return;
    if(!hb_cq_owner()){
        hb_cq_clear(0);g_chord_queue.owner=(int)(instance-g_pool)+1;
        g_chord_queue.seed=1;g_chord_queue.waiting=1;g_chord_queue.cycle_position=-1;
        g_chord_queue.last_beat=hb_current_beat();
    }
    g_chord_queue.phrase_held=1;
}
static void hb_cq_enqueue(Inst *instance,const char *parameter){
    int fields[9],note=0,range=0;
    if(instance->role!=0||!hb_selection_parse_register(parameter,fields,&range))return;
    unsigned long long word=hb_selection_resolve(instance,fields,&note);if(!word)return;word=hb_selection_with_register(word,&note,range);
    if(hb_cq_owner()&&!hb_cq_owned(instance))return;
    if(!hb_cq_owner()){
        hb_cq_clear(0);g_chord_queue.owner=(int)(instance-g_pool)+1;
        g_chord_queue.seed=1;g_chord_queue.waiting=1;g_chord_queue.last_beat=hb_current_beat();
    }
    hb_cq_sync();
    if(!hb_cq_owner())return;
    if(g_chord_queue.count>=HB_CQ_CAPACITY||(instance->chord_queue_loop&&!g_chord_queue.waiting&&g_chord_queue.cycle_count>=HB_CQ_CAPACITY)){g_chord_queue.result=2;return;}
    double after=g_chord_queue.count?g_chord_queue.pending[g_chord_queue.count-1].at:hb_current_beat();
    double at=!g_chord_queue.waiting&&g_chord_queue.running?hb_cq_next_boundary(after):-1;
    g_chord_queue.pending[g_chord_queue.count++]=(hb_cq_event){word,note,at};
    if(!g_chord_queue.waiting&&g_chord_queue.cycle_count<HB_CQ_CAPACITY)g_chord_queue.cycle[g_chord_queue.cycle_count++]=g_chord_queue.pending[g_chord_queue.count-1];
    g_chord_queue.result=1;hb_cq_sync();
}
/* Consumptive bridge, like hb_recorded_actions: prepares exactly one ordinary
   input event. Movy records its activation, never the earlier enqueue gesture. */
static int hb_cq_take(Inst *instance,char *buffer,int length){
    hb_cq_sync();
    if(!hb_cq_owned(instance)||!hb_cq_owner())return snprintf(buffer,(size_t)length,"off");
    if(g_chord_queue.paused)return snprintf(buffer,(size_t)length,"--");
    g_chord_queue_diagnostics.services++;
    double now=hb_current_beat();
    int due=hb_cq_ready()&&(g_chord_queue.running||g_chord_queue.seed)&&
        g_chord_queue.pending[0].at>=0&&now+1e-6>=g_chord_queue.pending[0].at;
    /* Transport start and chord-state operations drain native voice ownership.
       Restore only between changes: replaying the old selection must never
       displace a chord already due at this sequencer boundary. */
    if(!due&&!g_chord_queue.seed&&g_chord_queue.history_count&&!hb_has_live_selection(instance)){
        g_chord_queue_diagnostics.recoveries++;
        hb_cq_event current=g_chord_queue.history[g_chord_queue.history_count-1];
        instance->selection_pending=current.word;instance->selection_note=current.note;
        return snprintf(buffer,(size_t)length,"%d",current.note);
    }
    if(!due)return snprintf(buffer,(size_t)length,"--");
    hb_cq_event event=g_chord_queue.pending[0];
    if(g_chord_queue.seed)event.at=hb_current_beat();
    g_chord_queue_diagnostics.activations++;
    g_chord_queue_diagnostics.last_due=event.at;
    g_chord_queue_diagnostics.last_actual=now;
    double late=now-event.at;
    if(late>g_chord_queue_diagnostics.max_late)g_chord_queue_diagnostics.max_late=late;
    /* Allow one 96-PPQN service tick; this is beat lateness, not wall time. */
    if(late>1.0/96.0+1e-6)g_chord_queue_diagnostics.late++;
    g_chord_queue.seed=0;
    for(int index=1;index<g_chord_queue.count;index++)g_chord_queue.pending[index-1]=g_chord_queue.pending[index];
    g_chord_queue.count--;
    g_chord_queue.cycle_position=g_chord_queue.cycle_count-g_chord_queue.count-1;
    if(!g_chord_queue.count)g_chord_queue.waiting=1;
    if(g_chord_queue.history_count==HB_CQ_HISTORY){
        memmove(g_chord_queue.history,g_chord_queue.history+1,(HB_CQ_HISTORY-1)*sizeof(hb_cq_event));g_chord_queue.history_count--;
    }
    g_chord_queue.history[g_chord_queue.history_count++]=event;
    if(!g_chord_queue.count&&instance->chord_queue_loop&&g_chord_queue.cycle_count)hb_cq_repeat(event.at);
    instance->selection_pending=event.word;instance->selection_note=event.note;
    return snprintf(buffer,(size_t)length,"%d",event.note);
}
static hb_harmony_t hb_cq_next(void){
    return hb_cq_ready()?hb_selection_decode(g_chord_queue.pending[0].word,g_chord_queue.pending[0].note):(hb_harmony_t){0};
}
static hb_harmony_t hb_cq_at(const Inst *instance,double beat,int shifted){
    hb_chord_queue *queue=&g_chord_queue;
    if(!hb_cq_owner()||!queue->history_count)return (hb_harmony_t){0};
    hb_cq_event selected=queue->history[queue->history_count-1];
    if(hb_clock_status()!=MOVE_CLOCK_STATUS_RUNNING)return hb_selection_decode(selected.word,selected.note);
    if(shifted&&(instance?instance->next_predict:g_bus.next_predict)&&hb_next_enabled_for(instance)){
        double delay=hb_next_after_beats_for(instance);
        if((hb_next_immediate_for(instance)||(delay>=0&&beat-selected.at>=delay))&&hb_cq_ready())return hb_cq_next();
        if(delay<0)beat+=hb_next_shift_beats_for(instance);
    }
    /* Most audio reads ask for now. Search backwards only for an explicitly
       delayed harmony, rather than rescanning a growing history every read. */
    if(selected.at>beat+1e-6){
        selected=queue->history[0];
        for(int index=queue->history_count-2;index>0;index--)
            if(queue->history[index].at<=beat+1e-6){selected=queue->history[index];break;}
    }
    for(int index=0;index<queue->count;index++){
        if(queue->pending[index].at<0||queue->pending[index].at>beat+1e-6)break;
        selected=queue->pending[index];
    }
    return hb_selection_decode(selected.word,selected.note);
}
static double hb_cq_boundary_for(const Inst *instance,double beat){
    if(!hb_cq_owner()||!g_chord_queue.running)return -1;
    double shift=(instance?instance->next_predict:g_bus.next_predict)?hb_next_shift_beats_for(instance):0;
    int predict=(instance?instance->next_predict:g_bus.next_predict);
    double delay=predict?hb_next_after_beats_for(instance):-1,best=1e99;
    if(predict&&hb_next_immediate_for(instance))return -1;
    if(delay>=0&&hb_cq_ready()&&g_chord_queue.history_count){
        double at=g_chord_queue.history[g_chord_queue.history_count-1].at+delay;
        if(at>=beat-1e-6)best=at;
    }
    for(int index=0;index<g_chord_queue.history_count+g_chord_queue.count;index++){
        hb_cq_event event=index<g_chord_queue.history_count?g_chord_queue.history[index]:g_chord_queue.pending[index-g_chord_queue.history_count];
        double at=event.at-shift;if(event.at>=0&&at>=beat-1e-6&&at<best)best=at;
    }
    return best<1e98?best:-1;
}
static int hb_cq_status(Inst *instance,char *buffer,int length){
    if(!hb_cq_owned(instance)||!hb_cq_owner())return snprintf(buffer,(size_t)length,"--");
    hb_harmony_t current=g_chord_queue.history_count?hb_selection_decode(g_chord_queue.history[g_chord_queue.history_count-1].word,g_chord_queue.history[g_chord_queue.history_count-1].note):(hb_harmony_t){0};
    hb_harmony_t next=g_chord_queue.count?hb_selection_decode(g_chord_queue.pending[0].word,g_chord_queue.pending[0].note):(hb_harmony_t){0};
    current=hb_key_harmony(instance,current);next=hb_key_harmony(instance,next);
    int cycle=instance->chord_queue_loop&&!g_chord_queue.waiting&&g_chord_queue.cycle_count;
    int count=cycle?g_chord_queue.cycle_count:g_chord_queue.count+(current.valid?1:0);
    int cursor=cycle?g_chord_queue.cycle_position:current.valid?0:-1;
    int used=snprintf(buffer,(size_t)length,"cq1|%d|%.6f|%s|%s|%d|%d|%d|%d|%d|%d",g_chord_queue.count,
        g_chord_queue.count?g_chord_queue.pending[0].at:-1,current.valid?current.name:"--",next.valid?next.name:"--",g_chord_queue.result,g_chord_queue.waiting,instance->chord_queue_depth,instance->chord_queue_loop,cursor,count);
    for(int index=0;index<count;index++){
        hb_harmony_t harmony;
        if(!cycle&&current.valid&&index==0)harmony=current;
        else {hb_cq_event event=cycle?g_chord_queue.cycle[index]:g_chord_queue.pending[index-(current.valid?1:0)];harmony=hb_key_harmony(instance,hb_selection_decode(event.word,event.note));}
        int available=used<length?length-used:0;
        used+=snprintf(available?buffer+used:0,(size_t)available,"|%s",harmony.valid?harmony.name:"--");
    }
    return used;
}
static int hb_cq_diagnostics(char *buffer,int length){
    return snprintf(buffer,(size_t)length,"qd1|%llu|%llu|%llu|%llu|%llu|%llu|%.6f|%.6f|%.6f|%d",
        g_chord_queue_diagnostics.services,g_chord_queue_diagnostics.activations,
        g_chord_queue_diagnostics.recoveries,g_chord_queue_diagnostics.late,
        g_chord_queue_diagnostics.clears,g_chord_queue_diagnostics.rearms,
        g_chord_queue_diagnostics.max_late,g_chord_queue_diagnostics.last_due,
        g_chord_queue_diagnostics.last_actual,g_chord_queue.owner);
}
