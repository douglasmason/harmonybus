/* Reuse the motif event renderer for motif steps; simple transformations keep
   the ordinary chord/arp/gate pipeline and its original input-note ownership. */
static int hb_ar_schedule(Inst *instance,int source,int shift,unsigned token,int velocity,int channel,int whole){
    hb_mt_phrase builtin;const hb_mt_phrase *phrase=hb_ar_phrase(token&63,&builtin);
    if(!phrase||!phrase->count)return 0;
    int step=(token>>6)&31;if(step>=phrase->count)return 0;
    hb_mt_runtime *runtime=&instance->motif;
    int old_target=runtime->tap_target,old_rhythm=runtime->tap_rhythm,old_span=runtime->tap_span;
    hb_harmony_t old_harmony=runtime->tap_harmony;
    hb_harmony_t harmony=hb_render_harmony(instance);
    int target=source+shift;
    if(target<0||target>127)return 0;
    runtime->tap_harmony=harmony;
    runtime->tap_target=instance->role==1?hb_map_follower_note_unoperated(instance,target):target+g_bus.global_transpose;
    runtime->tap_rhythm=hb_rr_pattern(instance);runtime->tap_span=g_motif_span;
    double now=hb_motion_position(instance),arrival=now;
    if(whole)for(int index=0;index<phrase->anchor;index++)arrival+=hb_mt_duration(phrase,index,runtime->tap_rhythm,runtime->tap_span);
    /* A spatial row is an independently held note, not a timed motif launch.
       Capture only the new voices so releasing it cannot cancel another pad. */
    uint8_t occupied[HB_MT_SCHEDULE];
    for(int index=0;index<HB_MT_SCHEDULE;index++)occupied[index]=runtime->events[index].used;
    int result=hb_mt_schedule(instance,phrase,source,velocity,channel,whole?0:step,whole?phrase->count:step+1,arrival,whole?-1:now,1);
    if(result&&shift&&!whole)for(int index=0;index<HB_MT_SCHEDULE;index++){
        hb_mt_scheduled *event=&runtime->events[index];
        if(!occupied[index]&&event->used){event->pad_owner=source+1;event->pad_playback=instance->movy_playback;event->off=1e30;}
    }
    runtime->tap_target=old_target;runtime->tap_rhythm=old_rhythm;runtime->tap_span=old_span;runtime->tap_harmony=old_harmony;
    return result;
}
static int hb_ar_input(Inst *instance,const uint8_t *input,int length){
    if(length<3||instance->role>=2)return 0;
    int type=input[0]&0xf0,source=input[1]&127,channel=input[0]&15;
    int on=type==0x90&&input[2],off=type==0x80||(type==0x90&&!input[2]);
    hb_ar_state *state=&instance->approach_rows;
    if(off&&state->swallow[channel][source]){
        state->swallow[channel][source]--;
        for(int index=0;index<HB_MT_SCHEDULE;index++){
            hb_mt_scheduled *event=&instance->motif.events[index];
            if(event->used&&event->pad_owner==source+1&&event->channel==channel&&event->pad_playback==instance->movy_playback)event->off=-1;
        }
        return 1;
    }
    if(!on||!hb_source_channel_matches(instance,channel)||instance->motif.editor.recording>=0)return 0;
    if(!instance->movy_playback){
        int spatial=instance->movy_pad_pending==source+1&&hb_approach_pad_enabled(instance);
        hb_ar_pad_press(state,source,spatial?(state->pending_row<0?3:state->pending_row):-1,
            spatial?instance->movy_pad_pending_shift:0);
    }
    unsigned token=0;int shift=0,whole=0;
    if(instance->movy_playback&&instance->recorded_action_valid[source]){
        unsigned long long word=instance->recorded_actions[source][HB_MOTION_LANES];token=(word>>HB_AR_SHIFT)&2047;
        shift=hb_ar_alias_shift(word);
        whole=!!(word&(1ULL<<54));
    }else if(!instance->movy_playback&&(state->enabled||instance->surface_enabled||instance->movy_pad_pending==source+1)){
        if(instance->movy_pad_pending==source+1&&hb_approach_pad_enabled(instance)){
            token=hb_ar_live_peek(state,state->pending_row<0?3:state->pending_row);shift=instance->movy_pad_pending_shift;
        }else if(state->bank_armed>=0){token=15+state->bank[state->bank_armed];whole=1;}else token=hb_ar_live_peek(state,-1);
    }
    if(((token&63)<16&&(token&63)!=15)||(token&63)>=59)return 0;
    hb_ar_schedule(instance,source,shift,token,input[2],channel,whole);
    if(state->swallow[channel][source]<255)state->swallow[channel][source]++;
    if(!instance->movy_playback){
        if(!instance->synthetic_advance&&instance->action_count<64){
            int slot=(instance->action_head+instance->action_count)%64;
            instance->movy_pad_shift[source]=shift;state->tokens[source]=token;
            hb_capture_input_intent(instance,source,instance->action_queue[slot]);
            if(whole)instance->action_queue[slot][HB_MOTION_LANES]|=1ULL<<54;
            instance->action_pitch[slot]=source;instance->action_count++;
        }
        if(whole)state->bank_armed=-1;else hb_ar_live_advance(state,shift?(state->pending_row<0?3:state->pending_row):-1);
    }
    instance->movy_pad_pending=0;state->pending_row=-1;return 1;
}
static unsigned hb_ar_preview(Inst *preview,int source,unsigned token,int root_only,unsigned long long *low,unsigned long long *high){
    memset(preview->motif.events,0,sizeof(preview->motif.events));preview->motif.pending=0;preview->motif.cancel=0;
    hb_cp_config config=preview->player.config;if(root_only)preview->player.config.mode=0;
    int valid=hb_ar_schedule(preview,source,preview->movy_pad_shift[source],token,100,0,0);
    preview->player.config=config;
    unsigned mask=0;preview->preview_single_low=preview->preview_single_high=0;
    if(valid)for(int index=0;index<HB_MT_SCHEDULE;index++)if(preview->motif.events[index].used){
        if(preview->preview_target<0&&preview->motif.events[index].trail_target)preview->preview_target=preview->motif.events[index].trail_target-1;
        int pitch=preview->motif.events[index].pitch;mask|=1u<<mod12(pitch);
        if(pitch<64)preview->preview_single_low|=1ULL<<pitch;else preview->preview_single_high|=1ULL<<(pitch-64);
    }
    if(low)*low=preview->preview_single_low;if(high)*high=preview->preview_single_high;
    hb_harmony_t harmony=hb_render_harmony(preview);preview->preview_gap_mask=hb_follower_scale_target(preview,harmony).pitch_mask;
    return mask;
}
