#ifndef HB_MOTION_PARAMS_H
#define HB_MOTION_PARAMS_H
#include "motion_metadata.h"
/* The selected lane is an editor cursor. Holds are runtime-only, never state. */
static const char *MO_OPERATIONS[]={"Off","Velocity","Pan","Octave","Rotate","Gate","Skip","Harmony","Chrom Below","Scale Above","Enclose Above Below","Enclose Below Above","Clip Repeat","Clip Reverse","Clip Time Shift","Clip Speed","Transpose","Ratchet","MIDI Echo"};
static const char *MO_PATTERNS[]={"Constant","Alternate","Rise","Fall","Triangle","Backbeat","Random"};
static const char *MO_GRIDS[]={"1/64","1/32","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars"};
static const char *MO_CYCLES[]={"1/8","1/4","1/2","1 Bar","2 Bars","3 Bars","4 Bars"};
static const char *MO_SWITCH[]={"Off","On"};
static const char *MO_GROUPS[]={"Chord","Voice"};
static const char *MO_ADVANCE[]={"Clock","Note","Chord"};
static const char *MO_TOUCH[]={"Hold","Latch","Tap/Hold"};
static const char *MO_TRIGGER_TOUCH[]={"Hold","Arm","Tap/Hold"};
static const char *const *hb_mo_touch_options(hb_motion_config *config){
    int operation=config->lanes[config->selected].operation;
    return operation>=HB_MO_BELOW&&operation<=HB_MO_ENCLOSE_BA?MO_TRIGGER_TOUCH:MO_TOUCH;
}
static const char *MO_RANDOM[]={"Repeat","Evolve"};
typedef struct { const char *key; size_t offset; int low,high; const char *const *options; } hb_motion_parameter;
#define MO_FIELD(name,low,high,options) {"motion_" #name,__builtin_offsetof(hb_motion_lane,name),low,high,options}
static const hb_motion_parameter MO_PARAMETERS[]={
    MO_FIELD(operation,0,18,MO_OPERATIONS),MO_FIELD(pattern,0,6,MO_PATTERNS),
    MO_FIELD(amount,-400,400,0),MO_FIELD(offset,-400,400,0),MO_FIELD(enabled,0,1,MO_SWITCH),
    MO_FIELD(grid,0,8,MO_GRIDS),MO_FIELD(cycle,0,6,MO_CYCLES),MO_FIELD(phase,-64,64,0),
    MO_FIELD(probability,0,100,0),MO_FIELD(group,0,1,MO_GROUPS),MO_FIELD(evolve,0,1,MO_RANDOM)
};
#undef MO_FIELD
static int *hb_mo_field(hb_motion_lane *lane,int index){return (int *)((char *)lane+MO_PARAMETERS[index].offset);}
static int hb_mo_slot_key(const char *key,const char *prefix){
    size_t size=strlen(prefix);if(strncmp(key,prefix,size))return -1;
    char *end=0;long slot=strtol(key+size,&end,10);
    return end!=key+size&&!*end&&slot>=1&&slot<=HB_MOTION_LANES?(int)slot-1:-1;
}
static int hb_mo_set(hb_motion_config *config,const char *key,const char *value){
    if(!strcmp(key,"touch_hold_ms")){g_hb_hold_ms=hb_mo_clamp(parse_i(value,g_hb_hold_ms),150,500);g_hb_hold_restored=1;return 1;}
    if(!strcmp(key,"motion_touch_mode")){config->lanes[config->selected].touch_mode=!strcmp(value,"Toggle")?1:enum_index(value,hb_mo_touch_options(config),3,config->lanes[config->selected].touch_mode);return 1;}
    int gesture=hb_mo_slot_key(key,"motion_gesture_");
    if(!strcmp(key,"performance_gesture_above"))gesture=16;
    if(!strcmp(key,"performance_gesture_below"))gesture=17;
    if(gesture>=0){
        int elapsed=-1;if(!strncmp(value,"Up,",3))elapsed=parse_i(value+3,-1);
        hb_mo_gesture(config,gesture,!strcmp(value,"Down"),elapsed);return 1;
    }
    if(!strcmp(key,"motion_host")){config->host_capabilities=!strcmp(value,"movy-clip-v2")?2:!strcmp(value,"movy-clip-v1");return 1;}
    if(!strcmp(key,"performance_reset")){
        config->held=0;config->pitch_held=0;config->enclosure=0;hb_mo_input_reset(config);hb_mo_gesture_reset(config);
        for(int lane=0;lane<HB_MOTION_LANES;lane++)config->revision[lane]++;return 1;
    }
    if(!strcmp(key,"performance_below")||!strcmp(key,"performance_above")){
        unsigned bit=!strcmp(key,"performance_below")?1u:2u;
        if(enum_index(value,MO_SWITCH,2,0)){
            config->pitch_held|=bit;config->pitch_last=(int)bit;
            config->enclosure=0; /* an explicit live hold replaces an armed enclosure */
        }else{
            config->pitch_held&=~bit;
            if(!(config->pitch_held&(unsigned)config->pitch_last))config->pitch_last=(int)config->pitch_held;
        }
        return 1;
    }
    if(!strcmp(key,"performance_enclose_ab")||!strcmp(key,"performance_enclose_ba")){
        if(strcmp(value,"0")&&strcmp(value,"Off")){
            int wanted=!strcmp(key,"performance_enclose_ab")?1:2;
            if(config->enclosure==wanted&&!config->tap_started){config->enclosure=0;config->tap_mask=0;}
            else {config->tap_mask=3;config->tap_first=wanted==1?2:1;hb_mo_tap_rebuild(config);}

        }
        return 1;
    }
    if(!strcmp(key,"motion_lane")){
        int lane=parse_i(value,config->selected+1);if(lane>=1&&lane<=HB_MOTION_LANES)config->selected=lane-1;return 1;
    }
    if(!strcmp(key,"motion_bypass")){config->bypass=enum_index(value,MO_SWITCH,2,config->bypass);return 1;}
    if(!strcmp(key,"motion_release")){config->held=0;return 1;}
    if(!strcmp(key,"motion_advance")){
        int selected=config->selected;int previous=config->lanes[selected].advance;
        config->lanes[selected].advance=enum_index(value,MO_ADVANCE,3,previous);
        if(previous!=config->lanes[selected].advance){config->events[selected]=0;config->revision[selected]++;}
        return 1;
    }
    if(!strcmp(key,"motion_every")||!strcmp(key,"motion_from")||!strcmp(key,"motion_through")){
        hb_motion_lane *lane=&config->lanes[config->selected];
        int every=lane->every,from=lane->from,through=lane->through;
        if(!strcmp(key,"motion_every")){
            lane->every=hb_mo_clamp(parse_i(value,every),1,16);
            lane->from=hb_mo_clamp(from,1,lane->every);lane->through=hb_mo_clamp(through,lane->from,lane->every);
        }else if(!strcmp(key,"motion_from")){
            lane->from=hb_mo_clamp(parse_i(value,from),1,every);
            if(lane->through<lane->from)lane->through=lane->from;
        }else{
            lane->through=hb_mo_clamp(parse_i(value,through),1,every);
            if(lane->from>lane->through)lane->from=lane->through;
        }
        if(every!=lane->every||from!=lane->from||through!=lane->through)config->revision[config->selected]++;
        return 1;
    }
    /* Fixed lane keys let a release follow its original lane after selection changes. */
    int slot=hb_mo_slot_key(key,"motion_hold_");
    if(slot>=0){
        unsigned bit=1u<<slot;int down=enum_index(value,MO_SWITCH,2,0);
        if(down&&!(config->held&bit)){
            config->held|=bit;config->held_serial[slot]=++config->serial;
            int operation=config->lanes[slot].operation;
            if(operation==HB_MO_BELOW||operation==HB_MO_ABOVE)config->enclosure=0;
            if(operation==HB_MO_ENCLOSE_AB||operation==HB_MO_ENCLOSE_BA){
                int wanted=operation==HB_MO_ENCLOSE_AB?1:2;
                if(config->enclosure==wanted&&!config->tap_started){config->enclosure=0;config->tap_mask=0;}
                else {config->tap_mask=3;config->tap_first=wanted==1?2:1;hb_mo_tap_rebuild(config);config->enclosure_lane=slot;}
            }
        }else if(!down)config->held&=~bit;
        return 1;
    }
    for(int index=0;index<11;index++){
        const hb_motion_parameter *spec=&MO_PARAMETERS[index];
        if(strcmp(key,spec->key))continue;
        int *field=hb_mo_field(&config->lanes[config->selected],index);
        int parsed=spec->options?enum_index(value,spec->options,spec->high+1,*field):hb_mo_clamp(parse_i(value,*field),spec->low,spec->high);
        if(index==0&&parsed!=*field){
            static const int amounts[]={0,25,50,1,1,50,100,100,1,1,1,1,1,1,1,2,1,4,3};
            config->lanes[config->selected].amount=amounts[parsed];
            config->lanes[config->selected].offset=parsed==HB_MO_ECHO?25:0;
            if(parsed>=HB_MO_ENCLOSE_AB&&parsed<=HB_MO_SPEED)config->lanes[config->selected].enabled=0;
        }
        if(index==4&&(config->lanes[config->selected].operation==HB_MO_ENCLOSE_AB||config->lanes[config->selected].operation==HB_MO_ENCLOSE_BA||
            (config->lanes[config->selected].operation>=HB_MO_REPEAT&&config->lanes[config->selected].operation<=HB_MO_SPEED&&config->host_capabilities<2)))parsed=0;
        if(index==3&&config->lanes[config->selected].operation==HB_MO_ECHO)parsed=hb_mo_clamp(parsed,0,100);
        if(*field!=parsed)config->revision[config->selected]++;
        *field=parsed;return 1;
    }
    return 0;
}
static int hb_mo_get(hb_motion_config *config,const char *key,char *buffer,int length){
    if(!strcmp(key,"touch_hold_ms"))return snprintf(buffer,(size_t)length,"%d",g_hb_hold_ms);
    if(!strcmp(key,"motion_touch_mode"))return snprintf(buffer,(size_t)length,"%s",hb_mo_touch_options(config)[config->lanes[config->selected].touch_mode]);
    int gesture_slot=hb_mo_slot_key(key,"motion_gesture_binding_");
    if(gesture_slot>=0){hb_motion_lane *lane=&config->lanes[gesture_slot];return snprintf(buffer,(size_t)length,"%d,%d,%d,%d,%d,%d",lane->operation,lane->amount,lane->grid,lane->touch_mode,g_hb_hold_ms,(config->gesture_latched>>gesture_slot)&1);}

    if(!strcmp(key,"chain_params")){
        int used=snprintf(buffer,(size_t)length,"%s{\"key\":\"motion_operation\",\"name\":\"Operation\",\"type\":\"enum\",\"options_as_string\":true,\"options\":[",HB_CHAIN_PARAMS_PREFIX);
        int count=0,selected=config->lanes[config->selected].operation;
        for(int operation=0;operation<19;operation++){
            if(operation>=HB_MO_REPEAT&&operation<=HB_MO_SPEED&&!config->host_capabilities&&operation!=selected)continue;
            if(used<0||used>=length)return -1;
            used+=snprintf(buffer+used,(size_t)(length-used),"%s\"%s\"",count++?",":"",MO_OPERATIONS[operation]);
        }
        if(used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),"]},{\"key\":\"motion_enabled\",\"name\":\"Auto\",\"type\":\"enum\",\"options_as_string\":true,\"options\":[\"Off\",\"On\"],\"readOnly\":%s}]",(selected==HB_MO_ENCLOSE_AB||selected==HB_MO_ENCLOSE_BA||(selected>=HB_MO_REPEAT&&selected<=HB_MO_SPEED&&config->host_capabilities<2))?"true":"false");
        if(used<0||used>=length)return -1;
        used--;
        used+=snprintf(buffer+used,(size_t)(length-used),",{\"key\":\"motion_touch_mode\",\"name\":\"Touch Mode\",\"type\":\"enum\",\"options_as_string\":true,\"options\":[\"Hold\",\"%s\",\"Tap/Hold\"]}]",hb_mo_touch_options(config)[1]);
        if(used<0||used>=length)return -1;
        used--; /* Replace the closing array bracket with contextual metadata. */
        used+=snprintf(buffer+used,(size_t)(length-used),",{\"key\":\"motion_offset\",\"name\":\"%s\",\"type\":\"int\",\"min\":%d,\"max\":100,\"step\":1,\"default\":0}]",selected==HB_MO_ECHO?"Decay %":"Offset",selected==HB_MO_ECHO?0:-100);
        if(used<0||used>=length)return -1;
        used--;
        const char *keys[]={"motion_from","motion_through"},*names[]={"From","Through"};
        for(int parameter=0;parameter<2;parameter++){
            if(used<0||used>=length)return -1;
            used+=snprintf(buffer+used,(size_t)(length-used),",{\"key\":\"%s\",\"name\":\"%s\",\"type\":\"enum\",\"options_as_string\":true,\"default\":\"1\",\"options\":[",keys[parameter],names[parameter]);
            /* Movy reloads dependent metadata immediately. Stock Schwung keeps a
               stable full list so increasing Every never leaves a one-item list cached. */
            int limit=config->host_capabilities?config->lanes[config->selected].every:16;
            for(int cycle=1;cycle<=limit;cycle++){
                if(used<0||used>=length)return -1;
                used+=snprintf(buffer+used,(size_t)(length-used),"%s\"%d\"",cycle>1?",":"",cycle);
            }
            if(used<0||used>=length)return -1;
            used+=snprintf(buffer+used,(size_t)(length-used),"]}");
        }
        if(used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),"]");
        return used>=length?-1:used;
    }
    if(!strcmp(key,"motion_clip_config")){
        int used=snprintf(buffer,(size_t)length,"mca1");
        for(int index=0;index<HB_MOTION_LANES;index++){
            const hb_motion_lane *lane=&config->lanes[index];
            if(config->host_capabilities<2||config->bypass||!lane->enabled||lane->operation<HB_MO_REPEAT||lane->operation>HB_MO_SPEED)continue;
            if(used<0||used>=length)return -1;
            used+=snprintf(buffer+used,(size_t)(length-used),";%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",index,lane->operation,lane->amount,lane->grid,lane->cycle,lane->every,lane->from,lane->through,lane->probability,lane->evolve);
        }
        return used>=length?-1:used;
    }
    if(!strcmp(key,"motion_every"))return snprintf(buffer,(size_t)length,"%d",config->lanes[config->selected].every);
    if(!strcmp(key,"motion_from"))return snprintf(buffer,(size_t)length,"%d",config->lanes[config->selected].from);
    if(!strcmp(key,"motion_through"))return snprintf(buffer,(size_t)length,"%d",config->lanes[config->selected].through);
    if(!strcmp(key,"motion_condition_range")){
        const hb_motion_lane *lane=&config->lanes[config->selected];
        if(lane->every==1)return snprintf(buffer,(size_t)length,"Every cycle");
        if(lane->from==lane->through)return snprintf(buffer,(size_t)length,"%d of %d",lane->from,lane->every);
        return snprintf(buffer,(size_t)length,"%d-%d of %d",lane->from,lane->through,lane->every);
    }
    if(!strcmp(key,"motion_advance"))return snprintf(buffer,(size_t)length,"%s",MO_ADVANCE[config->lanes[config->selected].advance]);
    if(!strcmp(key,"motion_lane"))return snprintf(buffer,(size_t)length,"%d",config->selected+1);
    if(!strcmp(key,"motion_bypass"))return snprintf(buffer,(size_t)length,"%s",MO_SWITCH[config->bypass]);
    if(!strcmp(key,"motion_punch")&&config->lanes[config->selected].operation>=HB_MO_REPEAT&&config->lanes[config->selected].operation<=HB_MO_SPEED&&!config->host_capabilities)return snprintf(buffer,(size_t)length,"Requires Movy");
    if(!strcmp(key,"motion_punch"))return snprintf(buffer,(size_t)length,"%s",hb_mo_lane_status(config,config->selected));
    int slot=hb_mo_slot_key(key,"motion_hold_");
    if(slot>=0)return snprintf(buffer,(size_t)length,"%s",MO_SWITCH[(config->held>>slot)&1]);
    slot=hb_mo_slot_key(key,"motion_binding_");
    if(slot>=0){hb_motion_lane *lane=&config->lanes[slot];return snprintf(buffer,(size_t)length,"%d,%d,%d",lane->operation,lane->amount,lane->grid);}
    if(!strcmp(key,"motion_overview")){
        int operation=config->lanes[config->selected].operation;
        return snprintf(buffer,(size_t)length,"%d/16 %s",config->selected+1,MO_OPERATIONS[operation]);
    }
    for(int index=0;index<11;index++)if(!strcmp(key,MO_PARAMETERS[index].key)){
        int field=*hb_mo_field(&config->lanes[config->selected],index);
        return MO_PARAMETERS[index].options?snprintf(buffer,(size_t)length,"%s",MO_PARAMETERS[index].options[field]):snprintf(buffer,(size_t)length,"%d",field);
    }
    return -1;
}
static int hb_mo_save(hb_motion_config *config,char *buffer,int length,int used){
    if(used<0||used>=length)return used;
    if(g_hb_hold_ms!=350)used+=snprintf(buffer+used,(size_t)(length-used),";gt1,%d",g_hb_hold_ms);
    for(int index=0;index<HB_MOTION_LANES;index++)if(config->lanes[index].touch_mode!=2&&used>=0&&used<length)
        used+=snprintf(buffer+used,(size_t)(length-used),";mt1,%d,%d",index,config->lanes[index].touch_mode);
    /* Omit untouched lanes so legacy/default snapshots remain byte-identical. */
    if(config->selected||config->bypass){
        if(used<0||used>=length)return used;
        used+=snprintf(buffer+used,(size_t)(length-used),";mc1,%d,%d",config->selected,config->bypass);
    }
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        hb_motion_config factory;hb_mo_defaults(&factory);hb_motion_lane defaults=factory.lanes[lane];int changed=0;
        for(int field=0;field<11;field++)if(*hb_mo_field(&defaults,field)!=*hb_mo_field(&config->lanes[lane],field))changed=1;
        if(!changed)continue;
        if(used<0||used>=length)return used;
        used+=snprintf(buffer+used,(size_t)(length-used),";mo1,%d",lane);
        for(int field=0;field<11;field++){
            if(used>=length)return used;
            used+=snprintf(buffer+used,(size_t)(length-used),",%d",*hb_mo_field(&config->lanes[lane],field));
        }
    }
    for(int lane=0;lane<HB_MOTION_LANES;lane++)if(config->lanes[lane].advance){
        if(used<0||used>=length)return used;
        used+=snprintf(buffer+used,(size_t)(length-used),";ma1,%d,%d",lane,config->lanes[lane].advance);
    }
    for(int index=0;index<HB_MOTION_LANES;index++){
        const hb_motion_lane *lane=&config->lanes[index];
        if(lane->every==1)continue;
        if(used<0||used>=length)return used;
        used+=snprintf(buffer+used,(size_t)(length-used),";mcond1,%d,%d,%d,%d",index,lane->every,lane->from,lane->through);
    }
    return used;
}
static void hb_mo_restore(hb_motion_config *config,const char *state){
    int capabilities=config->host_capabilities;hb_mo_defaults(config);config->host_capabilities=capabilities;
    const char *gesture_state=strstr(state,";gt1,");int hold_ms;
    if(!g_hb_hold_restored&&gesture_state&&sscanf(gesture_state,";gt1,%d",&hold_ms)==1&&hold_ms>=150&&hold_ms<=500){g_hb_hold_ms=hold_ms;g_hb_hold_restored=1;}
    gesture_state=state;
    while((gesture_state=strstr(gesture_state,";mt1,"))){int lane,mode;if(sscanf(gesture_state,";mt1,%d,%d",&lane,&mode)==2&&lane>=0&&lane<16&&mode>=0&&mode<3)config->lanes[lane].touch_mode=mode;gesture_state+=5;}
    const char *cursor=strstr(state,";mc1,");
    int selected=0,bypass=0;
    if(cursor&&sscanf(cursor,";mc1,%d,%d",&selected,&bypass)==2&&selected>=0&&selected<HB_MOTION_LANES&&bypass>=0&&bypass<=1){config->selected=selected;config->bypass=bypass;}
    cursor=state;
    while((cursor=strstr(cursor,";mo1,"))){
        cursor+=5;char *end=0;long lane=strtol(cursor,&end,10);
        if(end==cursor||lane<0||lane>=HB_MOTION_LANES)continue;
        hb_motion_lane restored;hb_mo_lane_default(&restored);int valid=1;cursor=end;
        for(int field=0;field<11;field++){
            if(*cursor!=','){valid=0;break;}
            cursor++;long value=strtol(cursor,&end,10);
            if(end==cursor||value<MO_PARAMETERS[field].low||value>MO_PARAMETERS[field].high){valid=0;break;}
            *hb_mo_field(&restored,field)=(int)value;cursor=end;
        }
        if(valid&&(*cursor==';'||!*cursor)){restored.touch_mode=config->lanes[lane].touch_mode;config->lanes[lane]=restored;}
    }
    cursor=state;
    while((cursor=strstr(cursor,";ma1,"))){
        int lane,advance,consumed=0;
        if(sscanf(cursor,";ma1,%d,%d%n",&lane,&advance,&consumed)==2&&consumed>0&&
            (!cursor[consumed]||cursor[consumed]==';')&&lane>=0&&lane<HB_MOTION_LANES&&advance>=0&&advance<3)config->lanes[lane].advance=advance;
        cursor+=5;
    }
    cursor=state;
    while((cursor=strstr(cursor,";mcond1,"))){
        int index,every,from,through,consumed=0;
        if(sscanf(cursor,";mcond1,%d,%d,%d,%d%n",&index,&every,&from,&through,&consumed)==4&&consumed>0&&
            (!cursor[consumed]||cursor[consumed]==';')&&index>=0&&index<HB_MOTION_LANES&&every>=1&&every<=16&&from>=1&&from<=through&&through<=every){
            hb_motion_lane *lane=&config->lanes[index];lane->every=every;lane->from=from;lane->through=through;
        }
        cursor+=8;
    }
}
#endif
