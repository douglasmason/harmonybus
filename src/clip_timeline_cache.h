#ifndef HB_CLIP_TIMELINE_CACHE_H
#define HB_CLIP_TIMELINE_CACHE_H
/* Individual learned conductor timelines. Composition uses the same ordered
   last-commit-wins rule as the conductor batch; no combination cache required. */
#define HB_TIMELINE_SLOTS 32
typedef struct {
    int used,ready,count,track,slot;
    unsigned configuration;
    hb_tick_t revision,period,rendering;
    unsigned long long age;
    hb_loop_harmony_event_t events[HB_MAX_LOOP_HARMONIES];
} hb_clip_timeline;
typedef struct {
    int entry; /* one-based, zero is inactive */
    hb_tick_t tick,origin,progress;
    double activated;
    hb_harmony_t observed;
} hb_clip_timeline_owner;
static hb_clip_timeline g_timelines[HB_TIMELINE_SLOTS];
static hb_clip_timeline_owner g_timeline_owners[HB_MAX_INSTANCES];
static unsigned long long g_timeline_age;
static int g_timeline_dirty;
static double hb_timeline_abs(double value){return value<0?-value:value;}
static hb_tick_t hb_timeline_rendering(const Inst *instance){
    hb_tick_t hash=14695981039346656037ULL;
    int globals[]={hb_shared_follower_scale(),hb_shared_dominant_scale(),hb_shared_borrowed_scale(),
        hb_global_root_policy(),hb_global_explicit_root(),g_bus.global_input_root};
    for(unsigned index=0;index<sizeof(globals)/sizeof(globals[0]);index++)hash=hb_clip_hash(hash,(unsigned)globals[index]);
    const unsigned char *bytes=(const unsigned char *)&instance->player.config;
    for(unsigned index=0;index<sizeof(instance->player.config);index++)hash=hb_clip_hash(hash,bytes[index]);
    for(int index=0;index<HB_MOTION_LANES;index++)hash=hb_clip_hash(hash,instance->motion.revision[index]);
    hash=hb_clip_hash(hash,instance->motion.held);hash=hb_clip_hash(hash,instance->motion.bypass);
    return hash;
}
static int hb_timeline_expected(const hb_clip_timeline *entry,double phase,double *age){
    int best=-1;*age=1e99;
    double period=(double)entry->period/g_movy_ppqn;
    for(int index=0;index<entry->count;index++){
        double distance=phase-entry->events[index].phase;if(distance<0)distance+=period;
        if(distance<*age){*age=distance;best=index;}
    }
    return best;
}
static double hb_timeline_phase(const hb_movy_clip_t *clip,hb_tick_t period){
    return (double)((clip->tick%period+period-clip->origin%period)%period)/clip->ppqn;
}
static void hb_timeline_invalidate(int index){
    hb_clip_timeline_owner *owner=&g_timeline_owners[index];
    if(!owner->entry)return;
    hb_clip_timeline *entry=&g_timelines[owner->entry-1];
    entry->ready=entry->count=0;owner->progress=0;
    owner->activated=hb_next_transport_beat();
    hb_clip_cache_evict_active();hb_next_reset_knowledge();g_timeline_dirty=0;
}
static void hb_timeline_reset_active(void){
    for(int index=0;index<HB_MAX_INSTANCES;index++)hb_timeline_invalidate(index);
}
static void hb_timeline_compose(void){
    hb_loop_harmony_event_t composed[HB_MAX_LOOP_HARMONIES];int count=0,active=0;
    if(g_movy_blocked||!g_movy_period)return;
    double length=(double)g_movy_period/g_movy_ppqn;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        hb_clip_timeline_owner *owner=&g_timeline_owners[index];
        if(!owner->entry)continue;
        hb_clip_timeline *entry=&g_timelines[owner->entry-1];
        if(!entry->ready)return;
        active++;
        hb_movy_clip_t *clip=&g_movy_clips[index];
        double period=(double)entry->period/clip->ppqn;
        double origin=(double)((clip->origin%entry->period+entry->period-g_movy_origin%entry->period)%entry->period)/clip->ppqn;
        for(int event=0;event<entry->count;event++)for(double repeat=0;repeat<length-1e-6;repeat+=period){
            hb_loop_harmony_event_t value=entry->events[event];
            value.phase+=origin+repeat;while(value.phase>=length)value.phase-=length;
            int at=0;while(at<count&&composed[at].phase<value.phase-1e-6)at++;
            if(at<count&&hb_timeline_abs(composed[at].phase-value.phase)<1e-6){composed[at]=value;continue;}
            if(count>=HB_MAX_LOOP_HARMONIES)return;
            for(int move=count;move>at;move--)composed[move]=composed[move-1];
            composed[at]=value;count++;
        }
    }
    if(!active||!count)return;
    g_bus.next_model_count=count;
    memcpy(g_bus.next_model,composed,(size_t)count*sizeof(composed[0]));
    g_bus.next_model_locked=1;g_infer_revision++;
}
static void hb_timeline_refresh(int configuration_changed,int restart){
    if(!g_movy_present)return;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *instance=&g_pool[index];hb_movy_clip_t *clip=&g_movy_clips[index];
        hb_clip_timeline_owner *owner=&g_timeline_owners[index];
        if(!instance->used||instance->role!=0||!clip->present||clip->active!=1||clip->slot<0){
            if(owner->entry){memset(owner,0,sizeof(*owner));g_timeline_dirty=1;}continue;
        }
        int track=instance->movy_track>=0?instance->movy_track:index,target=-1;
        hb_tick_t rendering=hb_timeline_rendering(instance);
        hb_tick_t period=hb_conductor_prediction_period(instance,clip);
        if(!period){owner->entry=0;continue;}
        for(int operation=0;operation<HB_MOTION_LANES;operation++)
            if(instance->motion.lanes[operation].operation==HB_MO_CHORD_FORM&&hb_mo_lane_active(&instance->motion,operation))
                rendering=hb_clip_hash(rendering,clip->origin%period);
        for(int slot=0;slot<HB_TIMELINE_SLOTS;slot++){
            hb_clip_timeline *entry=&g_timelines[slot];
            if(entry->used&&entry->track==track&&entry->slot==clip->slot&&entry->revision!=clip->revision)entry->used=0;
            if(entry->used&&entry->track==track&&entry->slot==clip->slot&&entry->revision==clip->revision&&entry->period==period&&
               entry->rendering==rendering&&entry->configuration==hb_next_configuration())target=slot;
        }
        if(target<0){
            target=0;
            for(int slot=0;slot<HB_TIMELINE_SLOTS;slot++){
                if(!g_timelines[slot].used){target=slot;break;}
                if(g_timelines[slot].age<g_timelines[target].age)target=slot;
            }
            hb_clip_timeline *entry=&g_timelines[target];memset(entry,0,sizeof(*entry));
            entry->used=1;entry->track=track;entry->slot=clip->slot;entry->revision=clip->revision;
            entry->period=period;entry->rendering=rendering;entry->configuration=hb_next_configuration();
            /* An evicted entry can never remain owned by an inactive instance. */
            for(int other=0;other<HB_MAX_INSTANCES;other++)if(g_timeline_owners[other].entry==target+1)g_timeline_owners[other].entry=0;
        }
        hb_clip_timeline *entry=&g_timelines[target];entry->age=++g_timeline_age;
        if(owner->entry!=target+1||owner->origin!=clip->origin||restart){
            memset(owner,0,sizeof(*owner));owner->entry=target+1;owner->origin=clip->origin;
            owner->tick=clip->tick;owner->activated=hb_next_transport_beat();g_timeline_dirty=1;
            if(!entry->ready)entry->count=0;
        }
        if(clip->running&&clip->tick>=owner->tick&&clip->tick-owner->tick<=clip->period)owner->progress+=clip->tick-owner->tick;
        owner->tick=clip->tick;
        if(!entry->ready&&entry->count&&owner->progress>=entry->period){entry->ready=1;g_timeline_dirty=1;}
        if(entry->ready&&clip->running){
            double age;int expected=hb_timeline_expected(entry,hb_timeline_phase(clip,entry->period),&age);
            double bpm=(g_host&&g_host->get_bpm)?g_host->get_bpm():120;
            double grace=(g_bus.inference_window_ms+25)*(bpm>0?bpm:120)/60000.0;if(grace<0.125)grace=0.125;
            if(expected>=0&&age>grace&&hb_next_transport_beat()-age>=owner->activated&&
               !hb_harmony_equal_effective(entry->events[expected].harmony,owner->observed))hb_timeline_invalidate(index);
        }
    }
    if(configuration_changed)g_timeline_dirty=1;
    if(g_timeline_dirty){g_timeline_dirty=0;hb_timeline_compose();}
}
static void hb_timeline_observe(Inst *instance,hb_harmony_t harmony){
    int index=(int)(instance-g_pool);if(index<0||index>=HB_MAX_INSTANCES||!hb_next_is_harmony(harmony))return;
    hb_clip_timeline_owner *owner=&g_timeline_owners[index];if(!owner->entry)return;
    hb_clip_timeline *entry=&g_timelines[owner->entry-1];
    hb_movy_clip_t *clip=&g_movy_clips[index];if(!clip->running)return;
    owner->observed=harmony;
    double phase=hb_timeline_phase(clip,entry->period);
    if(entry->ready){
        double age;int expected=hb_timeline_expected(entry,phase,&age);
        int matches=expected>=0&&hb_harmony_equal_effective(entry->events[expected].harmony,harmony);
        for(int event=0;!matches&&event<entry->count;event++){
            double distance=entry->events[event].phase-phase;if(distance<0)distance+=(double)entry->period/clip->ppqn;
            if(distance<=0.125&&hb_harmony_equal_effective(entry->events[event].harmony,harmony))matches=1;
        }
        if(matches)return;
        hb_timeline_invalidate(index);
    }
    /* Store the same grid/anticipation boundary shown by Chord Timing. */
    phase=hb_next_record_phase()+((double)g_movy_origin-(double)clip->origin)/clip->ppqn;
    double period=(double)entry->period/clip->ppqn;
    while(phase<0)phase+=period;while(phase>=period)phase-=period;
    int at=0;while(at<entry->count&&entry->events[at].phase<phase-1e-6)at++;
    if(at<entry->count&&hb_timeline_abs(entry->events[at].phase-phase)<1e-6){entry->events[at].harmony=harmony;return;}
    if(entry->count>=HB_MAX_LOOP_HARMONIES){entry->used=0;owner->entry=0;return;}
    for(int move=entry->count;move>at;move--)entry->events[move]=entry->events[move-1];
    entry->events[at]=(hb_loop_harmony_event_t){.phase=phase,.harmony=harmony};entry->count++;
}
#endif
