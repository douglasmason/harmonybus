#ifndef HB_MOTION_H
#define HB_MOTION_H
/* Sixteen per-instance lanes. Pure evaluation uses transport position, never a
   mutable random stream, so local and MIDI-render routes make identical choices. */
#define HB_MOTION_LANES 16
#define HB_MOTION_OWNERS 512
#define HB_MOTION_QUEUE 2048
#define HB_MOTION_BURSTS 128
enum { HB_MO_OFF, HB_MO_VELOCITY, HB_MO_PAN, HB_MO_OCTAVE, HB_MO_ROTATE,
       HB_MO_GATE, HB_MO_SKIP, HB_MO_HARMONY, HB_MO_BELOW, HB_MO_ABOVE,
       HB_MO_ENCLOSE_AB, HB_MO_ENCLOSE_BA, HB_MO_REPEAT, HB_MO_REVERSE,
       HB_MO_TIME_SHIFT, HB_MO_SPEED, HB_MO_TRANSPOSE, HB_MO_RATCHET, HB_MO_ECHO };
typedef struct { int operation,pattern,amount,offset,enabled,grid,cycle,phase,probability,group,evolve,advance,every,from,through; } hb_motion_lane;
typedef struct { hb_motion_lane lanes[HB_MOTION_LANES]; int selected,bypass,host_capabilities,enclosure_lane; unsigned serial,held_serial[HB_MOTION_LANES]; unsigned held;
    unsigned revision[HB_MOTION_LANES];
    unsigned long long events[HB_MOTION_LANES];
    const unsigned long long *event_override;
    int input_valid; double input_beat; uint8_t input_notes[128];
    unsigned pitch_held,enclosure_revision; int pitch_last,enclosure;
} hb_motion_config;
typedef struct { int used,channel,source,pitch,sounding; double off_beat; unsigned long long serial; double repeat_off; int generated,lane,manual; unsigned revision,held_serial; } hb_motion_owner;
typedef struct {
    int used,lane,channel,pitch,velocity,remaining,manual,decay;
    unsigned revision,held_serial;
    double next,spacing,gate;
} hb_motion_burst;
typedef struct {
    hb_motion_owner owners[HB_MOTION_OWNERS];
    hb_motion_burst bursts[HB_MOTION_BURSTS];
    unsigned short refs[16][128];
    uint8_t queue[HB_MOTION_QUEUE][3];
    int head,count,owned,pan_dirty[16],base_pan[16];
    unsigned long long next_serial;
    unsigned enclosure_revision;
    int enclosure_step,performance_valid,performance_modifier;
    double performance_beat;
    uint8_t performance_notes[128];
} hb_motion_route;
static int hb_mo_clamp(int value,int low,int high){return value<low?low:value>high?high:value;}
static int hb_mo_round(double value){return (int)(value+(value>=0?0.5:-0.5));}
static double hb_mo_floor(double value){long long whole=(long long)value;return (double)whole-(value<(double)whole);}
static double hb_mo_grid(int index){return 0.0625*(1u<<hb_mo_clamp(index,0,8));}
static double hb_mo_cycle(int index){static const double lengths[]={0.5,1,2,4,8,12,16};return lengths[hb_mo_clamp(index,0,6)];}
static void hb_mo_lane_default(hb_motion_lane *lane){memset(lane,0,sizeof(*lane));lane->enabled=1;lane->grid=3;lane->cycle=3;lane->probability=100;lane->every=lane->from=lane->through=1;}
static void hb_mo_defaults(hb_motion_config *config){memset(config,0,sizeof(*config));for(int index=0;index<HB_MOTION_LANES;index++)hb_mo_lane_default(&config->lanes[index]);
    config->enclosure_lane=-1;
    for(int index=12;index<16;index++){config->lanes[index].operation=HB_MO_BELOW+index-12;config->lanes[index].enabled=0;config->lanes[index].amount=1;}
}
static void hb_mo_route_init(hb_motion_route *route){memset(route,0,sizeof(*route));for(int channel=0;channel<16;channel++)route->base_pan[channel]=64;}
static int hb_mo_lane_active(const hb_motion_config *config,int index){
    int operation=config->lanes[index].operation;
    if(operation>=HB_MO_REPEAT&&operation<=HB_MO_SPEED&&
        (!config->host_capabilities||(config->host_capabilities<2&&!(config->held&(1u<<index)))))return 0;
    if(operation==HB_MO_ENCLOSE_AB||operation==HB_MO_ENCLOSE_BA)return config->enclosure&&config->enclosure_lane==index;
    return operation && ((config->held&(1u<<index)) || (!config->bypass&&config->lanes[index].enabled));
}
static int hb_mo_enabled(const hb_motion_config *config){if(config->pitch_held||config->enclosure)return 1;for(int index=0;index<HB_MOTION_LANES;index++)if(hb_mo_lane_active(config,index))return 1;return 0;}
/* Each route advances independently so the local and render copies agree.
   Simultaneous distinct pitches share a step; a repeated pitch starts another
   onset even if both arrive before the next transport sample. */
static int hb_mo_performance(hb_motion_config *config,hb_motion_route *route,int source,double beat,double grouping){
    unsigned newest=0;int held_modifier=0;
    for(int lane=0;lane<HB_MOTION_LANES;lane++)if(config->held&(1u<<lane)){
        int operation=config->lanes[lane].operation;
        if((operation==HB_MO_BELOW||operation==HB_MO_ABOVE)&&config->held_serial[lane]>=newest){
            newest=config->held_serial[lane];held_modifier=operation==HB_MO_BELOW?-1:1;
        }
    }
    if(held_modifier)return held_modifier;
    if(config->pitch_held)return config->pitch_last==1?-1:1;
    if(!config->enclosure)return 0;
    if(route->enclosure_revision!=config->enclosure_revision){
        route->enclosure_revision=config->enclosure_revision;
        route->enclosure_step=0;route->performance_valid=0;
    }
    double elapsed=beat-route->performance_beat;
    if(!route->performance_valid||elapsed<0||elapsed>grouping||route->performance_notes[source]){
        int step=route->enclosure_step++;
        route->performance_modifier=step>=2?0:((config->enclosure==1)==(step==0)?1:-1);
        route->performance_beat=beat;route->performance_valid=1;
        memset(route->performance_notes,0,sizeof(route->performance_notes));
    }
    route->performance_notes[source]=1;
    return route->performance_modifier;
}
/* Source events advance once before mapping; UI reads and generated copies are pure. */
static void hb_mo_input_reset(hb_motion_config *config){
    memset(config->events,0,sizeof(config->events));config->input_valid=0;
}
static void hb_mo_input(hb_motion_config *config,int pitch,double beat,double grouping){
    double elapsed=beat-config->input_beat;
    int chord=!config->input_valid||elapsed<0||elapsed>grouping||config->input_notes[pitch];
    if(chord){config->input_valid=1;config->input_beat=beat;memset(config->input_notes,0,sizeof(config->input_notes));}
    config->input_notes[pitch]=1;
    for(int lane=0;lane<HB_MOTION_LANES;lane++)
        if(config->lanes[lane].advance==1||(config->lanes[lane].advance==2&&chord))config->events[lane]++;
}
static unsigned hb_mo_hash(unsigned value){value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;return value^(value>>16);}
/* Conditions use transport cycles, independently of pattern phase or input
   advancement. Negative beat means stopped; unrestricted lanes keep working. */
static int hb_mo_condition_cycle(const hb_motion_lane *lane,double beat){
    int every=hb_mo_clamp(lane->every,1,16);
    if(beat<0)return 1;
    long long cycle=(long long)hb_mo_floor((beat+1e-9)/hb_mo_cycle(lane->cycle));
    return (int)(cycle%every)+1;
}
static int hb_mo_condition(const hb_motion_config *config,int index,double beat){
    const hb_motion_lane *lane=&config->lanes[index];
    if(config->held&(1u<<index))return 1;
    if(lane->every<=1)return 1;
    if(beat<0)return 0;
    int cycle=hb_mo_condition_cycle(lane,beat);
    return cycle>=lane->from&&cycle<=lane->through;
}
/* Return false means do not perform this operation; it never means skip a note. */
static int hb_mo_value_at(const hb_motion_config *config,int index,double beat,double condition_beat,int voice,double *value){
    const hb_motion_lane *lane=&config->lanes[index];
    int held=(config->held&(1u<<index))!=0;
    if(!hb_mo_lane_active(config,index)||!hb_mo_condition(config,index,condition_beat)||(!held&&!lane->probability))return 0;
    double grid=hb_mo_grid(lane->grid),cycle=hb_mo_cycle(lane->cycle);
    if(lane->advance){const unsigned long long *events=config->event_override?config->event_override:config->events;
        beat=(double)(events[index]?events[index]-1:0)*grid;}
    double shifted=beat+(double)lane->phase*grid;
    long long iteration=(long long)hb_mo_floor(shifted/cycle);
    double position=shifted-(double)iteration*cycle;
    int steps=(int)(cycle/grid+0.999999),step=(int)hb_mo_floor((position+1e-8)/grid);
    if(steps<1)steps=1;if(step>=steps)step=steps-1;
    unsigned seed=(unsigned)(index+1)*0x9e3779b9u^(unsigned)step*0x85ebca6bu;
    if(lane->evolve)seed^=(unsigned)iteration*0xc2b2ae35u;
    if(lane->group)seed^=(unsigned)(voice+1)*0x27d4eb2du;
    if(!held&&lane->probability<100&&hb_mo_hash(seed)%100u>=(unsigned)lane->probability)return 0;
    double pattern=1.0;
    if(lane->pattern==1)pattern=(step&1)?1.0:-1.0;
    else if(lane->pattern==2)pattern=steps>1?2.0*step/(steps-1)-1.0:0;
    else if(lane->pattern==3)pattern=steps>1?1.0-2.0*step/(steps-1):0;
    else if(lane->pattern==4){double phase=(double)step/steps;pattern=phase<0.5?4*phase-1:3-4*phase;}
    else if(lane->pattern==5)pattern=((int)hb_mo_floor(position)&1)?1.0:0.0;
    else if(lane->pattern==6)pattern=2.0*(hb_mo_hash(seed^0xa511e9b3u)%10001u)/10000.0-1.0;
    *value=(lane->operation==HB_MO_ECHO?0:lane->offset)+lane->amount*pattern;return 1;
}
static int hb_mo_value(const hb_motion_config *config,int index,double beat,int voice,double *value){
    return hb_mo_value_at(config,index,beat,beat,voice,value);
}
static int hb_mo_push(hb_motion_route *route,int status,int pitch,int velocity){
    if(route->count>=HB_MOTION_QUEUE)return 0;
    int index=(route->head+route->count)%HB_MOTION_QUEUE;
    route->queue[index][0]=(uint8_t)status;route->queue[index][1]=(uint8_t)pitch;route->queue[index][2]=(uint8_t)velocity;route->count++;return 1;
}
static int hb_mo_pop(hb_motion_route *route,uint8_t message[3]){if(!route->count)return 0;memcpy(message,route->queue[route->head],3);route->head=(route->head+1)%HB_MOTION_QUEUE;route->count--;return 1;}
/* Releasing one source must not silence a different source mapped to that pitch. */
static int hb_mo_release(hb_motion_route *route,hb_motion_owner *owner){
    if(!owner->sounding)return 1;
    unsigned short *refs=&route->refs[owner->channel][owner->pitch];
    if(*refs==1&&!hb_mo_push(route,0x80|owner->channel,owner->pitch,0))return 0;
    if(*refs)(*refs)--;owner->sounding=0;return 1;
}
static void hb_mo_due(hb_motion_route *route,double beat){
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&!owner->generated&&owner->sounding&&owner->off_beat>=0&&beat+1e-9>=owner->off_beat)hb_mo_release(route,owner);
    }
}
static void hb_mo_panic(hb_motion_route *route){
    memset(route->bursts,0,sizeof(route->bursts));
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&hb_mo_release(route,owner)){owner->used=0;route->owned--;}
    }
}
/* The adapter supplies the finished output pitch/velocity, optional pan, and
   gate deadline. Skipped notes still get an owner to swallow their matching OFF. */
static int hb_mo_event(hb_motion_route *route,const uint8_t message[3],int pitch,int velocity,int pan,double off_beat,int skip){
    int status=message[0]&0xf0,channel=message[0]&15,source=message[1]&127;
    if(status==0xb0&&source==10)route->base_pan[channel]=message[2]&127;
    if(status!=0x90&&status!=0x80)return hb_mo_push(route,message[0],message[1],message[2]);
    int on=status==0x90&&message[2];
    if(!on){
        hb_motion_owner *oldest=0;
        for(int index=0;index<HB_MOTION_OWNERS;index++){
            hb_motion_owner *owner=&route->owners[index];
            if(owner->used&&!owner->generated&&owner->channel==channel&&owner->source==source&&(!oldest||owner->serial<oldest->serial))oldest=owner;
        }
        if(oldest){
            if(!hb_mo_release(route,oldest))return 0;
            oldest->used=0;route->owned--;return 1;
        }
        /* Notes that were already held when lanes were enabled are unowned. */
        if(route->refs[channel][source])return 1;
        return hb_mo_push(route,0x80|channel,source,0);
    }
    int slot=-1;
    for(int index=0;index<HB_MOTION_OWNERS;index++)if(!route->owners[index].used){slot=index;break;}
    if(slot<0||route->count>HB_MOTION_QUEUE-2)return 0;
    hb_motion_owner *owner=&route->owners[slot];
    *owner=(hb_motion_owner){.used=1,.channel=channel,.source=source,.pitch=pitch,.sounding=!skip,.off_beat=off_beat,.serial=route->next_serial++};route->owned++;
    if(skip)return 1;
    if(pan<0&&route->pan_dirty[channel]){
        hb_mo_push(route,0xb0|channel,10,route->base_pan[channel]);route->pan_dirty[channel]=0;
    }
    if(pan>=0){hb_mo_push(route,0xb0|channel,10,pan);route->pan_dirty[channel]=1;}
    route->refs[channel][pitch]++;
    return hb_mo_push(route,0x90|channel,pitch,velocity);
}
/* Generated notes use the monotonic DSP beat, independent of transport seeks.
   Repeats from different lanes add; they never pass through input processing. */
static int hb_mo_repeat_active(const hb_motion_config *config,int lane,int manual,unsigned revision,unsigned held_serial){
    return config->revision[lane]==revision&&hb_mo_lane_active(config,lane)&&
        (!manual||((config->held&(1u<<lane))&&config->held_serial[lane]==held_serial));
}
static void hb_mo_repeat_cancel(hb_motion_route *route,const hb_motion_config *config,int all){
    for(int index=0;index<HB_MOTION_BURSTS;index++){
        hb_motion_burst *burst=&route->bursts[index];
        if(burst->used&&(all||!hb_mo_repeat_active(config,burst->lane,burst->manual,burst->revision,burst->held_serial)))burst->used=0;
    }
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&owner->generated&&(all||!hb_mo_repeat_active(config,owner->lane,owner->manual,owner->revision,owner->held_serial)))
            if(hb_mo_release(route,owner)){owner->used=0;route->owned--;}
    }
}
static void hb_mo_repeat_schedule(hb_motion_route *route,const hb_motion_config *config,
                                const uint8_t message[3],int pitch,int velocity,double pattern_beat,double condition_beat,double now){
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        const hb_motion_lane *settings=&config->lanes[lane];double value;
        if((settings->operation!=HB_MO_RATCHET&&settings->operation!=HB_MO_ECHO)||!hb_mo_value_at(config,lane,pattern_beat,condition_beat,message[1],&value))continue;
        int ratchet=settings->operation==HB_MO_RATCHET;
        int count=hb_mo_clamp(hb_mo_round(value),ratchet?1:0,16);
        int remaining=count-ratchet;if(remaining<=0)continue;
        int slot=-1;for(int index=0;index<HB_MOTION_BURSTS;index++)if(!route->bursts[index].used){slot=index;break;}
        if(slot<0)continue; /* Overload leaves the original note intact. */
        double spacing=hb_mo_grid(settings->grid)/(ratchet?count:1);
        route->bursts[slot]=(hb_motion_burst){.used=1,.lane=lane,.channel=message[0]&15,.pitch=pitch,.velocity=velocity,
            .remaining=remaining,.manual=(config->held&(1u<<lane))!=0,.decay=ratchet?0:hb_mo_clamp(settings->offset,0,100),
            .revision=config->revision[lane],.held_serial=config->held_serial[lane],.next=now+spacing,.spacing=spacing,.gate=spacing*0.5};
        if(ratchet)for(int index=0;index<HB_MOTION_OWNERS;index++){
            hb_motion_owner *owner=&route->owners[index];
            if(owner->used&&!owner->generated&&owner->serial==route->next_serial-1){
                double deadline=now+spacing*0.5;
                if(owner->repeat_off<=0||deadline<owner->repeat_off)owner->repeat_off=deadline;
                break;
            }
        }
    }
}
static void hb_mo_repeat_tick(hb_motion_route *route,const hb_motion_config *config,double now){
    hb_mo_repeat_cancel(route,config,0);
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&!owner->generated&&owner->repeat_off>0&&now+1e-9>=owner->repeat_off)hb_mo_release(route,owner);
        if(owner->used&&owner->generated&&now+1e-9>=owner->off_beat&&hb_mo_release(route,owner)){owner->used=0;route->owned--;}
    }
    /* Earliest deadline first, at most one attack per burst per audio block. */
    for(int emitted=0;emitted<HB_MOTION_BURSTS;emitted++){
        hb_motion_burst *burst=0;
        for(int index=0;index<HB_MOTION_BURSTS;index++){
            hb_motion_burst *candidate=&route->bursts[index];
            if(candidate->used&&candidate->next<=now+1e-9&&(!burst||candidate->next<burst->next))burst=candidate;
        }
        if(!burst)break;
        burst->velocity=hb_mo_round(burst->velocity*(100-burst->decay)/100.0);
        int slot=-1;for(int index=0;index<HB_MOTION_OWNERS;index++)if(!route->owners[index].used){slot=index;break;}
        if(burst->velocity>0&&slot>=0&&route->count<HB_MOTION_QUEUE-1){
            hb_motion_owner *owner=&route->owners[slot];
            *owner=(hb_motion_owner){.used=1,.channel=burst->channel,.source=-1,.pitch=burst->pitch,.sounding=1,
                .off_beat=now+burst->gate,.serial=route->next_serial++,.generated=1,.lane=burst->lane,.manual=burst->manual,
                .revision=burst->revision,.held_serial=burst->held_serial};
            route->owned++;route->refs[owner->channel][owner->pitch]++;
            hb_mo_push(route,0x90|owner->channel,owner->pitch,burst->velocity);
        }
        burst->remaining--;burst->next+=burst->spacing;
        /* Drop missed attacks instead of dumping a late catch-up burst. */
        while(burst->remaining>0&&burst->next<=now+1e-9){
            burst->remaining--;burst->next+=burst->spacing;
            burst->velocity=hb_mo_round(burst->velocity*(100-burst->decay)/100.0);
        }
        if(burst->remaining<=0||burst->velocity<=0)burst->used=0;
    }
}
#endif
