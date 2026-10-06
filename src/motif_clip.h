#ifndef HB_MOTIF_CLIP_H
#define HB_MOTIF_CLIP_H
/* Clip construction preserves motif events verbatim. This layer only places
   events on a timeline; the ordinary note renderer still owns harmony. */
#define HB_MC_EVENTS (HB_MT_STEPS+2)
enum { HB_MC_END, HB_MC_START, HB_MC_BOTH, HB_MC_OMIT };
typedef struct {
    hb_mt_event event;
    int source_step; /* -1: inserted target, -2: padding rest */
    int onset,duration;
} hb_mc_event;
typedef struct {
    int count,duration,target_first,target_last;
    hb_mc_event events[HB_MC_EVENTS];
} hb_mc_plan;

static int hb_mc_append(hb_mc_plan *plan,const hb_mt_event *event,int source_step,int duration){
    if(plan->count>=HB_MC_EVENTS||duration<1)return 0;
    plan->events[plan->count++]=(hb_mc_event){.event=*event,.source_step=source_step,.duration=duration};
    return 1;
}
static int hb_mc_duration(const hb_mt_event *event,int unit,int rhythm){
    if(event->duration<1||event->duration>HB_MT_STEPS*1536)return 0;
    return rhythm?unit:(event->duration*unit+12)/24;
}
/* Placement adds targets around the canonical body. Its reference remains
   separate even when neither target sounds. Times use 1/24-beat units. */
static int hb_mc_build(const hb_mt_definition *definition,int placement,int unit,int rhythm,
                       int pad_even,hb_mc_plan *output){
    if(!definition||!output||definition->body.count<0||definition->body.count>HB_MT_STEPS||
       placement<HB_MC_END||placement>HB_MC_OMIT||unit<1||unit>1536||rhythm<0||rhythm>5)return 0;
    const hb_mt_phrase *phrase=&definition->body;
    hb_mc_plan plan={.target_first=-1,.target_last=-1};
    if(placement==HB_MC_START||placement==HB_MC_BOTH){
        if(definition->target_start.kind||definition->target_start.count<1)return 0;
        plan.target_first=plan.count;
        if(!hb_mc_append(&plan,&definition->target_start,-1,hb_mc_duration(&definition->target_start,unit,rhythm)))return 0;
    }
    int previous_body=-1;
    for(int step=0;step<phrase->count;step++){
        const hb_mt_event *event=&phrase->events[step];
        int duration=hb_mc_duration(event,unit,rhythm);if(duration<1)return 0;
        if(event->kind==2){
            if(previous_body<0||plan.events[previous_body].event.kind)return 0;
            plan.events[previous_body].duration+=duration;
        }else {
            if(event->kind<0||event->kind>1||(!event->kind&&(event->count<1||event->count>HB_MT_VOICES)))return 0;
            previous_body=plan.count;
            if(!hb_mc_append(&plan,event,step,duration))return 0;
        }
    }
    if(placement==HB_MC_END||placement==HB_MC_BOTH){
        if(definition->target_end.kind||definition->target_end.count<1)return 0;
        plan.target_last=plan.count;
        if(!hb_mc_append(&plan,&definition->target_end,-1,hb_mc_duration(&definition->target_end,unit,rhythm)))return 0;
    }
    if(!plan.count)return 0;
    /* Reuse motif rhythm weights. Quantize cumulative boundaries, retaining
       the exact total duration instead of accumulating per-note rounding. */
    int total=0;double weights=0;
    for(int index=0;index<plan.count;index++){
        total+=plan.events[index].duration;
        weights+=hb_mt_weight(rhythm,index,plan.count)*plan.events[index].duration;
    }
    if(rhythm>=2){
        double cumulative=0;int previous=0;
        for(int index=0;index<plan.count;index++){
            cumulative+=hb_mt_weight(rhythm,index,plan.count)*plan.events[index].duration;
            int boundary=index==plan.count-1?total:(int)(total*cumulative/weights+0.5);
            if(boundary<=previous)return 0;
            plan.events[index].duration=boundary-previous;previous=boundary;
        }
    }
    int padding=(2*unit-total%(2*unit))%(2*unit);
    if(pad_even&&padding){
        int target_index=plan.target_last>=0?plan.target_last:plan.target_first;
        if(target_index>=0)plan.events[target_index].duration+=padding;
        else {
            hb_mt_event rest={.kind=1,.duration=padding};
            if(!hb_mc_append(&plan,&rest,-2,padding))return 0;
        }
    }
    for(int index=0;index<plan.count;index++){
        plan.events[index].onset=plan.duration;
        plan.duration+=plan.events[index].duration;
    }
    *output=plan;return 1;
}
#endif
