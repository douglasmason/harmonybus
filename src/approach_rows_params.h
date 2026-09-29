static int hb_ar_slot(const char *key,const char *prefix,int count){
    size_t length=strlen(prefix);if(strncmp(key,prefix,length))return -1;
    char *end;long value=strtol(key+length,&end,10);return !*end&&value>=1&&value<=count?(int)value-1:-1;
}
static int hb_ar_set(Inst *instance,const char *key,const char *value){
    hb_ar_state *state=&instance->approach_rows;
    if(!strcmp(key,"approach_mode_active")){state->enabled=parse_i(value,0)!=0;state->down=0;state->bank_armed=-1;return 1;}
    int slot=hb_ar_slot(key,"approach_knob_",8);
    if(slot>=0){state->knobs[slot]=enum_index(!strcmp(value,"Chromatic Below")||!strcmp(value,"Chrom Below")?"Secondary LT":value,HB_AR_NAMES,29,state->knobs[slot]);
        for(int index=0;index<state->count;index++)if(state->order_slot[index]==slot){state->order[index]=hb_ar_code(state,slot);state->event=0;}
        return 1;}
    slot=hb_ar_slot(key,"approach_bank_",16);
    if(slot>=0){state->bank[slot]=hb_cp_clamp(enum_index(value,MO_MOTIFS+1,35,state->bank[slot]-1)+1,1,35);return 1;}
    slot=hb_ar_slot(key,"approach_touch_",8);
    if(slot>=0){
        unsigned bit=1u<<slot;
        if(!strcmp(value,"Down")){
            if(state->down&bit)return 1;
            if(!state->down){state->count=0;state->cursor=state->event=0;state->selected=0;}
            if(state->count<8){state->order_slot[state->count]=slot;state->order[state->count++]=hb_ar_code(state,slot);}
            state->selected|=bit;
            state->down|=bit;
        }else state->down&=~bit;
        return 1;
    }
    if(!strcmp(key,"approach_trigger")){state->bank_armed=hb_cp_clamp(parse_i(value,0),0,16)-1;return 1;}
    return 0;
}
static int hb_ar_get(Inst *instance,const char *key,char *buffer,int length){
    hb_ar_state *state=&instance->approach_rows;int slot=hb_ar_slot(key,"approach_knob_",8);
    if(slot>=0)return snprintf(buffer,(size_t)length,"%s",hb_ar_name(state->knobs[slot]));
    slot=hb_ar_slot(key,"approach_bank_",16);
    if(slot>=0)return snprintf(buffer,(size_t)length,"%s",MO_MOTIFS[state->bank[slot]]);
    if(!strcmp(key,"approach_row_status"))return snprintf(buffer,(size_t)length,"%d,%d,%d,%u,%d,%u",state->enabled,state->cursor,state->count,state->down,state->bank_armed,state->selected);
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
    return used>=length?-1:used;
}
static void hb_ar_restore(Inst *instance,const char *source){
    const char *cursor=strstr(source,";ar1,");if(!cursor)return;cursor+=4;
    hb_ar_state restored=instance->approach_rows;int count=25;
    for(int index=0;index<count;index++){
        if(*cursor++!=',')return;char *end;long value=strtol(cursor,&end,10);if(end==cursor)return;cursor=end;
        if(index<8){if(value<0||value>=29)return;restored.knobs[index]=(int)value;}
        else if(index<24){if(value<1||value>35)return;restored.bank[index-8]=(int)value;}
        else if(index==24){if(value<1||value>8)return;restored.count=(int)value;count+=value;}
        else{if(value<1||value>50||(value>13&&value<16))return;restored.order[index-25]=(int)value;}
    }
    if(*cursor&&*cursor!=';')return;restored.down=restored.selected=0;for(int index=0;index<8;index++)restored.order_slot[index]=-1;restored.cursor=restored.event=0;restored.bank_armed=-1;
    instance->approach_rows=restored;
}
