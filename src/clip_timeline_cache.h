#ifndef HB_CLIP_TIMELINE_CACHE_H
#define HB_CLIP_TIMELINE_CACHE_H
/* Individual learned conductor timelines. Composition uses the same ordered
   last-commit-wins rule as the conductor batch; no combination cache required. */
#define HB_TIMELINE_SLOTS 128 /* All 16 tracks x 8 clip slots. */
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
static unsigned g_timeline_generation;
static double hb_timeline_abs(double value){return value<0?-value:value;}
static hb_tick_t hb_timeline_rendering(Inst *instance){
    hb_tick_t hash=14695981039346656037ULL;
    int globals[]={hb_shared_follower_scale(),hb_shared_dominant_scale(),hb_shared_borrowed_scale(),
        hb_global_root_policy(),hb_global_explicit_root(),g_bus.global_input_root};
    for(unsigned index=0;index<sizeof(globals)/sizeof(globals[0]);index++)hash=hb_clip_hash(hash,(unsigned)globals[index]);
    for(int field=0;field<HB_POLICY_FIELDS;field++)hash=hb_clip_hash(hash,(unsigned)hb_policy_value((Inst*)instance,field));
    const unsigned char *bytes=(const unsigned char *)&instance->player.config;
    for(unsigned index=0;index<sizeof(instance->player.config);index++)hash=hb_clip_hash(hash,bytes[index]);
    /* Revision counters restart with the process. Hash the saved settings. */
    hb_tick_t revisions=14695981039346656037ULL;
    for(int index=0;index<HB_MOTION_LANES;index++)revisions=hb_clip_hash(revisions,instance->motion.revision[index]);
    if(!instance->timeline_lane_cached||instance->timeline_lane_revision!=revisions){
        hb_tick_t settings=14695981039346656037ULL;
        bytes=(const unsigned char *)instance->motion.lanes;
        for(unsigned index=0;index<sizeof(instance->motion.lanes);index++)settings=hb_clip_hash(settings,bytes[index]);
        instance->timeline_lane_hash=settings;instance->timeline_lane_revision=revisions;instance->timeline_lane_cached=1;
    }
    hash=hb_clip_hash(hash,instance->timeline_lane_hash);
    bytes=(const unsigned char *)&instance->play;
    for(unsigned index=0;index<sizeof(instance->play);index++)hash=hb_clip_hash(hash,bytes[index]);
    for(int index=0;index<4;index++)hash=hb_clip_hash(hash,(unsigned)instance->target_scale_policy[index]);
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
    entry->ready=entry->count=0;owner->progress=0;g_timeline_generation++;
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
            if(entry->used&&entry->track==track&&entry->slot==clip->slot&&
               (entry->revision!=clip->revision||entry->period!=period||entry->rendering!=rendering||
                entry->configuration!=hb_next_configuration()))entry->used=0;
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
/* Clip metadata is published before the conductor MIDI batch for that tick.
   Finalizing inside hb_timeline_refresh could therefore declare a traversal
   complete just before the wrap chord is heard. If startup missed the first
   chord, that incomplete model contradicted itself immediately and forced a
   second learning pass. Finalize only after every conductor has consumed the
   boundary batch. */
static void hb_timeline_finalize(void){
    int changed=0;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        hb_clip_timeline_owner *owner=&g_timeline_owners[index];
        if(!owner->entry)continue;
        hb_clip_timeline *entry=&g_timelines[owner->entry-1];
        if(!entry->ready&&entry->count&&owner->progress>=entry->period){entry->ready=1;changed=1;}
    }
    if(changed){g_timeline_generation++;g_timeline_dirty=0;hb_timeline_compose();}
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
/* Persist only the latest proven rendering of each slot, on its own track.
   Text is versioned and never depends on struct layout or memory addresses. */
static int hb_timeline_save(Inst *instance,char *buffer,int length,int used){
    if(instance->movy_track<0)return used;
    for(int index=0;index<HB_TIMELINE_SLOTS;index++){
        const hb_clip_timeline *entry=&g_timelines[index];
        if(!entry->used||!entry->ready||entry->track!=instance->movy_track)continue;
        int newer=0;
        for(int other=0;other<HB_TIMELINE_SLOTS;other++)if(g_timelines[other].used&&
            g_timelines[other].ready&&g_timelines[other].track==entry->track&&
            g_timelines[other].slot==entry->slot&&g_timelines[other].age>entry->age)newer=1;
        if(newer)continue;
        if(used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),";tl1,%d,%d,%llu,%llu,%llu,%u,%d",
            entry->track,entry->slot,entry->revision,entry->period,entry->rendering,entry->configuration,entry->count);
        for(int event=0;event<entry->count;event++){
            if(used<0||used>=length)return -1;
            const hb_loop_harmony_event_t *value=&entry->events[event];const hb_harmony_t *h=&value->harmony;
            used+=snprintf(buffer+used,(size_t)(length-used),":%.9g,%d,%d,%u,%u,%d,%d,%d,%d,%d,%u",
                value->phase,h->root_pc,h->bass_pc,h->pitch_mask,h->detected_mask,h->chord_index,
                h->confidence,h->intent_kind,h->intent_target,h->intent_minor,h->intent_scale);
        }
    }
    return used<length?used:-1;
}
static void hb_timeline_forget_track(const Inst *instance){
    if(instance->movy_track<0)return;
    for(int index=0;index<HB_TIMELINE_SLOTS;index++)if(g_timelines[index].used&&g_timelines[index].track==instance->movy_track){
        g_timelines[index].used=0;
        for(int owner=0;owner<HB_MAX_INSTANCES;owner++)if(g_timeline_owners[owner].entry==index+1)g_timeline_owners[owner].entry=0;
    }
    g_timeline_dirty=1;
}
static void hb_timeline_restore(const char *state){
    const char *cursor=state;
    while((cursor=strstr(cursor,";tl1,"))){
        hb_clip_timeline entry;memset(&entry,0,sizeof(entry));int consumed=0;
        int fields=sscanf(cursor,";tl1,%d,%d,%llu,%llu,%llu,%u,%d%n",&entry.track,&entry.slot,
            &entry.revision,&entry.period,&entry.rendering,&entry.configuration,&entry.count,&consumed);
        cursor+=5;
        if(fields!=7||entry.track<0||entry.track>=16||entry.slot<0||entry.slot>=8||!entry.period||
           entry.count<1||entry.count>HB_MAX_LOOP_HARMONIES)continue;
        const char *record=cursor-5+consumed;int valid=1;
        for(int index=0;index<entry.count;index++){
            hb_loop_harmony_event_t *event=&entry.events[index];hb_harmony_t *h=&event->harmony;
            unsigned pitch=0,detected=0,scale=0;consumed=0;
            fields=sscanf(record,":%lf,%d,%d,%u,%u,%d,%d,%d,%d,%d,%u%n",&event->phase,
                &h->root_pc,&h->bass_pc,&pitch,&detected,&h->chord_index,&h->confidence,
                &h->intent_kind,&h->intent_target,&h->intent_minor,&scale,&consumed);
            if(fields!=11||!(event->phase>=0&&event->phase<(double)entry.period)||
               (index&&event->phase<=entry.events[index-1].phase)||h->root_pc<0||h->root_pc>11||
               h->bass_pc<0||h->bass_pc>11||!pitch||pitch>4095||detected>4095||scale>4095||
               h->chord_index<0||h->chord_index>511||h->intent_kind<0||h->intent_kind>7||
               h->intent_target<0||h->intent_target>11||h->intent_minor<0||h->intent_minor>1){valid=0;break;}
            h->valid=1;h->pitch_mask=(uint16_t)pitch;h->detected_mask=(uint16_t)detected;h->intent_scale=(uint16_t)scale;
            *h=hb_transpose_harmony(*h,12); /* Rebuild the display name. */
            record+=consumed;
        }
        if(!valid||(*record&&*record!=';'))continue;
        int target=0;
        for(int index=0;index<HB_TIMELINE_SLOTS;index++){
            if(!g_timelines[index].used||(g_timelines[index].track==entry.track&&g_timelines[index].slot==entry.slot)){target=index;break;}
            if(g_timelines[index].age<g_timelines[target].age)target=index;
        }
        entry.used=entry.ready=1;entry.age=++g_timeline_age;g_timelines[target]=entry;
        g_timeline_dirty=1;cursor=record;
    }
}
#endif
