/* One register policy for conductor selection and generated follower chords.
   Updating a range edits existing owners; it is never a new queued attack. */
static int hb_register_top(Inst *instance){
    int range=hb_chord_register_for(instance,hb_render_harmony(instance));
    if(range>=1000)return range-1000;
    int top=-1;
    for(int slot=0;slot<HB_CP_KEYS;slot++){
        const hb_cp_key *key=&instance->player.keys[slot];
        if(key->used)for(int voice=0;voice<key->count;voice++)if(key->notes[voice]>top)top=key->notes[voice];
    }
    return top>=0?top:hb_cp_clamp(67+12*range,0,127);
}
static unsigned hb_register_mask(Inst *instance){
    unsigned mask=0;
    for(int slot=0;slot<HB_CP_KEYS;slot++){
        const hb_cp_key *key=&instance->player.keys[slot];
        if(key->used)for(int voice=0;voice<key->count;voice++)mask|=1u<<mod12(key->notes[voice]);
    }
    return mask?mask:hb_harmony_chord_mask(hb_render_harmony(instance));
}
/* Retune an existing attack without resetting its gate, strum or arp clock. */
static void hb_register_retarget(hb_chord_player *player,hb_cp_key *key,const hb_cp_key *before,int arp_note,int *arp_changed){
    if(!before->used||!key->used||before->source!=key->source||before->channel!=key->channel)return;
    key->fresh=before->fresh;key->started=before->started;
    memcpy(key->due,before->due,sizeof(key->due));
    for(int voice=0;voice<before->count;voice++){
        int replacement=key->count?key->notes[voice<key->count?voice:key->count-1]:before->notes[voice];
        if(!*arp_changed&&player->arp_channel==key->channel&&arp_note==before->notes[voice]){player->arp_note=replacement;*arp_changed=1;}
        for(int hit=0;hit<key->rhythm_count;hit++)if(before->rhythm_notes[hit]==before->notes[voice])key->rhythm_notes[hit]=replacement;
    }
}
static void hb_register_refresh(void){
    /* Publish the conductor's new range before resolving follower defaults. */
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *instance=&g_pool[index];if(!instance->used)continue;
        int arp_note=instance->player.arp_note,arp_changed=0;
        for(int slot=0;slot<HB_CP_KEYS;slot++){
            hb_cp_key *key=&instance->player.keys[slot];if(!key->used||!key->selection)continue;
            hb_cp_key before=*key;
            hb_harmony_t harmony=hb_selection_decode(key->selection_word,key->selection_pitch);
            hb_cp_config config=hb_chord_config_at(instance,key->source,hb_motion_position(instance),hb_motion_condition_position());
            config.mode=2;if(key->selection_word&HB_SELECTION_REGISTER){config.register_octaves=0;config.top_note=hb_selection_top(key->selection_word)+1;}
            if(instance->render_channel>=0){
                key->count=hb_cp_voice_semantic(config,key->selection_pitch+g_bus.global_transpose,harmony.root_pc,harmony.pitch_mask,harmony.intent_scale,key->notes,0);
                for(int voice=0;voice<key->count;voice++)key->notes[voice]=hb_key_pitch(instance,key->notes[voice]);
        if(config.top_note)key->count=hb_cp_range_top(key->notes,key->count,config.top_note-1,mod12(hb_key_pitch(instance,60+harmony.root_pc)),config.voicing);
            }
            key->onset_config=config;
            hb_register_retarget(&instance->player,key,&before,arp_note,&arp_changed);
        }
    }
    hb_harmony_t current=hb_live_current();if(current.valid)hb_effective_write(current);
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *instance=&g_pool[index];if(!instance->used||(instance->role!=0&&instance->role!=1)||hb_cp_mode(&instance->player)<=0)continue;
        hb_chord_player *player=&instance->player;
        hb_harmony_t harmony=hb_render_harmony(instance);int target=hb_chord_register_for(instance,harmony),changed=0;
        for(int slot=0;slot<HB_CP_KEYS;slot++){
            hb_cp_key *key=&player->keys[slot];
            if(key->used&&!key->selection&&key->onset_config.mode&&
               (key->onset_config.register_octaves!=(target>=1000?0:target)||key->onset_config.top_note!=(target>=1000?target-999:0)))changed=1;
        }
        if(!changed)continue;
        hb_cp_key before[HB_CP_KEYS];memcpy(before,player->keys,sizeof(before));
        uint8_t retrigger[16][128];memcpy(retrigger,player->retrigger,sizeof(retrigger));
        int held=instance->retrigger_held,running=player->running,step=player->order_state.step,arp_note=player->arp_note,arp_changed=0;
        instance->play_revision++;instance->retrigger_held=1;
        hb_reharmonize_held_chords(instance);
        instance->retrigger_held=held;player->running=running;player->order_state.step=step;
        memcpy(player->retrigger,retrigger,sizeof(retrigger));
        for(int slot=0;slot<HB_CP_KEYS;slot++)hb_register_retarget(player,&player->keys[slot],&before[slot],arp_note,&arp_changed);
    }
}
static int hb_register_matches(unsigned long long word,int note,unsigned long long candidate,int pitch){
    return (hb_selection_base_word(word)&~HB_SELECTION_REGISTER)==(hb_selection_base_word(candidate)&~HB_SELECTION_REGISTER)&&mod12(note)==mod12(pitch);
}
static void hb_selection_range_edit(Inst *instance,const char *parameter){
    int fields[9],range=0,note=0;
    if(instance->role!=0||!hb_selection_parse_register(parameter,fields,&range))return;
    unsigned long long word=hb_selection_resolve(instance,fields,&note);if(!word)return;
    word=hb_selection_with_register(word,&note,range);
    if(hb_cq_owned(instance)){
        for(int group=0;group<3;group++){
            hb_cq_event *events=group==0?g_chord_queue.pending:group==1?g_chord_queue.cycle:g_chord_queue.history;
            int count=group==0?g_chord_queue.count:group==1?g_chord_queue.cycle_count:g_chord_queue.history_count;
            /* History is an immutable timeline except its currently held chord. */
            for(int i=group==2?count-1:0;i<count&&i>=0;i++)if(hb_register_matches(word,note,events[i].word,events[i].note)){
                events[i].word=word;events[i].note=note;
            }
        }
    }
    for(int slot=0;slot<HB_CP_KEYS;slot++){
        hb_cp_key *key=&instance->player.keys[slot];
        if(key->used&&key->selection&&!key->playback_origin&&hb_register_matches(word,note,key->selection_word,key->selection_pitch)){
            key->selection_word=word;key->selection_pitch=note;
        }
    }
    hb_register_refresh();
}
