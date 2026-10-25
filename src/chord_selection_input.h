/* Selection supplies explicit harmony to the existing conductor player.
   Render Off retains silent harmony control for independently triggered followers. */
static hb_harmony_t hb_selection_decode(unsigned long long word,int note){
    hb_harmony_t result={0};if(!hb_selection_word(word))return result;
    int root=mod12(note+g_bus.global_transpose);uint8_t notes[12];int count=0;
    unsigned mask=hb_transpose_mask(word&4095u,root);
    for(int pitch=0;pitch<12;pitch++)if(mask&(1u<<pitch))notes[count++]=(uint8_t)(60+pitch);
    result=hb_infer_harmony(notes,count);result.valid=1;result.root_pc=root;
    result=hb_refine_harmony_with_root(notes,count,result);
    result.chord_index|=HB_HARMONY_EXPLICIT_TONES;result.pitch_mask=result.detected_mask=mask;
    result.bass_pc=mod12(root+((word>>24)&15));result.intent_kind=7;
    result.intent_target=mod12(root+((word>>28)&15));
    result.intent_scale=hb_transpose_mask((word>>12)&4095u,root);
    result.intent_minor=hb_target_minor(result.intent_scale,result.intent_target);
    if(hb_selection_has_intent(word)){
        result.chord_index|=HB_HARMONY_FIXED_SCALE;result.intent_kind=(word>>37)&15;
        result.intent_target=mod12(root+((word>>41)&15));result.intent_minor=(word>>45)&1;
    }
    hb_format_harmony(result.name,sizeof(result.name),result);return result;
}
static hb_harmony_t hb_selected_harmony(const Inst *instance){
    const hb_cp_key *winner=0;
    for(int index=0;index<HB_CP_KEYS;index++){
        const hb_cp_key *key=&instance->player.keys[index];
        if(!key->used||!key->selection)continue;
        if(!winner||key->playback_origin<winner->playback_origin||
           (key->playback_origin==winner->playback_origin&&key->sequence>winner->sequence))winner=key;
    }
    if(!winner)return (hb_harmony_t){0};
    return hb_selection_decode(winner->selection_word,winner->source);
}
/* Read-only resolution shared by live selection and the sixteen-slot display.
   Fields: root, quality, form, scale policy, tonic, scale, bass, intent, degree. */
static int hb_selection_fields(const int *fields){
    static const int limits[]={11,12,16,3,11,19,12,13,6};
    for(int index=0;index<9;index++)if(fields[index]<0||fields[index]>limits[index])return 0;
    return fields[5]>=1;
}
static int hb_selection_parse(const char *parameter,int *fields,int bank){
    char extra;memset(fields,0,9*sizeof(int));
    int count=sscanf(parameter,"%d,%d,%d,%d,%d,%d,%d,%d,%d%c",fields,fields+1,fields+2,fields+3,fields+4,fields+5,fields+6,fields+7,fields+8,&extra);
    if(count!=9){
        if(bank||sscanf(parameter,"%d,%d,%d,%d,%d,%d,%d%c",fields,fields+1,fields+2,fields+3,fields+4,fields+5,fields+6,&extra)!=7)return 0;
        fields[7]=fields[8]=0;
    }
    return hb_selection_fields(fields);
}
static unsigned hb_selection_reference(Inst *instance,int *root){
    *root=0;hb_resolve_follower_reference_root(instance,root);
    unsigned parent=hb_follower_input_scale(instance,*root);
    hb_key_context context=hb_key_for(instance);
    /* Definitions stay in the source key; the shared render key map applies once. */
    if(context.active){*root=mod12(context.source_root-g_bus.global_transpose);parent=hb_transpose_mask(context.source_mask,-g_bus.global_transpose);}
    return parent;
}
static unsigned long long hb_selection_resolve(Inst *instance,const int *fields,int *source_note){
    if(!hb_selection_fields(fields))return 0;
    int root=fields[0],quality=fields[1],form=fields[2],policy=fields[3],bass=fields[6],intent=fields[7];
    int parent_root=0;unsigned reference=hb_selection_reference(instance,&parent_root),collection=reference;
    int target=mod12(parent_root+hb_nth_scale_interval_from_root(reference,parent_root,fields[8]));
    int kind=7,minor=0,scale_root=parent_root;
    hb_cp_config config={0};config.mode=1;config.quality=quality;config.size=form;
    if(intent>=12){
        /* Resolve scale families here, alongside Motif's shared key context. */
        int parent_minor=!!(reference&(1u<<mod12(parent_root+3)))&&!(reference&(1u<<mod12(parent_root+4)));
        collection=hb_explicit_scale_mask(parent_root,intent==13||!parent_minor?2:1);
        root=mod12(parent_root+hb_nth_scale_interval_from_root(collection,parent_root,fields[8]));
        target=parent_root;kind=7;minor=intent==13||!parent_minor;
        config.quality=quality?quality:intent==13?6:0;
        if(intent==13&&!config.size)config.size=3;
    }else if(intent){
        static const int secondary[]={0,3,15,2,12,0,16,17,0,0,0,0};
        hb_cadence_step borrowed={{0},intent==8?3:intent==9?4:intent==10?6:7,HB_CAD_BORROWED,0};
        hb_approach_result approach=hb_resolve_chord_approach(instance,60+target,reference,config,(hb_harmony_t){0},secondary[intent],intent>=8?&borrowed:0,intent==5?3:0,0);
        root=mod12(approach.root);collection=approach.scale;kind=approach.intent_kind;
        minor=approach.intent_minor;target=approach.intent_target;config=approach.config;
        if(quality)config.quality=quality;
        scale_root=target;
    }
    if(!config.size&&config.quality>=5)config.size=3;
    if(policy==2&&!intent)collection=hb_explicit_scale_mask(fields[4],fields[5]);
    int pitches[HB_CP_VOICES];unsigned semantic=0;
    hb_cp_voice_semantic(config,60+root,root,0,collection,pitches,&semantic);
    hb_harmony_t harmony={0};harmony.valid=1;harmony.root_pc=root;
    harmony.chord_index=HB_HARMONY_EXPLICIT_TONES;harmony.pitch_mask=semantic;
    harmony.intent_kind=intent?kind:0;harmony.intent_target=target;harmony.intent_minor=minor;harmony.intent_scale=collection;
    unsigned parent=reference;
    if(policy==1)parent=hb_accommodate_chord(reference,harmony,parent_root);
    else if(policy==2){scale_root=fields[4];parent=hb_explicit_scale_mask(scale_root,fields[5]);}
    else if(policy==3)parent=intent?hb_accommodate_chord(collection,harmony,scale_root):hb_parent_chord_scale(instance,harmony,parent_root,reference,0);
    else scale_root=parent_root;
    unsigned long long word=HB_SELECTION_TAG|hb_transpose_mask(semantic,-root)|
        ((unsigned long long)hb_transpose_mask(parent,-root)<<12)|
        ((unsigned long long)(bass==12?0:mod12(bass-root))<<24)|
        ((unsigned long long)mod12(scale_root-root)<<28);
    if(intent)word=(word&0xffffffffULL)|HB_SELECTION_INTENT_TAG|((unsigned long long)kind<<37)|
        ((unsigned long long)mod12(target-root)<<41)|((unsigned long long)minor<<45);
    *source_note=60+root;return hb_selection_word(word)?word:0;
}
static void hb_selection_prepare(Inst *instance,const char *parameter){
    int fields[9];instance->selection_pending=0;
    if(instance->role!=0||!hb_selection_parse(parameter,fields,0))return;
    instance->selection_pending=hb_selection_resolve(instance,fields,&instance->selection_note);
}
static void hb_selection_bank_set(Inst *instance,const char *parameter){
    int slots[21][9],count=0;const char *cursor=parameter;
    while(*cursor&&count<21){
        const char *end=strchr(cursor,';');size_t size=end?(size_t)(end-cursor):strlen(cursor);char text[128];
        if(size>=sizeof(text))return;
        memcpy(text,cursor,size);text[size]=0;
        if(!hb_selection_parse(text,slots[count++],1))return;
        cursor=end?end+1:cursor+size;
    }
    if(*cursor||(count!=16&&count!=21))return;
    memcpy(instance->selection_bank,slots,(size_t)count*sizeof(slots[0]));instance->selection_bank_valid=count;
}
static int hb_selection_family(hb_harmony_t harmony){
    unsigned mask=hb_transpose_mask(hb_harmony_chord_mask(harmony),-harmony.root_pc);
    if(__builtin_popcount(mask)<=2)return 0;
    if((mask&1024)&&((mask&16)||(!(mask&8)&&(mask&(4|32)))))return 3;
    if((mask&8)&&(mask&64)&&!(mask&128))return 4;
    if((mask&16)&&(mask&256)&&!(mask&128))return 5;
    if(mask&8)return 2;if(mask&16)return 1;
    return mask&(4|32)?6:0;
}
static int hb_selection_bank_snapshot(Inst *instance,char *buffer,int length){
    if(instance->role!=0||!instance->selection_bank_valid)return snprintf(buffer,(size_t)length,"--");
    int root=0;unsigned reference=hb_transpose_mask(hb_selection_reference(instance,&root),g_bus.global_transpose);
    hb_key_context context=hb_key_for(instance);if(context.active)reference=context.target_mask;
    hb_harmony_t current=hb_live_current();
    hb_harmony_t active=hb_key_harmony(instance,current.valid?current:hb_render_harmony(instance));
    int used=snprintf(buffer,(size_t)length,"cb1|%u|%d,%u,%u,%d,%d,%d",reference,active.valid?active.root_pc:-1,hb_harmony_chord_mask(active),active.intent_scale,active.intent_kind,active.intent_target,active.bass_pc);
    for(int index=0;index<instance->selection_bank_valid;index++){
        int note=0;unsigned long long word=hb_selection_resolve(instance,instance->selection_bank[index],&note);
        hb_harmony_t harmony=hb_key_harmony(instance,hb_selection_decode(word,note));unsigned mask=hb_harmony_chord_mask(harmony);
        if(used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),"|%d,%u,%u,%d,%d,%d,%d,%d,%s",harmony.valid?harmony.root_pc:-1,mask,harmony.intent_scale,harmony.intent_kind,harmony.intent_target,harmony.bass_pc,hb_selection_family(harmony),!!(mask&~reference),harmony.name);
    }
    return used<length?used:-1;
}
static int hb_selection_input(Inst *instance,const uint8_t *input,int length){
    if(!input||length<3)return 0;
    int type=input[0]&240,note=input[1]&127,channel=input[0]&15,origin=instance->movy_playback!=0;
    int on=type==144&&input[2],off=type==128||(type==144&&!input[2]);
    if(!on&&!off)return 0;
    if(origin&&hb_cq_owned(instance))return 1; /* Queued live authority survives recorded onsets/off edges. */
    if(off){
        int found=0;
        for(int index=0;index<HB_CP_KEYS;index++){
            hb_cp_key *key=&instance->player.keys[index];
            if(key->used&&key->selection&&key->source==note&&key->channel==channel&&key->playback_origin==origin){memset(key,0,sizeof(*key));found=1;}
        }
        if(found){instance->dirty=1;if(!origin){instance->selection_serial=0;instance->selection_until=0;hb_next_apply_effective(hb_clip_playhead());}}return found;
    }
    unsigned long long word=origin&&instance->recorded_action_valid[note]&&hb_selection_actions(instance->recorded_actions[note])?
        instance->recorded_actions[note][0]:!origin&&instance->selection_note==note?instance->selection_pending:0;
    if(!hb_selection_word(word)){
        // A recorded chord onset also ends an audition before a loop is learned.
        if(origin&&instance->role==0&&hb_live_selection(instance).valid){
            for(int index=0;index<HB_CP_KEYS;index++){
                hb_cp_key *key=&instance->player.keys[index];
                if(key->used&&key->selection&&!key->playback_origin)memset(key,0,sizeof(*key));
            }
            instance->selection_serial=0;instance->selection_until=0;instance->dirty=1;
            hb_next_apply_effective(hb_clip_playhead());
        }
        return 0;
    }
    instance->selection_pending=0;
    /* Recorded selections remain silent even if their clip is moved to a follower. */
    if(instance->role!=0)return 1;
    if(!origin){
        instance->selection_serial=++g_selection_serial;instance->selection_until=0;
        if(!hb_cq_owned(instance)&&hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING&&g_bus.next_model_locked){
            double phase=hb_next_phase(hb_clip_playhead());int next=hb_next_upcoming_event(phase);
            if(next>=0){double distance=g_bus.next_model[next].phase-phase;
                if(distance<=0)distance+=hb_next_loop_length();
                instance->selection_until=hb_current_beat()+distance;
            }
        }
    }
    int slot=-1;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&instance->player.keys[index];
        if(key->used&&(key->selection||(!origin&&key->playback_origin)))memset(key,0,sizeof(*key));
        if(!key->used&&slot<0)slot=index;
    }
    if(slot<0)return 1;
    hb_harmony_t harmony=hb_selection_decode(word,note);
    hb_cp_key *key=&instance->player.keys[slot];memset(key,0,sizeof(*key));
    key->selection_word=word;key->used=key->held=key->selection=1;key->source=note;key->channel=channel;key->playback_origin=origin;
    key->root_pc=harmony.root_pc;key->semantic_mask=harmony.pitch_mask;key->selection_bass=harmony.bass_pc;
    key->intent_kind=harmony.intent_kind;key->intent_target=harmony.intent_target;key->intent_scale=harmony.intent_scale;key->intent_minor=harmony.intent_minor;
    key->sequence=++instance->player.sequence;
    if(!origin)hb_effective_write(harmony);
    if(instance->render_channel>=0){
        hb_cp_config config=hb_chord_config_at(instance,note,hb_motion_position(instance),hb_motion_condition_position());
        config.mode=2; /* Voice the selected harmony, never the preceding bus chord. */
        key->count=hb_cp_voice_semantic(config,note+g_bus.global_transpose,harmony.root_pc,
            harmony.pitch_mask,harmony.intent_scale,key->notes,0);
        for(int voice=0;voice<key->count;voice++)key->notes[voice]=hb_key_pitch(instance,key->notes[voice]);
        key->velocity=input[2];key->fresh=1;key->played_pitch=note;
        instance->player.render_channel=instance->render_channel;
    }
    instance->dirty=instance->conductor_note_on_pending=1;instance->frames_since_change=0;
    if(!origin&&instance->action_count<64){
        int queued=(instance->action_head+instance->action_count)%64;
        memset(instance->action_queue[queued],0,sizeof(instance->action_queue[queued]));instance->action_queue[queued][0]=word;
        instance->action_pitch[queued]=(uint8_t)note;instance->action_count++;
    }
    return 1;
}
