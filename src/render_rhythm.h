#ifndef HB_RENDER_RHYTHM_H
#define HB_RENDER_RHYTHM_H
/* Piecewise-linear, monotone beat warping. Endpoints never move. The same
   weights are used by the clip scheduler; an onset group has one position. */
static double hb_rr_warp(double phase,int pattern){
    static const int weights[6][4]={{1,1,1,1},{1,1,1,1},{3,1,3,1},{1,3,1,3},{4,3,2,1},{1,2,3,4}};
    if(phase<=0)return 0;if(phase>=1)return 1;
    if(pattern<0||pattern>5)pattern=0;
    double scaled=phase*4;int step=(int)scaled,total=0,before=0;
    for(int i=0;i<4;i++){total+=weights[pattern][i];if(i<step)before+=weights[pattern][i];}
    return (before+(scaled-step)*weights[pattern][step])/total;
}
static double hb_rr_time(double beat,int pattern,double span){
    if(pattern<2||span<=0)return beat;
    long long cycle=(long long)(beat/span);if((double)cycle>beat/span)cycle--;
    double start=cycle*span;return start+span*hb_rr_warp((beat-start)/span,pattern);
}
#define HB_RR_VOICES 256
#define HB_RR_EVENTS 512
typedef struct {int used,closed,channel,pitch,sounding;unsigned serial;double delay;} hb_rr_voice;
typedef struct {int used,owner;unsigned serial;double due;unsigned short trail;uint8_t midi[3];} hb_rr_event;
typedef struct {
    hb_rr_voice voices[HB_RR_VOICES];hb_rr_event events[HB_RR_EVENTS];
    unsigned short trail_in,trail_out;unsigned short refs[16][128];unsigned serial;int count,owned,panic,cursor;
} hb_rr_route;
static void hb_rr_panic(hb_rr_route *r){
    memset(r->voices,0,sizeof(r->voices));memset(r->events,0,sizeof(r->events));r->count=0;r->owned=0;r->panic=1;r->cursor=0;
}
static int hb_rr_active(const hb_rr_route *r){
    return r->count||r->owned||r->panic;
}
/* FIFO ownership per channel/pitch; each OFF retains its ON's timing even if
   settings change. Overflow cancels pending attacks and drains sounding OFFs. */
static void hb_rr_push(hb_rr_route *r,const uint8_t midi[3],double now,double delay){
    if(r->panic)return;
    int status=midi[0]&240,ch=midi[0]&15,pitch=midi[1]&127;
    int on=status==0x90&&midi[2],off=status==0x80||(status==0x90&&!midi[2]);
    int owner=-1;
    if(on){
        for(int i=0;i<HB_RR_VOICES;i++)if(!r->voices[i].used){owner=i;break;}
        if(owner<0){hb_rr_panic(r);return;}
        r->owned++;r->voices[owner]=(hb_rr_voice){.used=1,.channel=ch,.pitch=pitch,.serial=++r->serial,.delay=delay>0?delay:0};
    }else if(off||status==0xa0){
        for(int i=0;i<HB_RR_VOICES;i++){
            hb_rr_voice *v=&r->voices[i];
            if(v->used&&!v->closed&&v->channel==ch&&v->pitch==pitch&&(owner<0||v->serial<r->voices[owner].serial))owner=i;
        }
        delay=owner<0?0:r->voices[owner].delay;
        if(off&&owner>=0)r->voices[owner].closed=1;
    }else delay=0;
    for(int i=0;i<HB_RR_EVENTS;i++)if(!r->events[i].used){
        hb_rr_event *e=&r->events[i];*e=(hb_rr_event){.used=1,.owner=owner,.serial=++r->serial,.due=now+delay};
        e->trail=on?r->trail_in:0;memcpy(e->midi,midi,3);r->count++;return;
    }
    hb_rr_panic(r);
}
static int hb_rr_pop(hb_rr_route *r,double now,uint8_t midi[3]){
    r->trail_out=0;
    if(r->panic){
        while(r->cursor<2048){int i=r->cursor++;if(r->refs[i/128][i%128]){
            r->refs[i/128][i%128]=0;midi[0]=0x80|i/128;midi[1]=i%128;midi[2]=0;return 1;
        }}r->panic=0;
    }
    while(r->count){
        int best=-1;
        for(int i=0;i<HB_RR_EVENTS;i++)if(r->events[i].used&&r->events[i].due<=now+1e-9&&(best<0||r->events[i].due<r->events[best].due||(r->events[i].due==r->events[best].due&&r->events[i].serial<r->events[best].serial)))best=i;
        if(best<0)return 0;
        hb_rr_event e=r->events[best];
        int status=e.midi[0]&240,ch=e.midi[0]&15,pitch=e.midi[1]&127;
        if(status==0x90&&e.midi[2]&&r->refs[ch][pitch]){
            /* MIDI has no voice IDs: close the previous attack before a
               retrigger, and swallow that old owner's later release. This
               keeps downstream receiver gates balanced as well. */
            r->refs[ch][pitch]=0;
            for(int i=0;i<HB_RR_VOICES;i++)if(r->voices[i].used&&r->voices[i].channel==ch&&r->voices[i].pitch==pitch)r->voices[i].sounding=0;
            midi[0]=0x80|ch;midi[1]=pitch;midi[2]=0;return 1;
        }
        r->events[best].used=0;r->count--;
        if(status==0x90&&e.midi[2]){r->refs[ch][pitch]=1;if(e.owner>=0)r->voices[e.owner].sounding=1;}
        else if(status==0x80||(status==0x90&&!e.midi[2])){
            if(e.owner>=0&&r->voices[e.owner].used){
                int sounding=r->voices[e.owner].sounding;r->voices[e.owner].used=0;r->owned--;
                if(!sounding)continue;
            }else if(r->refs[ch][pitch])continue;
            r->refs[ch][pitch]=0;
        }
        r->trail_out=e.trail;memcpy(midi,e.midi,3);return 1;
    }return 0;
}
#endif
