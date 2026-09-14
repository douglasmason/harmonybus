#ifndef HB_MOTION_PARAMS_H
#define HB_MOTION_PARAMS_H
#include "motion_metadata.h"
/* The selected lane is an editor cursor. Holds are runtime-only, never state. */
static const char *MO_OPERATIONS[]={"Off","Velocity","Pan","Octave","Rotate","Gate","Skip","Harmony","Chrom Below","Scale Above","Enclose Above Below","Enclose Below Above","Clip Repeat","Clip Reverse","Clip Time Shift","Clip Speed","Transpose"};
static const char *MO_PATTERNS[]={"Constant","Alternate","Rise","Fall","Triangle","Backbeat","Random"};
static const char *MO_GRIDS[]={"1/64","1/32","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars"};
static const char *MO_CYCLES[]={"1/8","1/4","1/2","1 Bar","2 Bars","3 Bars","4 Bars"};
static const char *MO_SWITCH[]={"Off","On"};
static const char *MO_GROUPS[]={"Chord","Voice"};
static const char *MO_RANDOM[]={"Repeat","Evolve"};
typedef struct { const char *key; size_t offset; int low,high; const char *const *options; } hb_motion_parameter;
#define MO_FIELD(name,low,high,options) {"motion_" #name,__builtin_offsetof(hb_motion_lane,name),low,high,options}
static const hb_motion_parameter MO_PARAMETERS[]={
    MO_FIELD(operation,0,16,MO_OPERATIONS),MO_FIELD(pattern,0,6,MO_PATTERNS),
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
    if(!strcmp(key,"motion_host")){config->host_capabilities=!strcmp(value,"movy-clip-v1");return 1;}
    if(!strcmp(key,"performance_reset")){
        config->held=0;config->pitch_held=0;config->enclosure=0;return 1;
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
            config->enclosure=!strcmp(key,"performance_enclose_ab")?1:2;
            config->enclosure_revision++;
        }
        return 1;
    }
    if(!strcmp(key,"motion_lane")){
        int lane=parse_i(value,config->selected+1);if(lane>=1&&lane<=HB_MOTION_LANES)config->selected=lane-1;return 1;
    }
    if(!strcmp(key,"motion_bypass")){config->bypass=enum_index(value,MO_SWITCH,2,config->bypass);return 1;}
    if(!strcmp(key,"motion_release")){config->held=0;return 1;}
    /* Fixed lane keys let a release follow its original lane after selection changes. */
    int slot=hb_mo_slot_key(key,"motion_hold_");
    if(slot>=0){
        unsigned bit=1u<<slot;int down=enum_index(value,MO_SWITCH,2,0);
        if(down&&!(config->held&bit)){
            config->held|=bit;config->held_serial[slot]=++config->serial;
            int operation=config->lanes[slot].operation;
            if(operation==HB_MO_BELOW||operation==HB_MO_ABOVE)config->enclosure=0;
            if(operation==HB_MO_ENCLOSE_AB||operation==HB_MO_ENCLOSE_BA){
                config->enclosure=operation==HB_MO_ENCLOSE_AB?1:2;
                config->enclosure_lane=slot;config->enclosure_revision++;
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
            static const int amounts[]={0,25,50,1,1,50,100,100,1,1,1,1,1,1,1,2,1};
            config->lanes[config->selected].amount=amounts[parsed];
            config->lanes[config->selected].offset=0;
            if(parsed>=HB_MO_ENCLOSE_AB&&parsed<=HB_MO_SPEED)config->lanes[config->selected].enabled=0;
        }
        if(index==4&&config->lanes[config->selected].operation>=HB_MO_ENCLOSE_AB&&config->lanes[config->selected].operation<=HB_MO_SPEED)parsed=0;
        *field=parsed;return 1;
    }
    return 0;
}
static int hb_mo_get(hb_motion_config *config,const char *key,char *buffer,int length){
    if(!strcmp(key,"chain_params")){
        int used=snprintf(buffer,(size_t)length,"%s{\"key\":\"motion_operation\",\"name\":\"Operation\",\"type\":\"enum\",\"options_as_string\":true,\"options\":[",HB_CHAIN_PARAMS_PREFIX);
        int count=0,selected=config->lanes[config->selected].operation;
        for(int operation=0;operation<17;operation++){
            if(operation>=HB_MO_REPEAT&&operation<=HB_MO_SPEED&&!config->host_capabilities&&operation!=selected)continue;
            if(used<0||used>=length)return -1;
            used+=snprintf(buffer+used,(size_t)(length-used),"%s\"%s\"",count++?",":"",MO_OPERATIONS[operation]);
        }
        if(used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),"]},{\"key\":\"motion_enabled\",\"name\":\"Auto\",\"type\":\"enum\",\"options_as_string\":true,\"options\":[\"Off\",\"On\"],\"readOnly\":%s}]",selected>=HB_MO_ENCLOSE_AB&&selected<=HB_MO_SPEED?"true":"false");
        return used>=length?-1:used;
    }
    if(!strcmp(key,"motion_lane"))return snprintf(buffer,(size_t)length,"%d",config->selected+1);
    if(!strcmp(key,"motion_bypass"))return snprintf(buffer,(size_t)length,"%s",MO_SWITCH[config->bypass]);
    if(!strcmp(key,"motion_punch")&&config->lanes[config->selected].operation>=HB_MO_REPEAT&&config->lanes[config->selected].operation<=HB_MO_SPEED&&!config->host_capabilities)return snprintf(buffer,(size_t)length,"Requires Movy");
    if(!strcmp(key,"motion_punch")&&(config->lanes[config->selected].operation==HB_MO_ENCLOSE_AB||config->lanes[config->selected].operation==HB_MO_ENCLOSE_BA))return snprintf(buffer,(size_t)length,"%s",config->enclosure&&config->enclosure_lane==config->selected?"Armed":"Trigger");
    if(!strcmp(key,"motion_punch"))return snprintf(buffer,(size_t)length,"%s",config->held&(1u<<config->selected)?"Held":"Idle");
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
    return used;
}
static void hb_mo_restore(hb_motion_config *config,const char *state){
    int capabilities=config->host_capabilities;hb_mo_defaults(config);config->host_capabilities=capabilities;
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
        if(valid&&(*cursor==';'||!*cursor))config->lanes[lane]=restored;
    }
}
#endif
