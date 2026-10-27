/* On-demand chord probes reuse selection resolution and the sounding owner's
   voicing. Auditions use the existing output owners, never conductor inputs. */
#define HB_DISCOVERY_OWNER_BASE (-100)
static void hb_discovery_stop(Inst *instance){
    hb_motion_route *routes[]={&instance->motion_local,&instance->motion_render};
    for(int route_index=0;route_index<2;route_index++){
        hb_motion_route *route=routes[route_index];
        for(int index=0;index<HB_MOTION_OWNERS;index++){
            hb_motion_owner *owner=&route->owners[index];
            if(owner->used&&owner->input_owner<=HB_DISCOVERY_OWNER_BASE&&
               owner->input_owner>HB_DISCOVERY_OWNER_BASE-HB_CP_VOICES&&hb_mo_release(route,owner)){
                owner->used=0;route->owned--;
            }
        }
    }
    hb_motion_flush_render(instance);
}
static Inst *hb_discovery_conductor(int track){
    for(int index=0;index<HB_MAX_INSTANCES;index++)
        if(g_pool[index].used&&g_pool[index].role==0&&g_pool[index].movy_track==track)return &g_pool[index];
    return 0;
}
static void hb_discovery_voice(Inst *instance,Inst *conductor,hb_harmony_t harmony,
                               unsigned long long word,int note,int variant){
    hb_cp_config config=hb_chord_config_at(instance,note,hb_motion_position(instance),hb_motion_condition_position());
    config.mode=2;
    if(word&HB_SELECTION_REGISTER){config.register_octaves=0;config.top_note=hb_selection_top(word)+1;}
    if(variant>=1&&variant<=3){config.voicing=0;config.inversion=variant;}
    if(variant>=4)config.voicing=variant==4?2:variant==5?1:4;
    int count=hb_cp_voice_semantic(config,note+g_bus.global_transpose,harmony.root_pc,
            hb_harmony_chord_mask(harmony),harmony.intent_scale,instance->discovery_notes,0);
    for(int voice=0;voice<count;voice++)instance->discovery_notes[voice]=hb_key_pitch(conductor,instance->discovery_notes[voice]);
    harmony=hb_key_harmony(conductor,harmony);
    if(config.top_note)count=hb_cp_range_top(instance->discovery_notes,count,config.top_note-1,harmony.root_pc,config.voicing);
    if(!count)return;
    int reference_root=0;unsigned reference=hb_transpose_mask(hb_selection_reference(conductor,&reference_root),g_bus.global_transpose);
    hb_key_context context=hb_key_for(conductor);if(context.active)reference=context.target_mask;
    int used=snprintf(instance->discovery_text,sizeof(instance->discovery_text),"cd1|%s|%d,%u,%u,%d,%d,%d,%d,%d|",
        harmony.name,harmony.root_pc,hb_harmony_chord_mask(harmony),harmony.intent_scale,harmony.intent_kind,
        harmony.intent_target,harmony.bass_pc,hb_selection_family(harmony),!!(hb_harmony_chord_mask(harmony)&~reference));
    for(int voice=0;voice<count;voice++)used+=snprintf(instance->discovery_text+used,sizeof(instance->discovery_text)-(size_t)used,
        "%s%d",voice?",":"",instance->discovery_notes[voice]);
    snprintf(instance->discovery_text+used,sizeof(instance->discovery_text)-(size_t)used,"|%d,%d",config.voicing,config.inversion);
    instance->discovery_count=count;
}
/* Capture source harmony once. Queue entries are read without advancing them;
   a waiting Touch Phrase has a useful tail even before it begins sounding. */
static void hb_discovery_context(Inst *instance,const char *parameter){
    hb_discovery_stop(instance);instance->discovery_anchor=(hb_harmony_t){0};instance->discovery_count=0;
    if(++instance->discovery_epoch==0)instance->discovery_epoch=1;
    int track,mode,offset=0,fields[9],range=0,note=0;
    if(sscanf(parameter,"%d,%d;%n",&track,&mode,&offset)!=2||offset<=0||mode<0||mode>2||
       !hb_selection_parse_register(parameter+offset,fields,&range))return;
    Inst *conductor=hb_discovery_conductor(track);if(!conductor||instance->role>=2)return;
    hb_harmony_t anchor={0};unsigned long long word=0;const char *origin="Current";
    if(mode==1&&hb_cq_owned(conductor)){
        hb_cq_event *tail=g_chord_queue.count?&g_chord_queue.pending[g_chord_queue.count-1]:
            conductor->chord_queue_loop&&g_chord_queue.cycle_count?&g_chord_queue.cycle[g_chord_queue.cycle_count-1]:0;
        if(tail){word=tail->word;note=tail->note;anchor=hb_selection_decode(word,note);origin="Queue";}
    }
    if(!anchor.valid&&mode!=2){
        anchor=hb_cq_owned(conductor)?hb_cq_at(conductor,hb_current_beat(),0):hb_render_harmony(conductor);
        if(!anchor.valid)anchor=bus_read();
        note=60+mod12(anchor.root_pc-g_bus.global_transpose);
        /* Retain explicit top-note/octave selection ranges for live chords. */
        const hb_cp_key *selected=0;
        for(int index=0;index<HB_CP_KEYS;index++){
            const hb_cp_key *key=&conductor->player.keys[index];
            if(key->used&&key->selection&&(!selected||key->sequence>selected->sequence))selected=key;
        }
        if(selected&&hb_harmony_equal_effective(anchor,hb_selection_decode(selected->selection_word,selected->selection_pitch))){word=selected->selection_word;note=selected->selection_pitch;}
    }
    if(!anchor.valid||mode==2){
        word=hb_selection_resolve(conductor,fields,&note);if(!word)return;
        word=hb_selection_with_register(word,&note,range);anchor=hb_selection_decode(word,note);origin="Edit";
    }
    instance->discovery_track=track;instance->discovery_transpose=g_bus.global_transpose;
    instance->discovery_parent=hb_selection_reference(conductor,&instance->discovery_parent_root);
    instance->discovery_key=hb_key_for(conductor);
    if(!anchor.intent_scale)anchor.intent_scale=hb_parent_chord_scale(conductor,anchor,
        mod12(instance->discovery_parent_root+g_bus.global_transpose),
        hb_transpose_mask(instance->discovery_parent,g_bus.global_transpose),0);
    instance->discovery_anchor=anchor;instance->discovery_word=word;instance->discovery_note=note;
    hb_discovery_voice(instance,conductor,anchor,word,note,0);
    if(!instance->discovery_count){instance->discovery_anchor.valid=0;return;}
    snprintf(instance->discovery_context_text,sizeof(instance->discovery_context_text),"dc1|%u|%s|%s",
        instance->discovery_epoch,origin,instance->discovery_text);
}
static int hb_discovery_context_valid(Inst *instance,Inst *conductor){
    if(!conductor||!instance->discovery_anchor.valid||instance->discovery_transpose!=g_bus.global_transpose)return 0;
    int root=0;unsigned parent=hb_selection_reference(conductor,&root);hb_key_context key=hb_key_for(conductor);
    return root==instance->discovery_parent_root&&parent==instance->discovery_parent&&
        !memcmp(&key,&instance->discovery_key,sizeof(key));
}
static void hb_discovery_probe(Inst *instance,const char *parameter){
    int track,variant,anchor=0,offset=0,fields[9],range=0,note=0;unsigned epoch=0;
    instance->discovery_count=0;memcpy(instance->discovery_text,"--",3);
    int contextual=!strncmp(parameter,"c2,",3);
    if(contextual){
        if(sscanf(parameter,"c2,%u,%d,%d;%n",&epoch,&variant,&anchor,&offset)!=3||
           epoch!=instance->discovery_epoch||anchor<0||anchor>1)return;
        track=instance->discovery_track;
    }else if(sscanf(parameter,"%d,%d;%n",&track,&variant,&offset)!=2)return;
    if(offset<=0||track<0||track>=16||variant<0||variant>6||
       !hb_selection_parse_register(parameter+offset,fields,&range))return;
    Inst *conductor=hb_discovery_conductor(track);
    if(!conductor||instance->role>=2||(contextual&&!hb_discovery_context_valid(instance,conductor)))return;
    unsigned long long word=instance->discovery_word;hb_harmony_t harmony=instance->discovery_anchor;
    if(anchor)note=instance->discovery_note;
    else {word=hb_selection_resolve(conductor,fields,&note);if(!word)return;
        word=hb_selection_with_register(word,&note,range);harmony=hb_selection_decode(word,note);}
    hb_discovery_voice(instance,conductor,harmony,word,note,variant);
}
static void hb_discovery_audition(Inst *instance,const char *parameter){
    int frozen=!strncmp(parameter,"notes:",6);
    if(!frozen&&strcmp(parameter,"on")&&strcmp(parameter,"off"))return;
    hb_discovery_stop(instance);
    if(frozen){
        int notes[HB_CP_VOICES],count=0;const char *cursor=parameter+6;
        instance->discovery_count=0;
        while(*cursor){
            char *end=0;long pitch=strtol(cursor,&end,10);
            if(end==cursor||pitch<0||pitch>127||count>=HB_CP_VOICES||(*end&&*end!=','))return;
            notes[count++]=(int)pitch;if(!*end)break;cursor=end+1;if(!*cursor)return;
        }
        if(!count)return;memcpy(instance->discovery_notes,notes,(size_t)count*sizeof(int));instance->discovery_count=count;
    }
    if((!frozen&&strcmp(parameter,"on"))||instance->role>=2)return;
    for(int voice=0;voice<instance->discovery_count;voice++){
        int pitch=instance->discovery_notes[voice],identity=HB_DISCOVERY_OWNER_BASE-voice;
        uint8_t message[3]={0x90,(uint8_t)pitch,100};
        hb_mo_event_owned(&instance->motion_local,message,pitch,100,-1,-1,0,identity);
        if(instance->render_channel>=0){
            message[0]=(uint8_t)(0x90|instance->render_channel);
            hb_mo_event_owned(&instance->motion_render,message,pitch,100,-1,-1,0,identity);
        }
    }
    hb_motion_flush_render(instance);
}
