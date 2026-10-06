/* Included after the normal render path. Motif voices bypass remapping: their
   semantic relationships have already been resolved against the anchor. */
static hb_harmony_t hb_mt_harmony_at(Inst *instance,double arrival,double now){
    hb_harmony_t override;if(hb_override_read(instance,&override))return override;
    hb_harmony_t harmony=bus_read();
    if(hb_harmony_knowledge_ready_for(instance)){
        double phase=hb_next_phase(hb_clip_playhead()+arrival-now);
        int event=hb_next_model_event_for_phase_for(instance,phase,0);
        if(event>=0)harmony=g_bus.next_model[event].harmony;
    }
    return harmony;
}
static double hb_mt_arrival(Inst *instance,double now){
    int choice=instance->motif.editor.arrival;
    if(!choice)return now;
    if(choice==3){
        if(!hb_harmony_knowledge_ready_for(instance))return -1;
        double phase=hb_next_phase(hb_clip_playhead());int index=hb_next_upcoming_event(phase);
        if(index<0)return -1;
        double delta=g_bus.next_model[index].phase-phase;
        if(delta<=1e-6)delta+=hb_next_loop_length();return now+delta;
    }
    double grid=choice==1?1:choice==4?8:choice==5?16:4;
    return (hb_cp_floor(now/grid)+1)*grid;
}
static int hb_mt_target_pitch(Inst *instance,int input,hb_harmony_t harmony){
    int choice=instance->motif.editor.target,pitch=input+g_bus.global_transpose;
    if(!choice){
        if(instance->role!=1)return pitch;
        hb_harmony_t previous=instance->render_harmony;int active=instance->render_harmony_active;
        instance->render_harmony=harmony;instance->render_harmony_active=1;
        int mapped=hb_map_follower_note_unoperated(instance,input);
        instance->render_harmony=previous;instance->render_harmony_active=active;return mapped;
    }
    if(!harmony.valid)return -1;
    unsigned mask=hb_harmony_chord_mask(harmony);int interval=-1;
    if(choice==1)interval=0;
    else if(choice==2){if(mask&(1u<<mod12(harmony.root_pc+3)))interval=3;else if(mask&(1u<<mod12(harmony.root_pc+4)))interval=4;}
    else if(choice==3){for(int candidate=6;candidate<=8;candidate++)if(mask&(1u<<mod12(harmony.root_pc+candidate))){interval=candidate;break;}}
    else if(choice==4){for(int candidate=10;candidate<=11;candidate++)if(mask&(1u<<mod12(harmony.root_pc+candidate))){interval=candidate;break;}}
    else interval=choice==5?2:5;
    if(interval<0)return -1; /* Never silently relabel a missing chord member. */
    return hb_cp_nearest(pitch,1u<<mod12(harmony.root_pc+interval));
}
/* Both clock and tap playback use this single bounded event renderer. */
static int hb_mt_schedule(Inst *instance,const hb_mt_phrase *phrase,int input,int velocity,int channel,
    int first,int last,double arrival,double onset_override,int use_tap_context){
    hb_mt_runtime *runtime=&instance->motif;hb_mt_recorder *editor=&runtime->editor;
    if(runtime->cancel&&runtime->pending){editor->error=8;return 0;}
    if(!phrase->count||phrase->anchor<0||phrase->anchor>phrase->count||
       (phrase->anchor==phrase->count&&!phrase->reference_valid))return 0;
    double now=hb_motion_position(instance);
    if(arrival<0)arrival=hb_mt_arrival(instance,now);
    if(arrival<0){editor->error=5;return 0;}
    int rhythm=use_tap_context?runtime->tap_rhythm:hb_rr_pattern(instance);
    int span=use_tap_context?runtime->tap_span:g_motif_span;
    double before=0;
    for(int step=0;step<phrase->anchor;step++)before+=hb_mt_duration(phrase,step,rhythm,span);
    if(onset_override<0&&editor->late==2&&arrival-now<before){
        if(editor->arrival==3){editor->error=6;return 0;}
        double grid=editor->arrival==1?1:editor->arrival==4?8:editor->arrival==5?16:4;
        while(arrival-now<before)arrival+=grid;
    }
    double fit=1;
    if(onset_override<0&&editor->late==1&&before>0&&arrival-now<before)fit=(arrival-now)/before;
    hb_harmony_t harmony=use_tap_context?runtime->tap_harmony:hb_mt_harmony_at(instance,arrival,now);
    if(instance->role==1)harmony=hb_key_harmony(instance,harmony);
    int target=use_tap_context?runtime->tap_target:hb_mt_target_pitch(instance,input,harmony);
    if(target<0){editor->error=7;return 0;}
    unsigned scale=harmony.valid?hb_follower_scale_target(instance,harmony).pitch_mask:hb_follower_input_scale(instance,reference_root(instance));
    if(!scale)scale=0xFFF;
    scale=hb_key_approach_scale(instance,target,scale);
    hb_key_context phrase_key=hb_key_for(instance);
    if(phrase_key.approach_scale)phrase_key=hb_key_collection_context(phrase_key,target,scale);
    int anchor=phrase->reference_valid?phrase->reference_pitch:phrase->events[phrase->anchor].notes[0].pitch;
    hb_mt_scheduled staged[HB_MT_SCHEDULE];int count=0;double key_anchor=-1;
    double offset=-before;
    for(int step=0;step<phrase->count;step++){
        const hb_mt_event *event=&phrase->events[step];double duration=hb_mt_duration(phrase,step,rhythm,span);
        double onset=onset_override>=0?onset_override:arrival+(step<phrase->anchor?offset*fit:offset);
        double sustained=duration;
        for(int next=step+1;next<phrase->count&&phrase->events[next].kind==2;next++)sustained+=hb_mt_duration(phrase,next,rhythm,span);
        double end=onset+sustained*(step<phrase->anchor?fit:1);
        offset+=duration;
        if(step<first||step>=last)continue;
        /* Retain the anchor and tail. Drop missed attacks, never bunch them up.
           Fit has a 1/64-note density floor even after very late triggers. */
        if(onset<now-1e-6||(step<phrase->anchor&&duration*fit<0.0625))continue;
        if(step==phrase->anchor&&event->count)key_anchor=onset;
        int step_start=count,override_root=0,override_semantic=0;
        unsigned override_mask=0;
        for(int voice=0;voice<event->count;voice++){
            int pitch=hb_mt_relative(event,event->notes[voice].pitch,anchor,target,scale);
            unsigned collection=scale;
            int intent_kind=0,intent_target=mod12(pitch),intent_minor=0;
            const unsigned long long *old_actions=instance->motion.event_override;unsigned long long old_flags=instance->motion.render_flags;
            hb_harmony_t old_harmony=instance->render_harmony;int old_active=instance->render_harmony_active;
            instance->motion.event_override=event->actions;instance->motion.render_flags=event->flags;
            instance->render_harmony=harmony;instance->render_harmony_active=1;
            hb_cp_config config=hb_chord_config_at(instance,event->notes[voice].pitch,onset,onset);config.mode=event->chord_mode==3?hb_cp_mode(&instance->player):event->chord_mode;
            const hb_cadence_step *cadence=hb_cadence_decode(event->cadence);
            if(config.mode&&(event->secondary||cadence||event->modifier)){
                hb_approach_result approach=hb_resolve_chord_approach(instance,pitch,collection,config,harmony,event->secondary,cadence,event->modifier,0);
                pitch=approach.root;collection=approach.scale;config=approach.config;
                intent_kind=approach.intent_kind;intent_target=approach.intent_target;intent_minor=approach.intent_minor;
            }else{
                if(cadence){hb_cadence_result result=hb_resolve_cadence(instance,cadence,pitch,collection);pitch=result.root;collection=result.scale;}
                else if(event->secondary)pitch+=hb_relative_approach_offset(event->secondary,pitch,hb_secondary_collection(instance,event->secondary,pitch,collection));
                if(event->modifier<0)pitch--;
                else if(event->modifier==1)pitch=hb_mt_walk(pitch,1,collection);
                else if(event->modifier==2)pitch++;
            }
            instance->motion.event_override=old_actions;instance->motion.render_flags=old_flags;
            instance->render_harmony=old_harmony;instance->render_harmony_active=old_active;
            int pitches[HB_CP_VOICES],voices=1;pitches[0]=pitch;
            unsigned semantic=0;
            if(config.mode){config.playback=0;config.spread=0;voices=hb_cp_voice_semantic(config,pitch,harmony.valid?harmony.root_pc:mod12(target),hb_harmony_chord_mask(harmony),collection,pitches,&semantic);}
            if(intent_kind==2)semantic|=(1u<<mod12(pitch))|(1u<<mod12(pitch+4))|(1u<<mod12(pitch+10));
            if(intent_kind==3)semantic|=(1u<<mod12(pitch))|(1u<<mod12(pitch+3))|(1u<<mod12(pitch+6))|(1u<<mod12(pitch+9));
            if(intent_kind>=8&&intent_kind<=10){
                hb_cp_config identity=config;identity.mode=1;identity.size=3;
                int identity_notes[HB_CP_VOICES];unsigned identity_mask=0;
                hb_cp_voice_semantic(identity,pitch,mod12(pitch),0,collection,identity_notes,&identity_mask);semantic|=identity_mask;
            }
            override_mask|=semantic;
            override_root=config.mode==2&&harmony.valid?harmony.root_pc:mod12(pitch);
            override_semantic|=semantic!=0;
            int rendered_pitches[HB_CP_VOICES];
            for(int generated=0;generated<voices;generated++)rendered_pitches[generated]=instance->role==0?hb_key_map(phrase_key,pitches[generated]):pitches[generated];
            if(instance->role==0&&phrase_key.active&&!phrase_key.blues&&config.mode&&
               (intent_kind==2||intent_kind==3||(intent_kind>=8&&intent_kind<=10))){
                int destination=mod12(hb_key_map(phrase_key,60+intent_target));
                unsigned mapped_collection=hb_function_family(instance,destination,phrase_key.target_mask,hb_target_minor(phrase_key.target_mask,destination),0);
                int root=mod12(destination+(intent_kind==2?7:intent_kind==3?11:hb_nth_scale_interval_from_root(mapped_collection,destination,1+2*(intent_kind-8))));
                int mapped_pitch=hb_cp_nearest(hb_key_map(phrase_key,pitch),1u<<root);
                hb_cp_config mapped_config=config;mapped_config.mode=1;
                mapped_config.quality=intent_kind==2?6:intent_kind==3?9:0;
                int mapped_count=hb_cp_voice_semantic(mapped_config,mapped_pitch,mod12(pitch),semantic,mapped_collection,rendered_pitches,0);
                /* Forms retain voice cardinality under seven-note family changes. */
                if(mapped_count<voices)voices=mapped_count;
            }
            for(int generated=0;generated<voices;generated++){
                if(pitches[generated]<0||pitches[generated]>127)continue;
                if(count>=HB_MT_SCHEDULE){editor->error=8;return 0;}
                int gain=(int)event->notes[voice].velocity*velocity/100;
                staged[count++]=(hb_mt_scheduled){onset,end,rendered_pitches[generated],hb_cp_clamp(gain,1,127),channel,instance->render_channel,0,1};
                staged[count-1].reference_pitch=pitches[generated];
                staged[count-1].semantic_mask=semantic;
                staged[count-1].root_pc=override_root;
                staged[count-1].intent_kind=intent_kind;staged[count-1].intent_target=intent_target;staged[count-1].intent_minor=intent_minor;
                staged[count-1].intent_scale=phrase_key.approach_scale?scale:0;
                if(generated==0)staged[count-1].trail_target=(unsigned short)((instance->role==0?hb_key_map(phrase_key,pitch):pitch)+1);
                override_mask|=1u<<mod12(pitches[generated]);
            }
        }
        int owner_index=hb_override_index(instance);
        if(count>step_start&&owner_index>=0&&g_override[owner_index].active&&
           (g_override[owner_index].active==2||!instance->movy_playback)){
            /* One complete chord per step, not one authority per voice.
               Releasing/rearming the operation invalidates queued authority. */
            staged[step_start].override_mask=override_mask;
            staged[step_start].override_root=override_root;
            staged[step_start].override_semantic=override_semantic;
            staged[step_start].override_generation=g_override[owner_index].generation;
        }
    }
    int free_count=0;for(int index=0;index<HB_MT_SCHEDULE;index++)if(!runtime->events[index].used)free_count++;
    if(count>free_count){editor->error=8;return 0;}
    for(int source=0,index=0;source<count&&index<HB_MT_SCHEDULE;index++)if(!runtime->events[index].used)runtime->events[index]=staged[source++];
    runtime->pending+=count;runtime->cancel=0;runtime->last_beat=now;runtime->have_beat=1;runtime->was_running=hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING;editor->error=0;runtime->flash_serial++;runtime->flash_pitch=input;runtime->flash_step=phrase->anchor;if(instance->key_schedule_arm&&g_key_armed&&key_anchor>=0){
        instance->key_pending=1;instance->key_pending_at=key_anchor;
        instance->key_pending_context=hb_key_destination(instance,input);instance->key_pending_action=instance->key_action;
    }
    return 1;
}

static const hb_mt_phrase *hb_mt_selected(Inst *instance,hb_mt_phrase *builtin){
    hb_mt_recorder *editor=&instance->motif.editor;
    hb_mt_definition stock;const hb_mt_definition *definition=0;
    if(editor->preset){hb_mt_preset_definition(editor->preset,&stock);definition=&stock;}
    else if(editor->selected>=0&&editor->selected<HB_MT_SLOTS&&hb_mt_occupied(&g_motifs[editor->selected]))
        definition=&g_motifs[editor->selected];
    memset(builtin,0,sizeof(*builtin));
    if(definition)hb_mt_materialize(definition,hb_mt_placement(editor->placement,definition->placement),builtin);
    /* Omit approaches a silent destination after the body. Saved continues
       to honor the legacy arrival position of body-only stock phrases. */
    if(editor->placement==4)builtin->anchor=builtin->count;
    if(definition&&instance->motif_load[5])hb_mt_pad_even(builtin,g_motif_rhythm,hb_mt_placement(editor->placement,definition->placement));
    return builtin;
}
static double hb_mt_step_offset(Inst *instance,const hb_mt_phrase *phrase,int step){
    double offset=0;int rhythm=instance->motif.tap_rhythm,span=instance->motif.tap_span;
    for(int index=0;index<step;index++)offset+=hb_mt_duration(phrase,index,rhythm,span);
    for(int index=0;index<phrase->anchor;index++)offset-=hb_mt_duration(phrase,index,rhythm,span);
    return offset;
}
static int hb_mt_launch(Inst *instance,int input,int velocity,int channel){
    hb_mt_runtime *runtime=&instance->motif;hb_mt_recorder *editor=&runtime->editor;
    hb_mt_phrase builtin;const hb_mt_phrase *phrase=hb_mt_selected(instance,&builtin);
    if(!editor->playback){runtime->tap_active=0;return hb_mt_schedule(instance,phrase,input,velocity,channel,0,phrase->count,-1,-1,0);}
    double now=hb_motion_position(instance);
    if(runtime->cancel){if(runtime->pending){editor->error=8;return 0;}runtime->cancel=0;runtime->tap_active=0;}
    if(!runtime->tap_active||runtime->tap_input!=input||runtime->tap_channel!=channel){
        if(!phrase->count){editor->error=10;return 0;}
        double arrival=editor->playback==1?now:hb_mt_arrival(instance,now);
        if(arrival<0){editor->error=5;return 0;}
        hb_harmony_t harmony=hb_mt_harmony_at(instance,arrival,now);
        int target=hb_mt_target_pitch(instance,input,harmony);if(target<0){editor->error=7;return 0;}
        runtime->tap_phrase=*phrase;runtime->tap_rhythm=hb_rr_pattern(instance);runtime->tap_span=g_motif_span;runtime->tap_step=0;runtime->tap_active=1;
        runtime->tap_input=input;runtime->tap_channel=channel;runtime->tap_arrival=arrival;
        runtime->tap_target=target;runtime->tap_harmony=harmony;runtime->tap_last_due=-1;
        runtime->was_running=hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING;runtime->last_beat=now;runtime->have_beat=1;
    }
    int step=runtime->tap_step;double due=now;
    if(editor->playback==3){
        double grid=.125*(1<<editor->tap_grid);
        due=hb_cp_floor((now+grid-1e-7)/grid)*grid;
        if(due<=runtime->tap_last_due+1e-7)due=runtime->tap_last_due+grid;
    }
    if(!hb_mt_schedule(instance,&runtime->tap_phrase,input,velocity,channel,step,step+1,runtime->tap_arrival,due,1))return 0;
    runtime->tap_last_due=due;runtime->tap_velocity=velocity;
    runtime->flash_step=step;
    /* Ties lengthen their preceding note; rests remain deliberate tap steps. */
    runtime->tap_step++;
    while(runtime->tap_step<runtime->tap_phrase.count&&runtime->tap_phrase.events[runtime->tap_step].kind==2)runtime->tap_step++;
    if(runtime->tap_step>=runtime->tap_phrase.count)runtime->tap_active=0;
    return 1;
}

static int hb_mt_input(Inst *instance,const uint8_t *input,int length){
    if(length<3||instance->movy_playback||instance->role>=2)return 0;
    int kind=input[0]&0xf0,channel=input[0]&15,pitch=input[1]&127;
    int on=kind==0x90&&input[2],off=kind==0x80||(kind==0x90&&!input[2]);
    if(!on&&!off)return 0;
    hb_mt_recorder *editor=&instance->motif.editor;
    if(off&&editor->swallow[channel][pitch]){editor->swallow[channel][pitch]=0;hb_mt_release(editor,channel,pitch);return 1;}
    if(!hb_source_channel_matches(instance,channel))return 0;
    if(editor->recording>=0){
        if(on){
            hb_mt_event intent;memset(&intent,0,sizeof(intent));
            if(!editor->open)hb_mo_input(&instance->motion,pitch,instance->motion_beat,0);
            unsigned long long actions[HB_MOTION_LANES+1];hb_capture_input_intent(instance,pitch,actions);
            memcpy(intent.actions,actions,sizeof(actions));
            const unsigned long long *saved=instance->motion.event_override;instance->motion.event_override=actions;
            intent.secondary=hb_secondary_at(instance,pitch);intent.modifier=hb_operation_modifier(instance,pitch);
            if(!intent.modifier){int approach=hb_active_approach(instance);intent.modifier=approach==HB_APPROACH_CHROM_BELOW?-1:approach==HB_APPROACH_SCALE_ABOVE?1:0;}
            intent.cadence=(int)((actions[HB_MOTION_LANES]>>13)&127);intent.flags=instance->motion.render_flags;
            intent.chord_mode=hb_cp_mode(&instance->player);
            hb_harmony_t harmony=hb_render_harmony(instance);
            intent.scale=hb_follower_input_scale(instance,reference_root(instance));
            if(!intent.scale)intent.scale=harmony.valid?hb_follower_scale_target(instance,harmony).pitch_mask:0xFFF;
            instance->motion.event_override=saved;
            hb_mt_add(editor,channel,pitch,input[2],&intent);
            hb_consume_next_approach(instance);
            instance->motion.gesture_once_used|=instance->motion.gesture_once;
        }
        if(off)hb_mt_release(editor,channel,pitch);
        return 0; /* Normal HB path auditions the entry; Movy suppresses clip writes. */
    }
    if(instance->motif.playback_lane>=0&&instance->motif.tap_active&&!hb_mo_lane_active(&instance->motion,instance->motif.playback_lane))instance->motif.tap_active=0;
    if(on&&editor->lane<0){
        int selected=-1;unsigned serial=0;
        for(int lane=0;lane<HB_MOTION_USER_LANES;lane++){
            if(instance->motion.lanes[lane].operation!=HB_MO_MOTIF||!hb_mo_lane_active(&instance->motion,lane))continue;
            if(selected<0||instance->motion.held_serial[lane]>=serial){selected=lane;serial=instance->motion.held_serial[lane];}
        }
        if(selected>=0){
            hb_motion_lane *lane=&instance->motion.lanes[selected];
            if(lane->amount<1||lane->amount>=44){editor->error=10;return 0;}
            hb_mt_lane_load(editor,lane);
            if(instance->motif.playback_lane!=selected){instance->motif.tap_active=0;instance->motif.playback_lane=selected;}
            if(hb_mt_launch(instance,pitch,input[2],channel)){
                unsigned long long bit=1ULL<<selected;
                instance->motion.gesture_used|=instance->motion.gesture_down&bit;
                if(!instance->motif.tap_active&&(instance->motion.gesture_once&bit)&&!(instance->motion.gesture_persistent&bit))hb_mo_end_lanes(&instance->motion,bit);
                editor->swallow[channel][pitch]=1;return 1;
            }return 0;
        }
    }
    if(on&&(editor->armed>=0||instance->motif.tap_active)){
        if(editor->armed>=0){editor->selected=editor->armed;editor->armed=-1;instance->motif.tap_active=0;}
        hb_mt_launch(instance,pitch,input[2],channel);editor->swallow[channel][pitch]=1;return 1;
    }
    return 0;
}
static int hb_mt_emit(Inst *instance,hb_mt_scheduled *event,int on,uint8_t output[][3],int lengths[],int capacity){
    if(capacity<1)return 0;
    if(on&&event->override_mask){
        int index=hb_override_index(instance);
        if(index>=0&&g_override[index].active&&g_override[index].generation==event->override_generation)
            hb_override_offer(&g_override[index],event->override_mask,event->override_root,event->override_semantic);
    }
    int local_shared=0,render_shared=0;
    for(int index=0;index<HB_MT_SCHEDULE;index++){
        const hb_mt_scheduled *other=&instance->motif.events[index];
        if(other==event||!other->used||!other->started||other->pitch!=event->pitch)continue;
        if(other->channel==event->channel)local_shared=1;
        if(other->render==event->render)render_shared=1;
    }
    if(!local_shared){output[0][0]=(uint8_t)((on?0x90:0x80)|event->channel);output[0][1]=(uint8_t)event->pitch;output[0][2]=(uint8_t)(on?event->velocity:0);lengths[0]=3;}
    if(event->render>=0&&!render_shared){uint8_t packet[4]={(uint8_t)(on?0x29:0x28),(uint8_t)((on?0x90:0x80)|event->render),(uint8_t)event->pitch,(uint8_t)(on?event->velocity:0)};hb_send_render_raw(instance,packet,event->render==event->channel);}
    if(on&&!local_shared)hb_trail_heard(instance,event->trail_target);
    event->started=on;
    if(on&&instance->key_pending&&event->on+1e-6>=instance->key_pending_at){hb_key_commit_pending(instance);}
    if(instance->role==0){instance->conductor_note_on_pending|=on;instance->dirty|=on;instance->frames_since_change=0;hb_publish_instance_notes(instance);}
    return local_shared?0:1;
}
static int hb_mt_tick(Inst *instance,uint8_t output[][3],int lengths[],int capacity){
    hb_mt_runtime *runtime=&instance->motif;double now=hb_motion_position(instance);int emitted=0;
    int running=hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING;
    if((runtime->was_running&&!running)||(runtime->have_beat&&running&&now<runtime->last_beat-1e-6))runtime->cancel=1;
    runtime->was_running=running;runtime->last_beat=now;runtime->have_beat=1;
    if(runtime->cancel){runtime->tap_active=0;instance->key_pending=0;}
    if(runtime->tap_active&&runtime->editor.playback>=2&&runtime->editor.completion){
        double next=runtime->tap_arrival+hb_mt_step_offset(instance,&runtime->tap_phrase,runtime->tap_step);
        if(now+1e-6>=next&&now>runtime->tap_last_due+1e-6){
            if(hb_mt_schedule(instance,&runtime->tap_phrase,runtime->tap_input,runtime->tap_velocity,runtime->tap_channel,
                runtime->tap_step,runtime->tap_phrase.count,runtime->tap_arrival,-1,1))runtime->tap_active=0;
        }
    }
    if(!runtime->pending){runtime->cancel=0;return 0;}
    /* OFF precedes ON at a shared boundary. Full output buffers retain due
       events until a later tick; a missed complete event is omitted. */
    for(int index=0;index<HB_MT_SCHEDULE;index++){
        hb_mt_scheduled *event=&runtime->events[index];if(!event->used)continue;
        if(runtime->cancel||now+1e-6>=event->off){
            if(event->started){if(emitted>=capacity)continue;emitted+=hb_mt_emit(instance,event,0,output+emitted,lengths+emitted,capacity-emitted);}
            event->used=0;runtime->pending--;
        }
    }
    if(runtime->cancel){int left=0;for(int index=0;index<HB_MT_SCHEDULE;index++)left|=runtime->events[index].used;runtime->cancel=left;return emitted;}
    for(int index=0;index<HB_MT_SCHEDULE&&emitted<capacity;index++){
        hb_mt_scheduled *event=&runtime->events[index];
        if(event->used&&!event->started&&now+1e-6>=event->on)emitted+=hb_mt_emit(instance,event,1,output+emitted,lengths+emitted,capacity-emitted);
    }
    return emitted;
}
