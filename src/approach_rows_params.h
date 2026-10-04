/* Dedicated approach layouts give knobs exclusive ownership of spatial assignments. */
static int hb_ar_single_layout(const Inst *instance){
    if(!instance->approach_layout||instance->preview_count!=32)return 0;
    for(int index=0;index<32;index++)if(instance->preview_rows[index])return 0;
    return 1;
}
static int hb_ar_spatial_layout(const Inst *instance){return instance->approach_layout!=0;}
static int hb_ar_slot(const char *key,const char *prefix,int count){
    size_t length=strlen(prefix);if(strncmp(key,prefix,length))return -1;
    char *end;long value=strtol(key+length,&end,10);return !*end&&value>=1&&value<=count?(int)value-1:-1;
}
static int hb_ar_set(Inst *instance,const char *key,const char *value){
    hb_ar_state *state=&instance->approach_rows;
    if(!strcmp(value,"Secondary LT")||!strcmp(value,"Chromatic Below")||!strcmp(value,"Chrom Below")||!strcmp(value,"CCB"))value="Connector Below";
    else if(!strcmp(value,"Chromatic Above")||!strcmp(value,"Chrom Above")||!strcmp(value,"CCA"))value="Connector Above";
    else if(!strcmp(value,"Tritone V")||!strcmp(value,"TTS"))value="Tritone Sub";
    else if(!strcmp(value,"LT"))value="Leading Tone";
    if(!strcmp(key,"approach_motif_latch")){
        if(hb_ar_spatial_layout(instance))return 1;
        int on=!strcmp(value,"On")||!strcmp(value,"1");
        if(on&&state->count&&state->selected){
            if(!state->performance)state->cursor=state->event=0;
            state->motif_latch=state->latch=state->performance=1;
        }else if(!on){state->motif_latch=state->latch=state->performance=0;}
        return 1;
    }
    if(!strcmp(key,"approach_mode_active")){state->enabled=parse_i(value,0)!=0;state->down=state->knob_down=state->step_down=state->turned=0;state->performance=0;state->latch=state->motif_latch=0;state->bank_armed=-1;return 1;}
    int control=hb_ar_slot(key,"approach_control_",16);
    if(control>=0){
        if(hb_ar_spatial_layout(instance))return 1;
        unsigned bit=1u<<control;int on=!strcmp(value,"LatchOn")||!strcmp(value,"On")||!strcmp(value,"1");
        if(on)state->latch_slots|=bit;else state->latch_slots&=~bit;
        state->turned|=bit;
        /* A composed phrase owns its finite cursor, even if a member's saved
           latch preference changes while the controls are still touched. */
        if(state->count>1&&state->performance)return 1;
        if(on||(state->selected&bit))state->latch=on;
        if(on){state->order[0]=hb_ar_code(state,control);state->order_slot[0]=control;state->count=1;state->cursor=state->event=0;state->selected=bit;state->performance=1;}
        else if(state->selected&bit)state->performance=0;
        return 1;
    }
    int slot=hb_ar_slot(key,"approach_knob_",8);
    if(slot>=0){state->knobs[slot]=enum_index(!strcmp(value,"Chromatic Below")||!strcmp(value,"Chrom Below")?"Secondary LT":value,HB_AR_NAMES,35,state->knobs[slot]);
        int choice=state->knobs[slot];state->bank[slot]=(choice<13||choice>=29)?-(choice+1):state->bank[choice-13];
        for(int index=0;index<state->count;index++)if(state->order_slot[index]==slot){state->order[index]=hb_ar_code(state,slot);state->event=0;}
        return 1;}
    slot=hb_ar_slot(key,"approach_bank_",16);
    if(slot>=0){int parsed=enum_index(value,MO_MOTIFS+1,43,-1);if(parsed>=0)state->bank[slot]=parsed+1;else for(int choice=0;choice<35;choice++)if((choice<13||choice>=29)&&choice!=2&&!strcmp(value,hb_ar_name(choice))){state->bank[slot]=-(choice+1);break;}state->sequence_cursor=state->sequence_event=state->row_event=0;for(int row=0;row<3;row++)if(state->row_slots[row]==slot)state->row_steps[row]=0;for(int index=0;index<state->count;index++)if(state->order_slot[index]==slot){state->order[index]=hb_ar_code(state,slot);state->event=0;}return 1;}
    int row_knob=hb_ar_slot(key,"approach_touch_",16);
    if(row_knob>=0&&hb_ar_spatial_layout(instance)){
        unsigned bit=1u<<row_knob;
        if(!strcmp(value,"Down")){
            if(!(state->knob_down&bit)){
                if(hb_ar_single_layout(instance))hb_ar_sequence_touch(state,row_knob);
                else state->sequence_count=0;
                hb_ar_row_touch(state,row_knob);
            }
            state->knob_down|=bit;
        }else state->knob_down&=~bit;
        /* Spatial knob holds never arm, extend or cancel a step gesture. */
        state->down=state->step_down;return 1;
    }
    int step_touch=hb_ar_slot(key,"approach_step_touch_",16);
    slot=step_touch>=0?step_touch:hb_ar_slot(key,"approach_touch_",16);
    if(slot>=0){
        unsigned bit=1u<<slot;unsigned *physical=step_touch>=0?&state->step_down:&state->knob_down;
        if(!strcmp(value,"Cancel")){
            *physical&=~bit;state->down=(hb_ar_spatial_layout(instance)?0:state->knob_down)|state->step_down;
            if(state->count==1){if(!state->latch)state->performance=0;}
            else {
                for(int index=state->count-1;index>=0;index--)if(state->order_slot[index]==slot){
                    for(int next=index+1;next<state->count;next++){state->order[next-1]=state->order[next];state->order_slot[next-1]=state->order_slot[next];}
                    state->count--;if(index<state->cursor)state->cursor--;else if(index==state->cursor)state->event=0;
                }
                state->selected&=~bit;if(state->cursor>=state->count){state->cursor=0;state->performance=0;}
            }
            return 1;
        }
        if(!strcmp(value,"Down")){
            if(*physical&bit)return 1;*physical|=bit;state->turned&=~bit;
            if(state->down&bit)return 1;
            if(!state->down){state->motif_latch=0;state->count=0;state->cursor=state->event=0;state->selected=0;state->used=0;state->latch=!hb_ar_spatial_layout(instance)&&step_touch<0&&!!(state->latch_slots&bit);}
            state->performance=1;state->touched_at[slot]=hb_motion_position(instance);
            if(state->count<8){state->order_slot[state->count]=slot;state->order[state->count++]=hb_ar_code(state,slot);}
            state->selected|=bit;
            if(state->count>1&&!state->motif_latch)state->latch=0;
            state->down|=bit;
        }else{*physical&=~bit;state->down=(hb_ar_spatial_layout(instance)?0:state->knob_down)|state->step_down;int elapsed=parse_i(strchr(value,',')?strchr(value,',')+1:"0",0);if(!state->down&&!state->latch&&state->count==1&&!(state->turned&bit)&&(elapsed>=g_hb_hold_ms||state->used))state->performance=0;}
        return 1;
    }
    if(!strcmp(key,"approach_trigger")){state->bank_armed=hb_cp_clamp(parse_i(value,0),0,16)-1;return 1;}
    return 0;
}
/* Use the same preset dictionary and event boundaries as the player. */
static void hb_ar_step_label(int code,int step,const hb_mt_phrase *phrase,char *label,int length){
    int preset=hb_mt_reference_preset(code-15);
    if(preset){
        const char *start=HB_MT_PRESETS[preset];
        for(int index=0;index<step&&*start;index++){const char *next=strchr(start,'-');if(!next)break;start=next+1;}
        const char *end=strchr(start,'-');int count=end?(int)(end-start):(int)strlen(start);
        snprintf(label,(size_t)length,"%.*s",count,start);return;
    }
    if(phrase){snprintf(label,(size_t)length,phrase->events[step].kind==1?"Rest":"U%d.%d",code-34,step+1);return;}
    static const char *names[]={"","CCB","CCA","Up","ii","V","vi","iv","bVII","iiT","TTS","iii","IV","vii","LT"};
    snprintf(label,(size_t)length,"%s",code==62?"iiD":code==63?"ivD":code==59?"viD":code==61?"5th":code==60?"UD":code>0&&code<15?names[code]:"?");
}
static int hb_ar_sequence_view(Inst *instance,char *buffer,int length){
    hb_ar_state *state=&instance->approach_rows;
    int spatial=hb_ar_spatial_layout(instance),single=hb_ar_single_layout(instance);
    int entries=spatial?(single?(state->sequence_count?state->sequence_count:1):3):state->performance?state->count:0;
    if(!entries)return snprintf(buffer,(size_t)length,"0,0,0");
    char labels[HB_MT_STEPS*8+1][24];int count=0,selected=0;
    for(int entry=0;entry<entries;entry++){
        int code=spatial?hb_ar_code(state,single?(state->sequence_count?state->sequence_slots[entry]:state->row_preset):state->row_slots[2-entry]):state->order[entry];
        hb_mt_phrase builtin;const hb_mt_phrase *phrase=hb_ar_phrase(code,&builtin);
        int end=phrase?phrase->count:1;
        if(spatial&&phrase)end=single?(phrase->anchor>0?phrase->anchor:phrase->count):1;
        for(int event=0;event<end;event++){
            if(phrase&&phrase->events[event].kind==2)continue;
            if(entry==(spatial?state->sequence_cursor:state->cursor)&&event==(spatial?state->sequence_event:state->event))selected=count;
            hb_ar_step_label(code,event,phrase,labels[count++],24);
        }
    }
    if(spatial&&!single)selected=-1; /* Three rows are spatial choices, not a tap cursor. */
    if(spatial)snprintf(labels[count++],24,"T"); /* Explicit resolution, never consumed. */
    int begin=selected>3?selected-3:0,end=begin+7;if(end>count)end=count;
    int used=snprintf(buffer,(size_t)length,"%d,%d,%d",selected,count,begin);
    for(int index=begin;index<end&&used>=0&&used<length;index++)used+=snprintf(buffer+used,(size_t)(length-used),"|%s",labels[index]);
    return used;
}
static int hb_ar_get(Inst *instance,const char *key,char *buffer,int length){
    if(!strcmp(key,"approach_sequence_view"))return hb_ar_sequence_view(instance,buffer,length);
    hb_ar_state *state=&instance->approach_rows;int slot=hb_ar_slot(key,"approach_knob_",8);
    if(!strcmp(key,"approach_latch_slots"))return snprintf(buffer,(size_t)length,"%u",state->latch_slots);
    int control=hb_ar_slot(key,"approach_control_",16);
    if(control>=0)return snprintf(buffer,(size_t)length,"%s",state->bank[control]<0?hb_ar_name(-state->bank[control]-1):MO_MOTIFS[state->bank[control]]);
    if(slot>=0)return snprintf(buffer,(size_t)length,"%s",hb_ar_name(state->knobs[slot]));
    slot=hb_ar_slot(key,"approach_bank_",16);
    if(slot>=0)return snprintf(buffer,(size_t)length,"%s",state->bank[slot]<0?hb_ar_name(-state->bank[slot]-1):MO_MOTIFS[state->bank[slot]]);
    if(!strcmp(key,"approach_motif_latch"))return snprintf(buffer,(size_t)length,"%s",hb_ar_spatial_layout(instance)?"Rows":state->motif_latch&&state->performance?"On":"Off");
    if(!strcmp(key,"approach_row_status")){unsigned sequence_mask=0;for(int index=0;index<state->sequence_count;index++)sequence_mask|=1u<<state->sequence_slots[index];int used=snprintf(buffer,(size_t)length,"%d,%d,%d,%u,%d,%u,%d,%d,%d,%d,%d,%d,%u,%d,%d,%d,%d",state->enabled,state->cursor,state->count,state->down,state->bank_armed,state->selected,state->performance,state->row_preset,state->latch,state->row_slots[0],state->row_slots[1],state->row_slots[2],sequence_mask,state->sequence_count?state->sequence_slots[state->sequence_cursor]:state->row_preset,state->sequence_cursor,state->sequence_event,state->sequence_count);for(int index=0;index<state->sequence_count&&used<length;index++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",state->sequence_slots[index]);return used;}
    if(!strcmp(key,"approach_rows_view")){int used=0;for(int row=0;row<3;row++){int slot=state->row_slots[row],ref=state->bank[slot];used+=snprintf(buffer+used,(size_t)(length-used),"%s%d,%s",row?"|":"",slot+1,ref<0?hb_ar_name(-ref-1):MO_MOTIFS[ref]);if(used>=length)return length-1;}return used;}
    return -1;
}
static int hb_ar_save(Inst *instance,char *buffer,int length,int used){
    if(used<0||used>=length)return used;
    hb_ar_state *state=&instance->approach_rows;
    used+=snprintf(buffer+used,(size_t)(length-used),";ar1");
    for(int index=0;index<8&&used<length;index++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",state->knobs[index]);
    for(int index=0;index<16&&used<length;index++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",state->bank[index]);
    if(used<length)used+=snprintf(buffer+used,(size_t)(length-used),",%d",state->count);
    for(int index=0;index<state->count&&used<length;index++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",state->order[index]);
    if(used<length)used+=snprintf(buffer+used,(size_t)(length-used),";ar2,%d;ar3,%d,%d,%d;ar4,%u",state->row_preset,state->row_slots[0],state->row_slots[1],state->row_slots[2],state->latch_slots);
    if(used<length)used+=snprintf(buffer+used,(size_t)(length-used),";ar5,1");
    if(state->sequence_count&&used<length){
        used+=snprintf(buffer+used,(size_t)(length-used),";ar6,%d",state->sequence_count);
        for(int index=0;index<state->sequence_count&&used<length;index++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",state->sequence_slots[index]);
    }
    return used>=length?-1:used;
}
static void hb_ar_restore(Inst *instance,const char *source){
    const char *cursor=strstr(source,";ar1,");if(!cursor)return;cursor+=4;
    hb_ar_state restored=instance->approach_rows;int count=25;
    for(int index=0;index<count;index++){
        if(*cursor++!=',')return;char *end;long value=strtol(cursor,&end,10);if(end==cursor)return;cursor=end;
        if(index<8){if(value<0||value>=35)return;restored.knobs[index]=(int)value;}
        else if(index<24){if(value==0||(value< -13&&(value> -30||value< -35))||value>43)return;restored.bank[index-8]=(int)value;}
        else if(index==24){if(value<1||value>8)return;restored.count=(int)value;count+=value;}
        else{if(value<1||value>63)return;restored.order[index-25]=(int)value;}
    }
    if(*cursor&&*cursor!=';')return;restored.down=restored.knob_down=restored.step_down=restored.selected=0;for(int index=0;index<8;index++)restored.order_slot[index]=-1;restored.cursor=restored.event=0;restored.bank_armed=-1;
    const char *extra=strstr(source,";ar2,");
    if(extra){char *end;long selected=strtol(extra+5,&end,10);if(end==extra+5||(*end&&*end!=';')||selected<0||selected>=16)return;restored.row_preset=(int)selected;}
    else {int original[16];memcpy(original,restored.bank,sizeof(original));for(int slot=0;slot<8;slot++){int choice=restored.knobs[slot];restored.bank[slot]=(choice<13||choice>=29)?-(choice+1):original[choice-13];}}
    const char *rows=strstr(source,";ar3,");
    if(rows){rows+=4;for(int row=0;row<3;row++){if(*rows++!=',')return;char *end;long slot=strtol(rows,&end,10);if(end==rows||slot<0||slot>=16)return;restored.row_slots[row]=(int)slot;rows=end;}if(*rows&&*rows!=';')return;}
    else for(int row=0;row<3;row++)restored.row_slots[row]=2-row;
    const char *latches=strstr(source,";ar4,");restored.latch_slots=restored.turned=0;
    if(latches){char *end;long mask=strtol(latches+5,&end,10);if(end==latches+5||(*end&&*end!=';')||mask<0||mask>65535)return;restored.latch_slots=(unsigned)mask;}
    /* Migrate only untouched legacy factory slots; keep custom assignments. */
    if(!strstr(source,";ar5,1"))for(int slot=0;slot<16;slot++)
        if(restored.bank[slot]==slot+1)restored.bank[slot]=HB_AR_DEFAULT_BANK[slot];
    if(instance->approach_layout)restored.latch_slots=0;
    memset(restored.row_steps,0,sizeof(restored.row_steps));
    restored.performance=restored.latch=restored.motif_latch=restored.row_event=0;restored.pending_row=restored.preview_row=-1;
    restored.sequence_count=restored.sequence_cursor=restored.sequence_event=0;restored.sequence_pad=0;
    const char *sequence=strstr(source,";ar6,");
    if(sequence){
        char *end;long entries=strtol(sequence+5,&end,10);if(entries<1||entries>8)return;
        for(int index=0;index<entries;index++){if(*end++!=',')return;char *start=end;long slot=strtol(start,&end,10);if(end==start||slot<0||slot>=16)return;restored.sequence_slots[index]=(int)slot;}
        if(*end&&*end!=';')return;restored.sequence_count=(int)entries;
    }
    instance->approach_rows=restored;
}
