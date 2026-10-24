/* Included after the ordinary conductor/recording helpers. Selection owns
   semantic evidence only. The comp actor retains every sounding-note owner. */
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
    unsigned long long word=HB_SELECTION_TAG|hb_transpose_mask(winner->semantic_mask,-winner->root_pc)|
        ((unsigned long long)hb_transpose_mask(winner->intent_scale,-winner->root_pc)<<12)|
        ((unsigned long long)mod12(winner->selection_bass-winner->root_pc)<<24)|
        ((unsigned long long)mod12(winner->intent_target-winner->root_pc)<<28);
    return hb_selection_decode(word,winner->source);
}
static void hb_selection_prepare(Inst *instance,const char *parameter){
    int root,quality,form,policy,tonic,scale,bass;char extra;
    instance->selection_pending=0;
    if(instance->role!=0||sscanf(parameter,"%d,%d,%d,%d,%d,%d,%d%c",&root,&quality,&form,&policy,&tonic,&scale,&bass,&extra)!=7||
       root<0||root>11||quality<0||quality>12||form<0||form>=HB_CP_FORMS||policy<0||policy>2||
       tonic<0||tonic>11||scale<1||scale>=HB_SCALE_COUNT||bass<0||bass>12)return;
    int parent_root=0;hb_resolve_follower_reference_root(instance,&parent_root);
    unsigned parent=hb_follower_input_scale(instance,parent_root);
    hb_key_context context=hb_key_for(instance);
    if(context.active){parent_root=context.target_root;parent=context.target_mask;}
    if(policy==2){parent_root=tonic;parent=hb_explicit_scale_mask(tonic,scale);}
    hb_cp_config config={0};config.mode=1;config.quality=quality;config.size=form==0&&quality>=5?3:form;
    int pitches[HB_CP_VOICES];unsigned semantic=0;
    hb_cp_voice_semantic(config,60+root,root,0,parent,pitches,&semantic);
    hb_harmony_t harmony={0};harmony.valid=1;harmony.root_pc=root;
    harmony.chord_index=HB_HARMONY_EXPLICIT_TONES;harmony.pitch_mask=semantic;
    if(policy==1)parent=hb_accommodate_chord(parent,harmony,parent_root);
    unsigned long long word=HB_SELECTION_TAG|hb_transpose_mask(semantic,-root)|
        ((unsigned long long)hb_transpose_mask(parent,-root)<<12)|
        ((unsigned long long)(bass==12?0:mod12(bass-root))<<24)|
        ((unsigned long long)mod12(parent_root-root)<<28);
    if(hb_selection_word(word)){instance->selection_pending=word;instance->selection_note=60+root;}
}
static int hb_selection_input(Inst *instance,const uint8_t *input,int length){
    if(!input||length<3)return 0;
    int type=input[0]&240,note=input[1]&127,channel=input[0]&15,origin=instance->movy_playback!=0;
    int on=type==144&&input[2],off=type==128||(type==144&&!input[2]);
    if(!on&&!off)return 0;
    if(off){
        int found=0;
        for(int index=0;index<HB_CP_KEYS;index++){
            hb_cp_key *key=&instance->player.keys[index];
            if(key->used&&key->selection&&key->source==note&&key->channel==channel&&key->playback_origin==origin){memset(key,0,sizeof(*key));found=1;}
        }
        if(found)instance->dirty=1;return found;
    }
    unsigned long long word=origin&&instance->recorded_action_valid[note]&&hb_selection_actions(instance->recorded_actions[note])?
        instance->recorded_actions[note][0]:!origin&&instance->selection_note==note?instance->selection_pending:0;
    if(!hb_selection_word(word))return 0;
    instance->selection_pending=0;
    /* Recorded selections remain silent even if their clip is moved to a follower. */
    if(instance->role!=0)return 1;
    int slot=-1;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&instance->player.keys[index];
        if(key->used&&key->selection&&key->playback_origin==origin)memset(key,0,sizeof(*key));
        if(!key->used&&slot<0)slot=index;
    }
    if(slot<0)return 1;
    hb_harmony_t harmony=hb_selection_decode(word,note);
    hb_cp_key *key=&instance->player.keys[slot];memset(key,0,sizeof(*key));
    key->used=key->held=key->selection=1;key->source=note;key->channel=channel;key->playback_origin=origin;
    key->root_pc=harmony.root_pc;key->semantic_mask=harmony.pitch_mask;key->selection_bass=harmony.bass_pc;
    key->intent_kind=7;key->intent_target=harmony.intent_target;key->intent_scale=harmony.intent_scale;key->intent_minor=harmony.intent_minor;
    key->sequence=++instance->player.sequence;
    instance->dirty=instance->conductor_note_on_pending=1;instance->frames_since_change=0;
    if(!origin&&instance->action_count<64){
        int queued=(instance->action_head+instance->action_count)%64;
        memset(instance->action_queue[queued],0,sizeof(instance->action_queue[queued]));instance->action_queue[queued][0]=word;
        instance->action_pitch[queued]=(uint8_t)note;instance->action_count++;
    }
    return 1;
}
