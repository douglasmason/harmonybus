/* Included after key-context helpers; all mutations occur on the host callback. */
static void hb_sc_begin(Inst *instance){
    if(g_sc_ready)return;
    int was_active=g_key_context.active;
    g_sc_base=hb_key_baseline(instance);g_sc_base.active=was_active;
    g_sc_base.source_root=mod12(g_sc_base.source_root-g_bus.global_transpose);
    g_sc_base.target_root=mod12(g_sc_base.target_root-g_bus.global_transpose);
    g_sc_base.source_mask=hb_transpose_mask(g_sc_base.source_mask,-g_bus.global_transpose);
    g_sc_base.target_mask=hb_transpose_mask(g_sc_base.target_mask,-g_bus.global_transpose);
    g_sc_ready=1;
}
static void hb_sc_resolve(Inst *instance){
    if(!g_sc_dirty)return;
    hb_sc_begin(instance);g_sc_dirty=0;
    hb_sc_value chosen[HB_SC_KINDS]={{0}};
    g_parallel_manual=g_parallel_latch=g_key_lane_parallel=0;
    for(int kind=0;kind<HB_SC_KINDS;kind++){
        g_sc_owner[kind]=-1;g_sc_recorded[kind]=0;
        /* Live performance overrides the recorded contribution, regardless
           of a loop wrap. Removing it reveals the current recorded state. */
        for(int track=0;track<16;track++){
            hb_sc_value value=g_sc_replay[track][kind];
            int conductor=0;for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].role==0&&g_pool[index].movy_track==track)conductor=1;
            if(!conductor)value.on=0;
            if(value.on&&value.order>=chosen[kind].order){chosen[kind]=value;g_sc_owner[kind]=track;g_sc_recorded[kind]=1;}
        }
        unsigned long long live_order=0;
        if(kind==HB_SC_KEY)for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_sc_key_audition[index].on&&g_sc_key_audition[index].order>=live_order){
            chosen[kind]=g_sc_key_audition[index];live_order=chosen[kind].order;g_sc_owner[kind]=g_pool[index].movy_track;g_sc_recorded[kind]=0;
        }
        live_order=0; /* Explicit live control always wins over audition/replay. */
        for(int index=0;index<HB_MAX_INSTANCES;index++){
            hb_sc_value value=g_sc_live[index][kind];
            if(g_pool[index].used&&g_pool[index].role<2&&value.on&&value.order>=live_order){
                chosen[kind]=value;live_order=value.order;g_sc_owner[kind]=g_pool[index].movy_track;g_sc_recorded[kind]=0;
            }
        }
    }
    hb_key_context result=g_sc_base;
    hb_sc_value key=chosen[HB_SC_KEY],parent=chosen[HB_SC_PARENT],parallel=chosen[HB_SC_PARALLEL];
    if(key.on){result.active=1;result.target_root=key.a;result.target_mask=key.b&4095;result.blues=key.c&1;}
    if(parent.on){result.active=1;result.target_mask=hb_explicit_scale_mask(result.target_root,parent.a);result.blues=HB_SCALE_DOMINANT[parent.a];}
    g_parallel_previous=result;
    if(parallel.on){
        result.active=1;int scale=parallel.a;
        result.blues=HB_SCALE_DOMINANT[HB_PARALLEL_SCALE_IDS[scale]];
        if(scale==18)hb_key_relative(&result);
        else result.target_mask=hb_explicit_scale_mask(result.target_root,HB_PARALLEL_SCALE_IDS[scale]);
    }
    g_parallel_on=parallel.on;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        g_parallel_manual|=g_sc_manual[index];g_parallel_latch|=g_sc_latch[index];g_key_lane_parallel|=g_sc_lane[index];
    }
    hb_key_context *contexts[]={&result,&g_parallel_previous};
    for(int index=0;index<2;index++){
        hb_key_context *context=contexts[index];
        context->source_root=mod12(context->source_root+g_bus.global_transpose);
        context->target_root=mod12(context->target_root+g_bus.global_transpose);
        context->source_mask=hb_transpose_mask(context->source_mask,g_bus.global_transpose);
        context->target_mask=hb_transpose_mask(context->target_mask,g_bus.global_transpose);
    }
    g_key_context=result;
    if(!key.on&&!parent.on&&!parallel.on)g_sc_ready=0;
}
static void hb_sc_set_live(Inst *instance,int kind,int on,int a,int b,int c){
    if(!instance||kind<0||kind>=HB_SC_KINDS)return;
    int index=(int)(instance-g_pool);if(index<0||index>=HB_MAX_INSTANCES)return;
    hb_sc_value *value=&g_sc_live[index][kind];
    if(value->on==on&&(!on||(value->a==a&&value->b==b&&value->c==c)))return;
    hb_sc_begin(instance);
    *value=(hb_sc_value){on,a,b,c,++g_sc_serial};
    if(kind!=HB_SC_KEY&&instance->role==0&&instance->movy_track>=0&&g_sc_count<64){
        g_sc_events[(g_sc_head+g_sc_count++)%64]=(hb_sc_event){instance->movy_track,kind,*value};
    }
    g_sc_dirty=1;hb_sc_resolve(instance);
}
/* Audition an explicit recordable action without taking ownership of the
   live knob. Movy captures the request, then supplies its authoritative state. */
static void hb_sc_key_action(Inst *instance,hb_sc_value event){
    int index=(int)(instance-g_pool);if(index<0||index>=HB_MAX_INSTANCES)return;
    hb_sc_begin(instance);event.order=++g_sc_serial;
    g_sc_key_request[index]=event;
    g_sc_key_audition[index]=hb_ks_apply(&g_sc_key_sequence[index],event);
    if(instance->role==0&&instance->movy_track>=0&&instance->movy_track==g_sc_record_track&&g_sc_count<64)
        g_sc_events[(g_sc_head+g_sc_count++)%64]=(hb_sc_event){instance->movy_track,HB_SC_KEY,event};
    g_sc_dirty=1;hb_sc_resolve(instance);
}
static int hb_sc_append_events(char *buffer,int length,int used){
    while(g_sc_count&&used>=0&&used<length){
        hb_sc_event *event=&g_sc_events[g_sc_head];char field[128];
        int size=snprintf(field,sizeof(field),"|sc1,%d,%d,%d,%d,%d,%d",event->owner,event->kind,event->value.on,event->value.a,event->value.b,event->value.c);
        if(size>=length-used)break;
        memcpy(buffer+used,field,(size_t)size+1);used+=size;g_sc_head=(g_sc_head+1)%64;g_sc_count--;
    }
    return used;
}
static int hb_sc_set(Inst *instance,const char *key,const char *text){
    if(!strcmp(key,"hb_key_sequence_state")){
        int track,valid,count,depth;unsigned origin,current;int used=0;
        if(sscanf(text,"%d,%d,%d,%d,%u,%u%n",&track,&valid,&count,&depth,&origin,&current,&used)!=6||track<0||track>=16||valid<0||valid>1||count<0||count>64||depth<0||depth>64)return 1;
        hb_ks_state state={0};state.valid=valid;state.count=count;state.depth=depth;
        state.origin=(hb_ks_key){origin&15,(origin>>4)&4095,(origin>>16)&1};
        state.current=(hb_ks_key){current&15,(current>>4)&4095,(current>>16)&1};
        const char *cursor=text+used;
        for(int item=0;item<depth;item++){
            unsigned packed;if(sscanf(cursor,",%u%n",&packed,&used)!=1)return 1;cursor+=used;
            state.history[item]=(hb_ks_key){packed&15,(packed>>4)&4095,(packed>>16)&1};
            if(state.history[item].root>11||!state.history[item].mask)return 1;
        }
        if(*cursor||state.origin.root>11||state.current.root>11||(valid&&(!state.origin.mask||!state.current.mask)))return 1;
        for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].movy_track==track){g_sc_key_sequence[index]=state;g_sc_key_audition[index].on=0;}
        g_sc_dirty=1;return 1;
    }
    if(!strcmp(key,"hb_shared_reset")){
        memset(g_sc_replay,0,sizeof(g_sc_replay));
        for(int index=0;index<HB_MAX_INSTANCES;index++)g_pool[index].sc_anchor_valid=0;
        if(!strcmp(text,"All")){
            memset(g_sc_key_audition,0,sizeof(g_sc_key_audition));memset(g_sc_key_request,0,sizeof(g_sc_key_request));memset(g_sc_key_sequence,0,sizeof(g_sc_key_sequence));
            memset(g_sc_live,0,sizeof(g_sc_live));memset(g_sc_manual,0,sizeof(g_sc_manual));memset(g_sc_latch,0,sizeof(g_sc_latch));memset(g_sc_lane,0,sizeof(g_sc_lane));
            g_sc_count=g_sc_head=0;g_key_armed=0;g_sc_arm_owner=g_sc_record_track=-1;
        }
        g_sc_dirty=1;hb_sc_resolve(instance);return 1;
    }
    if(!strcmp(key,"hb_shared_context")){
        int track,kind,on,a,b,c;unsigned long long order;
        if(sscanf(text,"%d,%d,%d,%d,%d,%d,%llu",&track,&kind,&on,&a,&b,&c,&order)!=7||track<0||track>=16||kind<0||kind>=HB_SC_KINDS||on<0||on>1)return 1;
        if(on&&((kind==HB_SC_KEY&&(a<0||a>11||b<1||b>0xfffffff||!(b&4095)||c<0||c>0x3ffffff))||
           (kind==HB_SC_PARALLEL&&(a<1||a>=HB_PARALLEL_COUNT))||(kind==HB_SC_PARENT&&(a<1||a>=HB_SCALE_COUNT))))return 1;
        hb_sc_value value={on,a,b,c,order};
        hb_sc_value previous=g_sc_replay[track][kind];
        if(previous.on!=on||previous.a!=a||previous.b!=b||previous.c!=c||previous.order!=order){hb_sc_begin(instance);g_sc_replay[track][kind]=value;g_sc_dirty=1;}
        return 1;
    }
    if(!strcmp(key,"hb_shared_anchor")){
        int track,pitch,mask,detail;
        if(sscanf(text,"%d,%d,%d,%d",&track,&pitch,&mask,&detail)!=4||track<0||track>=16||pitch<0||pitch>127||mask<0||detail<0)return 1;
        int source=(mask>>12)&15,target=(detail>>9)&15;
        unsigned source_mask=(mask>>16)&4095,target_mask=(detail>>13)&4095;
        if(source>11||target>11||!source_mask||!target_mask)return 1;
        hb_key_context before={1,mod12(source+g_bus.global_transpose),mod12(target+g_bus.global_transpose),(detail>>25)&1,
            hb_transpose_mask(source_mask,g_bus.global_transpose),hb_transpose_mask(target_mask,g_bus.global_transpose)};
        for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].movy_track==track){
            g_pool[index].sc_anchor_valid=1;g_pool[index].sc_anchor_pitch=pitch;g_pool[index].sc_anchor_context=before;
        }
        return 1;
    }
    if(!strcmp(key,"hb_shared_flush")){hb_sc_resolve(instance);return 1;}
    if(!strcmp(key,"hb_shared_record")){
        int track=(int)strtol(text,0,10);g_sc_record_track=track>=0&&track<16?track:-1;
        if(g_sc_record_track>=0)for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].role==0&&g_pool[index].movy_track==track)
            for(int kind=1;kind<HB_SC_KINDS;kind++)if(g_sc_live[index][kind].on&&g_sc_count<64)
                g_sc_events[(g_sc_head+g_sc_count++)%64]=(hb_sc_event){track,kind,g_sc_live[index][kind]};
        return 1;
    }
    if(!strcmp(key,"hb_shared_handoff")){
        int track=(int)strtol(text,0,10);if(track<0||track>=16)return 1;
        memset(g_sc_replay[track],0,sizeof(g_sc_replay[track]));
        for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].movy_track==track){
            g_sc_key_audition[index].on=0;
            g_sc_live[index][HB_SC_PARENT].on=0;
            if(!g_sc_manual[index]&&!g_sc_latch[index]&&!g_sc_lane[index])g_sc_live[index][HB_SC_PARALLEL].on=0;
        }
        g_sc_dirty=1;return 1;
    }
    if(!strcmp(key,"follower_scale")&&instance->role==0&&instance->movy_track==g_sc_record_track&&g_sc_record_track>=0){
        int selected=enum_index(text,FOLLOWER_SCALE_OPTS,HB_SCALE_COUNT,hb_shared_follower_scale());
        if(selected<=0)return 0; /* Infer remains a live base-setting choice. */
        hb_sc_set_live(instance,HB_SC_PARENT,1,selected,0,0);
        return 1;
    }
    return 0;
}
static int hb_sc_get(Inst *instance,const char *key,char *buffer,int length){
    int field=-1;
    if(sscanf(key,"shared_context_%d",&field)!=1&&strcmp(key,"shared_context_snapshot"))return -1;
    hb_key_context current=hb_key_baseline(instance),base=g_sc_ready?g_sc_base:current;
    if(g_sc_ready){base.target_root=mod12(base.target_root+g_bus.global_transpose);base.target_mask=hb_transpose_mask(base.target_mask,g_bus.global_transpose);}
    char values[8][80];
    snprintf(values[0],80,"%s%s",HB_KEY_NAMES[current.target_root],hb_key_quality(current));
    snprintf(values[1],80,"%s%s",HB_KEY_NAMES[base.target_root],hb_key_quality(base));
    for(int kind=0;kind<3;kind++){
        int active=0;for(int index=0;index<HB_MAX_INSTANCES;index++)active+=g_sc_live[index][kind].on!=0;
        for(int track=0;track<16;track++)active+=g_sc_replay[track][kind].on!=0;
        if(kind==HB_SC_KEY)for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used)active+=g_sc_key_audition[index].on!=0;
        if(!active)snprintf(values[kind+2],80,"--");
        else if(g_sc_owner[kind]<0)snprintf(values[kind+2],80,"Live%s",active>1?" +":"");
        else snprintf(values[kind+2],80,"T%d %s%s",g_sc_owner[kind]+1,g_sc_recorded[kind]?"REC":"LIVE",active>1?" +":"");
    }
    snprintf(values[5],80,"%+d",g_bus.global_transpose);
    int scale=-1;for(int choice=1;choice<HB_SCALE_COUNT;choice++)if(hb_explicit_scale_mask(current.target_root,choice)==current.target_mask&&!HB_SCALE_DOMINANT[choice]){scale=choice;break;}
    snprintf(values[6],80,"%s",current.blues?"Blues":scale>=0?FOLLOWER_SCALE_OPTS[scale]:"Custom scale");
    int capturing=0;for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].role==0&&g_pool[index].movy_track==g_sc_record_track&&g_sc_record_track>=0)capturing=1;
    snprintf(values[7],80,"%s",capturing?"Conductor REC":g_sc_record_track>=0?"Follower live only":"Live / clips");
    if(g_override_winner>=0)snprintf(values[7],80,"Override T%d %s",g_pool[g_override_winner].movy_track>=0?g_pool[g_override_winner].movy_track+1:g_override_winner+1,g_override[g_override_winner].active==1?"LIVE":"INPUT");
    if(field>=0&&field<8)return snprintf(buffer,(size_t)length,"%s",values[field]);
    if(field>=0)return -1;
    return snprintf(buffer,(size_t)length,"dp1|%s|%s|%s|%s|%s|%s|%s|%s",values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7]);
}
