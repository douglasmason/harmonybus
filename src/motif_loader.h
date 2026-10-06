/* Explicit clip construction only: no audio-loop work and no rendered voices.
   One immutable export belongs to its preparing instance. All 64-bit action
   words cross the host boundary as hex strings, never JavaScript numbers. */
static const char *HB_ML_NAMES[]={"V","ii-V","iv-bVII","bII7","ii-bII7","bVI-bVII","bVI-V","bIII-IV","vi-V","iii-vi-ii-V","IV-iv","ii halfdim-V","VI7-ii-V","V/V-V","ii/V-V/V-V","V/ii-ii-V","V/vi-vi-ii-V","vii dim/V-V","III7-VI7-II7-V7","vi-ii-V","ii-V-LT","iv-bVII-LT","ii-bII7-LT","iii-vi-ii","IV-ii-V","vii-iii-vi","LT-ii-V"};
static const char *HB_ML_KEYS[]={"motif_load_choice","motif_load_target","motif_load_collection","motif_load_unit","motif_load_rhythm","motif_load_even"};
static const int HB_ML_MAX[]={42,127,1,12,5,1};
static hb_mc_plan g_ml_plan;
static Inst *g_ml_owner;
static unsigned g_ml_serial;
static int g_ml_root,g_ml_scale,g_ml_transpose;
static const char *g_ml_error="Prepare preview";
static int hb_ml_build(Inst *instance,hb_mc_plan *plan,int *root,int *scale){
    hb_mt_definition definition;
    int choice=instance->motif_load[0];
    if(choice<27)hb_mt_preset_definition(choice+1,&definition);
    else {definition=g_motifs[choice-27];if(!hb_mt_occupied(&definition)){g_ml_error="Empty motif slot";return 0;}}
    *root=hb_global_explicit_root();hb_resolve_follower_reference_root(instance,root);
    *scale=hb_follower_input_scale_index(instance,*root);if(*scale<1)*scale=1;
    int placement=hb_mt_placement(instance->motif.editor.placement,definition.placement);
    int mode=placement==3?HB_MC_BOTH:placement==HB_MT_TARGET_START?HB_MC_START:placement==HB_MT_TARGET_END?HB_MC_END:HB_MC_OMIT;
    if(!hb_mc_build(&definition,mode,HB_MT_DURATIONS[instance->motif_load[3]],g_motif_rhythm,instance->motif_load[5],plan)){
        g_ml_error="Invalid motif timing";return 0;
    }
    if(plan->duration*4>256*24){g_ml_error="Exceeds 16 bars";return 0;}
    unsigned collection=hb_explicit_scale_mask(*root,*scale);
    for(int index=0;index<plan->count;index++){
        hb_mc_event *item=&plan->events[index];hb_mt_event *event=&item->event;
        for(int voice=0;voice<event->count;voice++){
            int pitch=hb_mt_relative(event,event->notes[voice].pitch,definition.reference_pitch,instance->motif_load[1],collection);
            if(pitch<0||pitch>127){g_ml_error="Target exceeds MIDI range";return 0;}
            event->notes[voice].pitch=pitch;
        }
        /* Parallel borrowing is a recorded per-note destination operation,
           just like a live approach; it never changes the track's scale. */
        unsigned long long *intent=&event->actions[HB_MOTION_LANES];
        if(event->modifier)*intent=(*intent&~3ULL)|(event->modifier<0?1:event->modifier==2?3:2);
        if(event->secondary)*intent=(*intent&~((7ULL<<4)|(1ULL<<8)|(1ULL<<63)))|hb_mo_role_word(event->secondary);
        if(event->cadence)*intent=(*intent&~HB_MO_CADENCE_MASK)|((unsigned long long)event->cadence<<13);
        if((*intent&(1ULL<<20))&&((*intent>>21)&3)!=3){
            /* Relocate captured construction context with the input notes.
               Explicit quality/size/mode stay intact; the parent belongs to
               this destination track, just as its input scale does. */
            *intent=(*intent&~((15ULL<<32)|(31ULL<<36)))|((unsigned long long)*root<<32)|((unsigned long long)*scale<<36);
        }
        if(instance->motif_load[2]&&(*intent&(0x173ULL|HB_MO_CADENCE_MASK|HB_AR_MASK))){
            int destination=g_parallel_scale==18?31:HB_PARALLEL_SCALE_IDS[g_parallel_scale];
            *intent=(*intent&~HB_MO_INTENT_MASK)|(1ULL<<20)|(3ULL<<21)|((unsigned long long)destination<<36);
        }
    }
    g_ml_error="";return 1;
}
static int hb_ml_set(Inst *instance,const char *key,const char *value){
    if(strncmp(key,"motif_load_",11))return 0;
    if(!strcmp(key,"motif_load_capture")){instance->motif_load[1]=instance->motif_load_last;return 1;}
    if(!strcmp(key,"motif_load_prepare")){
        g_ml_owner=0;
        if(hb_ml_build(instance,&g_ml_plan,&g_ml_root,&g_ml_scale)){
            g_ml_owner=instance;g_ml_transpose=g_bus.global_transpose;g_ml_serial++;
        }
        return 1;
    }
    for(int index=0;index<6;index++)if(!strcmp(key,HB_ML_KEYS[index])){
        int number=-1;
        if(index==0){for(int preset=1;preset<HB_MT_PRESET_COUNT;preset++)if(!strcmp(value,HB_MT_PRESETS[preset])||!strcmp(value,HB_ML_NAMES[preset-1]))number=preset-1;
            int slot=0;if(sscanf(value,"User %d",&slot)==1&&slot>=1&&slot<=16)number=26+slot;}
        else if(index==2)number=!strcmp(value,"Parent")?0:!strcmp(value,"Parallel")?1:-1;
        else if(index==3)number=enum_index(value,HB_MT_DURATION_NAMES,13,-1);
        else if(index==4)number=enum_index(value,HB_MT_RHYTHMS,6,-1);
        else if(index==5)number=!strcmp(value,"Off")?0:!strcmp(value,"On")?1:-1;
        else if(index==1){char tail;int pitch;if(sscanf(value,"%d%c",&pitch,&tail)==1)number=pitch;}
        if(number>=0&&number<=HB_ML_MAX[index])instance->motif_load[index]=number;
        return 1;
    }
    return 1;
}
static int hb_ml_get(Inst *instance,const char *key,char *buffer,int length){
    if(strncmp(key,"motif_load_",11))return -1;
    for(int index=0;index<6;index++)if(!strcmp(key,HB_ML_KEYS[index])){
        int number=instance->motif_load[index];
        if(index==0)return number<27?snprintf(buffer,(size_t)length,"%s",HB_ML_NAMES[number]):snprintf(buffer,(size_t)length,"User %d",number-26);
        if(index==1)return snprintf(buffer,(size_t)length,"%d",number);
        const char *label=index==2?(number?"Parallel":"Parent"):index==3?HB_MT_DURATION_NAMES[number]:index==4?HB_MT_RHYTHMS[number]:number?"On":"Off";
        return snprintf(buffer,(size_t)length,"%s",label);
    }
    if(!strcmp(key,"motif_load_header")){
        if(g_ml_owner!=instance)return snprintf(buffer,(size_t)length,"error:%s",g_ml_error);
        return snprintf(buffer,(size_t)length,"ml1,%u,%d,%d,%d,%d,%d",g_ml_serial,g_ml_plan.duration*4,g_ml_plan.count,g_ml_root,g_ml_scale,g_ml_transpose);
    }
    if(!strcmp(key,"motif_load_labels")){
        if(g_ml_owner!=instance)return snprintf(buffer,(size_t)length,"Preview expired");
        int used=0;
        static const char *roles[]={"Input","ii","V","T","vi","iv","bVII","bII","iii","IV","vii"};
        for(int i=0;i<g_ml_plan.count&&used<length;i++){
            const hb_mc_event *item=&g_ml_plan.events[i];const hb_mt_event *e=&item->event;
            const hb_cadence_step *cadence=hb_cadence_decode(e->cadence);
            const char *label=item->source_step==-1?"T":e->kind?"Rest":cadence?cadence->label:e->modifier<0?"LT":e->modifier==2?"bII":e->secondary>=0&&e->secondary<11?roles[e->secondary]:"Input";
            used+=snprintf(buffer+used,(size_t)(length-used),"%s%s",i?" > ":"",label);
        }
        return used>=length?snprintf(buffer,(size_t)length,"Long motif"):used;
    }
    unsigned serial;int index,consumed=0;
    if(sscanf(key,"motif_load_event_%u_%d%n",&serial,&index,&consumed)==2&&key[consumed]==0){
        if(g_ml_owner!=instance||serial!=g_ml_serial||index<0||index>=g_ml_plan.count)return snprintf(buffer,(size_t)length,"error:Preview expired");
        const hb_mc_event *item=&g_ml_plan.events[index];const hb_mt_event *event=&item->event;
        int used=snprintf(buffer,(size_t)length,"%d,%d",item->onset*4,item->duration*4);
        for(int voice=0;voice<event->count&&used<length;voice++)used+=snprintf(buffer+used,(size_t)(length-used),",%d.%d",event->notes[voice].pitch,event->notes[voice].velocity);
        if(used<length)used+=snprintf(buffer+used,(size_t)(length-used),":");
        for(int lane=0;lane<=HB_MOTION_LANES&&used<length;lane++)used+=snprintf(buffer+used,(size_t)(length-used),"%s%llx",lane?",":"",event->actions[lane]);
        if(used>=length)return snprintf(buffer,(size_t)length,"error:Preview too large");
        return used;
    }
    return snprintf(buffer,(size_t)length,"Preview");
}
static int hb_ml_save(Inst *instance,char *buffer,int length,int used){
    if(used<0||used>=length)return used;
    int *v=instance->motif_load;
    used+=snprintf(buffer+used,(size_t)(length-used),";ml1,%d,%d,%d,%d,%d,%d",v[0],v[1],v[2],v[3],v[4],v[5]);
    return used>=length?-1:used;
}
static void hb_ml_restore(Inst *instance,const char *state){
    const char *marker=strstr(state,";ml1,");int v[6],used=0;
    if(!marker||sscanf(marker+5,"%d,%d,%d,%d,%d,%d%n",v,v+1,v+2,v+3,v+4,v+5,&used)!=6||(marker[5+used]&&marker[5+used]!=';'))return;
    for(int i=0;i<6;i++)if(v[i]<0||v[i]>HB_ML_MAX[i])return;
    memcpy(instance->motif_load,v,sizeof(v));
}
