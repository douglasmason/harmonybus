/* Versioned, bounded motif persistence; no transient recording/voice ownership
   is serialized. Each saved HB instance carries the shared library. */
static int hb_mt_write_hex(char *buffer,int length,int used,unsigned value,int digits){
    if(used<0||used+digits>=length)return length;
    static const char hex[]="0123456789abcdef";
    for(int index=digits-1;index>=0;index--)buffer[used++]=hex[(value>>(index*4))&15];
    buffer[used]=0;return used;
}
static int hb_mt_read_hex(const char **cursor,unsigned *value,int digits){
    *value=0;
    for(int index=0;index<digits;index++){
        char character=**cursor;unsigned digit;
        if(character>='0'&&character<='9')digit=(unsigned)(character-'0');
        else if(character>='a'&&character<='f')digit=(unsigned)(character-'a'+10);else return 0;
        *value=(*value<<4)|digit;(*cursor)++;
    }return 1;
}
static int hb_mt_save(Inst *instance,char *buffer,int length,int used){
    if(used<0||used>=length)return used;
    hb_mt_recorder *editor=&instance->motif.editor;
    used+=snprintf(buffer+used,(size_t)(length-used),";mf1,%d,%d,%d,%d,%d,%d,%d:",editor->selected,editor->duration,editor->relation,editor->arrival,editor->late,g_motif_span,editor->target);
    const unsigned long long *snapshots[128];int snapshot_count=0;
    for(int slot=0;slot<HB_MT_SLOTS;slot++)for(int step=0;step<g_motifs[slot].count;step++){
        const unsigned long long *actions=g_motifs[slot].events[step].actions;int found=0;
        for(int index=0;index<snapshot_count;index++)if(!memcmp(actions,snapshots[index],sizeof(g_motifs[slot].events[step].actions))){found=1;break;}
        if(!found){if(snapshot_count>=128)return -1;snapshots[snapshot_count++]=actions;}
    }
    used=hb_mt_write_hex(buffer,length,used,(unsigned)snapshot_count,2);
    for(int index=0;index<snapshot_count;index++){
        const unsigned long long *actions=snapshots[index];unsigned long long baseline=actions[0],mask=0;int balance=0;
        for(int lane=0;lane<=HB_MOTION_LANES;lane++){if(!balance)baseline=actions[lane];balance+=actions[lane]==baseline?1:-1;}
        /* An exact common baseline plus exceptions preserves even inactive/evolving words. */
        for(int lane=0;lane<=HB_MOTION_LANES;lane++)if(actions[lane]!=baseline)mask|=1ULL<<lane;
        used=hb_mt_write_hex(buffer,length,used,(unsigned)(baseline>>32),8);used=hb_mt_write_hex(buffer,length,used,(unsigned)baseline,8);
        used=hb_mt_write_hex(buffer,length,used,(unsigned)(mask>>32),8);used=hb_mt_write_hex(buffer,length,used,(unsigned)mask,8);
        for(int lane=0;lane<=HB_MOTION_LANES;lane++)if(mask&(1ULL<<lane)){used=hb_mt_write_hex(buffer,length,used,(unsigned)(actions[lane]>>32),8);used=hb_mt_write_hex(buffer,length,used,(unsigned)actions[lane],8);}
    }
    for(int slot=0;slot<HB_MT_SLOTS;slot++){
        const hb_mt_phrase *phrase=&g_motifs[slot];
        used=hb_mt_write_hex(buffer,length,used,(unsigned)phrase->count,2);
        used=hb_mt_write_hex(buffer,length,used,(unsigned)(phrase->count?phrase->anchor:0),2);
        for(int step=0;step<phrase->count;step++){
            const hb_mt_event *event=&phrase->events[step];
            unsigned fields[]={event->kind,event->duration,event->count,event->relation,event->modifier+1,event->secondary,event->cadence,event->chord_mode,event->scale,event->flags};
            const int digits[]={1,3,1,1,1,2,2,1,3,4};
            for(int field=0;field<10;field++)used=hb_mt_write_hex(buffer,length,used,fields[field],digits[field]);
            int snapshot=0;while(snapshot<snapshot_count&&memcmp(event->actions,snapshots[snapshot],sizeof(event->actions)))snapshot++;
            used=hb_mt_write_hex(buffer,length,used,(unsigned)snapshot,2);
            for(int voice=0;voice<event->count;voice++){used=hb_mt_write_hex(buffer,length,used,event->notes[voice].pitch,2);used=hb_mt_write_hex(buffer,length,used,event->notes[voice].velocity,2);}
        }
    }
    if(used>=length)return -1;
    used+=snprintf(buffer+used,(size_t)(length-used),";mp1,%d,%d,%d,%d;mg1,%d,%d",editor->playback,editor->preset,editor->tap_grid,editor->completion,g_motif_rhythm,g_motif_span);
    if(used>=0&&used<length)used+=snprintf(buffer+used,(size_t)(length-used),";rr1,%d,%d,%d,%d",instance->rhythm_mode,instance->rhythm_pattern,instance->rhythm_window,g_render_window);
    return used>=length?-1:used;
}
static void hb_mt_restore(Inst *instance,const char *state){
    const char *marker=strstr(state,";mf1,");if(!marker)return;
    int selected,duration,relation,arrival,late,span,target,offset=0;
    if(sscanf(marker,";mf1,%d,%d,%d,%d,%d,%d,%d:%n",&selected,&duration,&relation,&arrival,&late,&span,&target,&offset)!=7||!offset)return;
    if(selected<0||selected>=16||duration<0||duration>=13||relation<0||relation>2||arrival<0||arrival>5||late<0||late>2||span<0||span>2||target<0||target>6)return;
    /* Restore atomically: malformed/truncated libraries cannot partially replace a live bank. */
    hb_mt_phrase *bank=(hb_mt_phrase*)malloc(sizeof(g_motifs));if(!bank)return;memset(bank,0,sizeof(g_motifs));
    const char *cursor=marker+offset;int valid=1;
    unsigned snapshot_count=0;
    unsigned long long (*snapshots)[HB_MOTION_LANES+1]=malloc(128*sizeof(*snapshots));
    if(!snapshots){free(bank);return;}
    if(!hb_mt_read_hex(&cursor,&snapshot_count,2)||snapshot_count>128)valid=0;
    for(unsigned index=0;index<snapshot_count&&valid;index++){
        unsigned high=0,low=0,mask_high=0,mask_low=0;
        if(!hb_mt_read_hex(&cursor,&high,8)||!hb_mt_read_hex(&cursor,&low,8)||!hb_mt_read_hex(&cursor,&mask_high,8)||!hb_mt_read_hex(&cursor,&mask_low,8)||mask_high>0xFFFFF){valid=0;break;}
        unsigned long long baseline=((unsigned long long)high<<32)|low,mask=((unsigned long long)mask_high<<32)|mask_low;
        for(int lane=0;lane<=HB_MOTION_LANES;lane++){
            snapshots[index][lane]=baseline;
            if(mask&(1ULL<<lane)){
                if(!hb_mt_read_hex(&cursor,&high,8)||!hb_mt_read_hex(&cursor,&low,8)){valid=0;break;}
                snapshots[index][lane]=((unsigned long long)high<<32)|low;
            }
        }
    }
    int total=0;
    for(int slot=0;slot<16&&valid;slot++){
        unsigned count=0,anchor=0;
        if(!hb_mt_read_hex(&cursor,&count,2)||!hb_mt_read_hex(&cursor,&anchor,2)||count>HB_MT_STEPS||(count&&anchor>=count)){valid=0;break;}
        total+=(int)count;if(total>128){valid=0;break;}
        bank[slot].count=(int)count;bank[slot].anchor=(int)anchor;
        for(unsigned step=0;step<count&&valid;step++){
            unsigned fields[10];const int digits[]={1,3,1,1,1,2,2,1,3,4};
            for(int field=0;field<10;field++)if(!hb_mt_read_hex(&cursor,&fields[field],digits[field])){valid=0;break;}
            if(!valid)break;
            if(fields[0]>2||!fields[1]||fields[1]>1536||fields[2]>8||fields[3]>2||fields[4]>3||fields[5]>15||fields[6]>HB_CADENCE_COUNT*HB_CADENCE_STEPS||fields[7]>3||(!fields[0]&&!fields[2])||(fields[0]&&fields[2])){valid=0;break;}
            hb_mt_event *event=&bank[slot].events[step];
            event->kind=fields[0];event->duration=fields[1];event->count=fields[2];event->relation=fields[3];event->modifier=(int)fields[4]-1;event->secondary=fields[5];event->cadence=fields[6];event->chord_mode=fields[7];event->scale=fields[8];event->flags=fields[9];
            if(event->kind==2&&(!step||bank[slot].events[step-1].kind==1)){valid=0;break;}
            unsigned snapshot=0;if(!hb_mt_read_hex(&cursor,&snapshot,2)||snapshot>=snapshot_count){valid=0;break;}
            memcpy(event->actions,snapshots[snapshot],sizeof(event->actions));
            for(int voice=0;voice<event->count;voice++){
                unsigned pitch=0,velocity=0;
                if(!hb_mt_read_hex(&cursor,&pitch,2)||!hb_mt_read_hex(&cursor,&velocity,2)||pitch>127||!velocity||velocity>127){valid=0;break;}
                event->notes[voice]=(hb_mt_note){(unsigned char)pitch,(unsigned char)velocity};
            }
        }
        if(count&&bank[slot].events[anchor].kind)valid=0;
    }
    if(valid&&(*cursor==';'||!*cursor)){
        hb_mt_recorder *editor=&instance->motif.editor;editor->selected=selected;editor->duration=duration;editor->relation=relation;editor->arrival=arrival;editor->late=late;editor->span=span;editor->target=target;
        if(!g_motif_timing_restored){
            int rhythm=0,global_span=span;const char *timing=strstr(state,";mg1,");
            if(!timing||(sscanf(timing,";mg1,%d,%d",&rhythm,&global_span)==2&&rhythm>=0&&rhythm<6&&global_span>=0&&global_span<3)){
                g_motif_rhythm=rhythm;g_motif_span=global_span;g_motif_timing_restored=1;
            }
        }
        if(!g_motifs_restored){memcpy(g_motifs,bank,sizeof(g_motifs));g_motifs_restored=1;}
    }
    if(valid){
        int mode=0,preset=0,grid=1,completion=0;
        const char *play=strstr(state,";mp1,");
        if(!play||(sscanf(play,";mp1,%d,%d,%d,%d",&mode,&preset,&grid,&completion)==4&&mode>=0&&mode<4&&preset>=0&&preset<HB_MT_PRESET_COUNT&&grid>=0&&grid<4&&completion>=0&&completion<2)){
            instance->motif.editor.playback=mode;instance->motif.editor.preset=preset;
            instance->motif.editor.tap_grid=grid;instance->motif.editor.completion=completion;
            instance->motif.tap_active=0;
        }
    }
    free(snapshots);free(bank);
}
static int hb_mt_free_slot(void){
    for(int slot=0;slot<HB_MT_SLOTS;slot++){
        if(g_motifs[slot].count)continue;
        int busy=0;
        for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].motif.editor.recording==slot)busy=1;
        if(!busy)return slot;
    }return -1;
}
static void hb_mt_lane_begin(Inst *instance,int duplicate){
    hb_mt_recorder *editor=&instance->motif.editor;
    if(editor->lane<0){hb_mt_begin(editor,editor->selected);return;}
    hb_motion_lane *lane=&instance->motion.lanes[editor->lane];
    if(lane->operation!=HB_MO_MOTIF){editor->error=11;return;}
    int reference=lane->amount,slot=reference>=20&&reference<36&&!duplicate?reference-20:hb_mt_free_slot();
    if(slot<0){editor->error=9;return;}
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&&g_pool[index]!=instance&&g_pool[index].motif.editor.recording==slot){editor->error=11;return;}
    hb_mt_begin(editor,slot);
    if(hb_mt_reference_preset(reference)){hb_mt_preset(hb_mt_reference_preset(reference),&editor->draft);editor->changed=1;}
    else if(duplicate&&reference>=20&&reference<36){editor->draft=g_motifs[reference-20];editor->changed=1;}
}
static int hb_mt_set(Inst *instance,const char *key,const char *value){
    if(strncmp(key,"motif_",6))return 0;
    hb_mt_recorder *editor=&instance->motif.editor;
    if(!strcmp(value,"Off"))return 1;
    if(!strcmp(key,"motif_edit")){
        if(editor->recording>=0)return 1;
        int lane=instance->motion.selected;
        if(lane<0||lane>=16||instance->motion.lanes[lane].operation!=HB_MO_MOTIF){editor->error=11;return 1;}
        editor->lane=lane;hb_mt_lane_load(editor,&instance->motion.lanes[lane]);editor->error=0;return 1;
    }
    if(!strcmp(key,"motif_close")){if(editor->recording<0)editor->lane=-1;return 1;}
    if(editor->lane>=0){
        hb_motion_lane *lane=&instance->motion.lanes[editor->lane];int changed=1;
        if(!strcmp(key,"motif_playback"))lane->motif_playback=enum_index(value,HB_MT_PLAYBACK,4,lane->motif_playback);
        else if(!strcmp(key,"motif_arrival"))lane->motif_arrival=enum_index(value,HB_MT_ARRIVALS,6,lane->motif_arrival);
        else if(!strcmp(key,"motif_target"))lane->motif_target=enum_index(value,HB_MT_TARGETS,7,lane->motif_target);
        else if(!strcmp(key,"motif_late"))lane->motif_late=enum_index(value,HB_MT_LATE,3,lane->motif_late);
        else if(!strcmp(key,"motif_grid"))lane->motif_grid=enum_index(value,HB_MT_GRIDS,4,lane->motif_grid);
        else if(!strcmp(key,"motif_completion"))lane->motif_completion=enum_index(value,HB_MT_COMPLETION,2,lane->motif_completion);
        else changed=0;
        if(changed){hb_motion_publish_settings(instance);hb_mt_lane_load(editor,lane);return 1;}
    }
    if(!strcmp(key,"motif_duplicate")){if(editor->recording<0)hb_mt_lane_begin(instance,1);return 1;}
    if(!strcmp(key,"motif_playback")){editor->playback=enum_index(value,HB_MT_PLAYBACK,4,editor->playback);instance->motif.tap_active=0;return 1;}
    if(!strcmp(key,"motif_preset")){editor->preset=enum_index(value,HB_MT_PRESETS,HB_MT_PRESET_COUNT,editor->preset);instance->motif.tap_active=0;editor->armed=-1;return 1;}
    if(!strcmp(key,"motif_grid")){editor->tap_grid=enum_index(value,HB_MT_GRIDS,4,editor->tap_grid);return 1;}
    if(!strcmp(key,"motif_completion")){editor->completion=enum_index(value,HB_MT_COMPLETION,2,editor->completion);return 1;}
    if(!strcmp(key,"motif_copy")){
        if(editor->preset&&editor->recording<0){
            hb_mt_begin(editor,editor->selected);hb_mt_preset(editor->preset,&editor->draft);
            editor->changed=1;editor->preset=0;instance->motif.tap_active=0;
        }return 1;
    }
    if(!strcmp(key,"motif_slot")){editor->selected=hb_cp_clamp(parse_i(value,editor->selected+1)-1,0,15);editor->preset=0;instance->motif.tap_active=0;return 1;}
    if(!strcmp(key,"motif_trigger")){int note=parse_i(value,60);if(note>=0&&note<=127)hb_mt_launch(instance,note,100,instance->source_channel>=0?instance->source_channel:0);return 1;}
    if(!strcmp(key,"motif_arm")){instance->motif.playback_lane=-1;int slot=parse_i(value,editor->selected+1)-1;if(slot>=0&&slot<16&&editor->recording<0){editor->selected=slot;instance->motif.tap_active=0;editor->armed=(editor->preset||g_motifs[slot].count)?slot:-1;editor->error=(editor->preset||g_motifs[slot].count)?0:10;}return 1;}
    if(!strcmp(key,"motif_record")){
        instance->motif.tap_active=0;
        if(!strcmp(value,"Cancel")){hb_mt_finish(editor,0);return 1;}
        if(editor->recording>=0){
            int slot=editor->recording;hb_mt_phrase previous=g_motifs[slot];
            if(editor->lane>=0&&instance->motion.lanes[editor->lane].operation!=HB_MO_MOTIF){editor->error=11;return 1;}
            if(hb_mt_finish(editor,1)){
                char encoded[4096];
                if(hb_mt_save(instance,encoded,sizeof(encoded),0)<0){g_motifs[slot]=previous;editor->recording=slot;editor->error=9;}
                else if(editor->lane>=0){instance->motion.lanes[editor->lane].amount=20+slot;editor->preset=0;hb_motion_publish_settings(instance);}
            }
        }else hb_mt_lane_begin(instance,0);return 1;
    }
    if(!strcmp(key,"motif_cancel")){instance->motif.cancel=1;instance->motif.tap_active=0;editor->armed=-1;hb_mt_finish(editor,0);return 1;}
    if(!strcmp(key,"motif_cursor")){
        if(editor->recording>=0){hb_mt_next(editor);editor->cursor=hb_cp_clamp(parse_i(value,1)-1,0,HB_MT_STEPS-1);}
        return 1;
    }
    if(!strcmp(key,"motif_step")){
        if(editor->recording>=0){
            hb_mt_next(editor);editor->cursor=hb_cp_clamp(parse_i(value,1)-1,0,HB_MT_STEPS-1);

        }return 1;
    }
    if(!strcmp(key,"motif_arrow")){
        if(editor->recording<0)return 1;
        int direction=parse_i(value,1);
        if(editor->open&&hb_mt_held(editor)){
            int head=editor->open_step+1;while(head<editor->draft.count&&editor->draft.events[head].kind==2)head++;
            if(direction>0){editor->cursor=head;hb_mt_event *event=hb_mt_entry(editor);if(event){memset(event,0,sizeof(*event));event->kind=2;event->duration=HB_MT_DURATIONS[editor->duration];}}
            else if(head>editor->open_step+1){hb_mt_checkpoint(editor);editor->draft.events[head-1].kind=1;if(head==editor->draft.count)editor->draft.count--;editor->cursor=head-2;}
        }else if(direction>0){
            if(editor->cursor>=editor->draft.count)hb_mt_rest(editor);else editor->cursor++;
        }else {editor->open=0;if(editor->cursor>0)editor->cursor--;}
        return 1;
    }
    if(!strcmp(key,"motif_rest")){hb_mt_rest(editor);return 1;}
    if(!strcmp(key,"motif_tie")){hb_mt_tie(editor);return 1;}
    if(!strcmp(key,"motif_next")){hb_mt_next(editor);return 1;}
    if(!strcmp(key,"motif_anchor")){
        if(editor->recording>=0){int step=editor->open?editor->open_step:editor->cursor;
            if(step<editor->draft.count&&editor->draft.events[step].count){hb_mt_checkpoint(editor);editor->draft.anchor=step;instance->motif.flash_serial++;instance->motif.flash_pitch=editor->draft.events[step].notes[0].pitch;instance->motif.flash_step=step;}
            else editor->anchor_next=1;}
        return 1;
    }
    if(!strcmp(key,"motif_undo")){if(editor->recording>=0&&editor->undo_valid){editor->draft=editor->undo;editor->undo_valid=0;editor->open=0;editor->cursor=editor->draft.count;editor->error=0;}return 1;}
    if(!strcmp(key,"motif_duration")){editor->duration=enum_index(value,HB_MT_DURATION_NAMES,13,editor->duration);return 1;}
    if(!strcmp(key,"motif_relation")){editor->relation=enum_index(value,HB_MT_RELATIONS,3,editor->relation);return 1;}
    if(!strcmp(key,"motif_arrival")){editor->arrival=enum_index(value,HB_MT_ARRIVALS,6,editor->arrival);return 1;}
    if(!strcmp(key,"motif_target")){editor->target=enum_index(value,HB_MT_TARGETS,7,editor->target);return 1;}
    if(!strcmp(key,"motif_late")){editor->late=enum_index(value,HB_MT_LATE,3,editor->late);return 1;}
    if(!strcmp(key,"motif_rhythm")){g_motif_rhythm=enum_index(value,HB_MT_RHYTHMS,6,g_motif_rhythm);g_motif_timing_restored=1;return 1;}
    if(!strcmp(key,"motif_span")){g_motif_span=enum_index(value,HB_MT_SPANS,3,g_motif_span);g_motif_timing_restored=1;return 1;}
    return 1;
}
static int hb_mt_get(Inst *instance,const char *key,char *buffer,int length){
    hb_mt_recorder *editor=&instance->motif.editor;
    if(!strcmp(key,"motif_lane"))return snprintf(buffer,(size_t)length,"%d",editor->lane+1);
    if(!strcmp(key,"motif_edit"))return snprintf(buffer,(size_t)length,"%s",instance->motion.lanes[instance->motion.selected].operation==HB_MO_MOTIF?"Edit Motif":"Play Motif only");
    if(!strcmp(key,"motif_playback"))return snprintf(buffer,(size_t)length,"%s",HB_MT_PLAYBACK[editor->playback]);
    if(!strcmp(key,"motif_preset"))return snprintf(buffer,(size_t)length,"%s",HB_MT_PRESETS[editor->preset]);
    if(!strcmp(key,"motif_grid"))return snprintf(buffer,(size_t)length,"%s",HB_MT_GRIDS[editor->tap_grid]);
    if(!strcmp(key,"motif_completion"))return snprintf(buffer,(size_t)length,"%s",HB_MT_COMPLETION[editor->completion]);
    if(!strcmp(key,"motif_slot"))return snprintf(buffer,(size_t)length,"%d",editor->selected+1);
    if(!strcmp(key,"motif_duration"))return snprintf(buffer,(size_t)length,"%s",HB_MT_DURATION_NAMES[editor->duration]);
    if(!strcmp(key,"motif_relation"))return snprintf(buffer,(size_t)length,"%s",HB_MT_RELATIONS[editor->relation]);
    if(!strcmp(key,"motif_arrival"))return snprintf(buffer,(size_t)length,"%s",HB_MT_ARRIVALS[editor->arrival]);
    if(!strcmp(key,"motif_target"))return snprintf(buffer,(size_t)length,"%s",HB_MT_TARGETS[editor->target]);
    if(!strcmp(key,"motif_late"))return snprintf(buffer,(size_t)length,"%s",HB_MT_LATE[editor->late]);
    if(!strcmp(key,"motif_rhythm"))return snprintf(buffer,(size_t)length,"%s",HB_MT_RHYTHMS[g_motif_rhythm]);
    if(!strcmp(key,"motif_span"))return snprintf(buffer,(size_t)length,"%s",HB_MT_SPANS[g_motif_span]);
    if(!strcmp(key,"motif_record"))return snprintf(buffer,(size_t)length,"%s",editor->recording>=0?"Done":"Edit");
    if(!strcmp(key,"motif_status")){
        static const char *errors[]={"","32 step limit","8 note limit","Tie needs a note","Duration limit","No future harmony","Choose bar for defer","Target unavailable","Phrase queue full","Motif bank full","Empty: Edit motif","Lane changed or busy"};
        if(editor->error)return snprintf(buffer,(size_t)length,"%s",errors[hb_cp_clamp(editor->error,0,11)]);
        if(instance->motif.tap_active){
            hb_mt_runtime *runtime=&instance->motif;
            if(editor->playback==1)return snprintf(buffer,(size_t)length,"Tap %d/%d Anchor %d",runtime->tap_step+1,runtime->tap_phrase.count,runtime->tap_phrase.anchor+1);
            double delta=runtime->tap_arrival-hb_motion_position(instance);
            return snprintf(buffer,(size_t)length,"Tap %d/%d A%+.2fb",runtime->tap_step+1,runtime->tap_phrase.count,delta);
        }
        if(editor->preset&&editor->recording<0)return snprintf(buffer,(size_t)length,"%s: %s",HB_MT_PRESETS[editor->preset],editor->armed>=0?"play target":"Arm to play");
        return snprintf(buffer,(size_t)length,editor->recording>=0?"REC %d Step %d":editor->armed>=0?"Slot %d: play target":"Slot %d: %d steps",editor->selected+1,editor->recording>=0?editor->cursor+1:g_motifs[editor->selected].count);
    }
    if(!strcmp(key,"motif_row")){
        unsigned occupied=0;for(int slot=0;slot<16;slot++)if(g_motifs[slot].count)occupied|=1u<<slot;
        int tapping=instance->motif.tap_active;
        hb_mt_phrase builtin;
        const hb_mt_phrase *display=tapping?&instance->motif.tap_phrase:&editor->draft;
        if(!tapping&&editor->lane>=0&&editor->recording<0){
            int reference=instance->motion.lanes[editor->lane].amount;
            if(hb_mt_reference_preset(reference)){hb_mt_preset(hb_mt_reference_preset(reference),&builtin);display=&builtin;}
            else if(reference>=20&&reference<36)display=&g_motifs[reference-20];
            else {memset(&builtin,0,sizeof(builtin));builtin.anchor=-1;display=&builtin;}
        }
        int used=snprintf(buffer,(size_t)length,"%d,%d,%d,%d,%u,%d",tapping?-2:editor->recording,editor->selected,tapping?instance->motif.tap_step:editor->cursor,editor->armed,occupied,display->anchor);
        for(int step=0;step<HB_MT_STEPS&&used<length;step++){
            const hb_mt_event *event=&display->events[step];int kind=step>=display->count?0:event->kind==1?3:event->kind==2?4:(event->modifier||event->secondary||event->cadence)?2:1;
            used+=snprintf(buffer+used,(size_t)(length-used),",%d",kind);
        }
        if(used<length)used+=snprintf(buffer+used,(size_t)(length-used),",%d,%d,%d",instance->motif.flash_serial,instance->motif.flash_pitch,instance->motif.flash_step);return used;
    }
    if(!strncmp(key,"motif_",6))return snprintf(buffer,(size_t)length,"Off");return -1;
}
