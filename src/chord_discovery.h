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
static void hb_discovery_probe(Inst *instance,const char *parameter){
    int conductor_track,variant,offset=0,fields[9],range=0,note=0;
    instance->discovery_count=0;memcpy(instance->discovery_text,"--",3);
    if(sscanf(parameter,"%d,%d;%n",&conductor_track,&variant,&offset)!=2||offset<=0||
       conductor_track<0||conductor_track>=16||variant<0||variant>6||
       !hb_selection_parse_register(parameter+offset,fields,&range))return;
    Inst *conductor=0;
    for(int index=0;index<HB_MAX_INSTANCES;index++)
        if(g_pool[index].used&&g_pool[index].role==0&&g_pool[index].movy_track==conductor_track){conductor=&g_pool[index];break;}
    if(!conductor||instance->role>=2)return;
    unsigned long long word=hb_selection_resolve(conductor,fields,&note);
    if(!word)return;
    word=hb_selection_with_register(word,&note,range);
    hb_harmony_t harmony=hb_selection_decode(word,note);
    hb_cp_config config=hb_chord_config_at(instance,note,hb_motion_position(instance),hb_motion_condition_position());
    config.mode=2;
    if(word&HB_SELECTION_REGISTER){config.register_octaves=0;config.top_note=hb_selection_top(word)+1;}
    if(variant>=1&&variant<=3){config.voicing=0;config.inversion=variant;}
    if(variant>=4)config.voicing=variant==4?2:variant==5?1:4;
    int count=hb_cp_voice_semantic(config,note+g_bus.global_transpose,harmony.root_pc,
        harmony.pitch_mask,harmony.intent_scale,instance->discovery_notes,0);
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
static void hb_discovery_audition(Inst *instance,const char *parameter){
    if(strcmp(parameter,"on")&&strcmp(parameter,"off"))return;
    hb_discovery_stop(instance);
    if(strcmp(parameter,"on")||instance->role>=2)return;
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
