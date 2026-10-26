#ifndef HB_RHYTHM_GENERATOR_H
#define HB_RHYTHM_GENERATOR_H
/* Bounded shared attack/rest vocabulary; no allocation and no MIDI ownership. */
#define HB_RG_STEPS 16
#define HB_RG_USERS 4
typedef struct {unsigned char kind,accent,gate;} hb_rg_step; /* rest/attack/tie */
typedef struct {int count;hb_rg_step steps[HB_RG_STEPS];} hb_rg_pattern;
/* Euclidean fill is an edit-time pattern builder, never a separate playback clock.
   Rotation moves attacks right; index zero is an attack with zero rotation. */
static int hb_rg_euclidean_at(int step,int length,int hits,int rotation){
    if(length<1||length>HB_RG_STEPS)return 0;
    if(hits<0)hits=0;if(hits>length)hits=length;
    int offset=(step-(rotation%length)+length)%length;
    return hits>0&&((offset*hits)%length)<hits;
}
/* Only attack/rest membership changes: preserve the slot's accents and gates. */
static void hb_rg_euclidean_fill(hb_rg_pattern *pattern,int steps,int hits,int rotation){
    if(steps<1||steps>HB_RG_STEPS)return;
    pattern->count=steps;
    for(int step=0;step<steps;step++)
        pattern->steps[step].kind=hb_rg_euclidean_at(step,steps,hits,rotation)?1:0;
}
static hb_rg_pattern g_rg_users[HB_RG_USERS];
static int g_rg_ready,g_rg_restored;
static const char *HB_RG_NAMES[]={"Off","Straight","Offbeats","Long-Short","Short-Long","3-3-2","User 1","User 2","User 3","User 4"};
static const char *HB_RG_MODES[]={"Repeat","Once","Fit"};
static void hb_rg_init(void){if(g_rg_ready)return;g_rg_ready=1;for(int u=0;u<HB_RG_USERS;u++){g_rg_users[u].count=8;for(int k=0;k<HB_RG_STEPS;k++)g_rg_users[u].steps[k]=(hb_rg_step){1,100,50};}}
static hb_rg_pattern hb_rg_get(int id){
    hb_rg_init();if(id>=6&&id<10)return g_rg_users[id-6];
    hb_rg_pattern p={.count=8};
    for(int k=0;k<8;k++){
        int attack=id==1||(id==2&&k%2)||(id==3&&(k%4==0||k%4==3))||(id==4&&(k%4==0||k%4==1))||(id==5&&(k==0||k==3||k==6));
        p.steps[k]=(hb_rg_step){attack?1:0,k==0?110:90,50};
    }
    return p;
}
/* A tie extends the preceding attack. A rest only suppresses a new attack. */
static double hb_rg_gate(const hb_rg_pattern *p,int step){
    double length=p->steps[step].gate/100.0;
    for(int k=step+1;k<p->count&&p->steps[k].kind==2;k++)length=(k-step)+p->steps[k].gate/100.0;
    return length;
}
typedef struct {int pattern,mode,span,source,edit,cell;int active,index,done;double start,end,gate;int velocity;hb_rg_pattern plan;int euclid_steps,euclid_hits,euclid_rotation,euclid_ready;} hb_rg_clock;
static void hb_rg_euclid_init(hb_rg_clock *clock){
    if(clock->euclid_ready)return;
    clock->euclid_ready=1;clock->euclid_steps=g_rg_users[clock->edit].count;
    clock->euclid_hits=3;clock->euclid_rotation=0;
}
static double hb_rg_span(const hb_rg_clock *r){return (double)(1<<r->span);}
static void hb_rg_reset(hb_rg_clock *r){r->active=r->done=0;r->index=-1;r->gate=0;}
/* Returns the newest attack only. Delayed callbacks never burst missed notes. */
static int hb_rg_advance(hb_rg_clock *r,double beat,int have_notes){
    if(!have_notes){hb_rg_reset(r);return -1;}
    if(r->done)return -1;
    double span=hb_rg_span(r);
    if(!r->active||beat<r->start){
        r->plan=hb_rg_get(r->pattern);r->start=beat;r->end=beat+span;r->index=-1;r->active=1;
        if(r->mode==2)r->end=((long long)(beat/span)+1)*span;
    }
    if(beat>=r->end-1e-9){
        if(r->mode!=0){r->done=r->mode==2?2:1;r->gate=r->end+span/r->plan.count*0.5;r->velocity=100;return r->mode==2?r->plan.count:-1;}
        long long cycles=(long long)((beat-r->start)/span);r->start+=cycles*span;r->end=r->start+span;r->plan=hb_rg_get(r->pattern);r->index=-1;
    }
    double unit=(r->end-r->start)/r->plan.count;
    int step=(int)((beat-r->start+1e-9)/unit);if(step>=r->plan.count)step=r->plan.count-1;
    if(step==r->index)return -1;r->index=step;
    if(unit<0.0625||r->plan.steps[step].kind!=1)return -1;
    r->gate=r->start+unit*(step+hb_rg_gate(&r->plan,step));r->velocity=r->plan.steps[step].accent;
    return step;
}
#endif
