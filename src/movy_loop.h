#ifndef HB_MOVY_LOOP_H
#define HB_MOVY_LOOP_H
/* Version-one runtime metadata: master tick, effective period, phase origin,
   content revision, activity (0/1/2), running, PPQN. Never serialized as a preset. */
typedef unsigned long long hb_tick_t;
typedef struct {
    hb_tick_t tick, period, origin, revision;
    unsigned active, running, ppqn;
    int present;
} hb_movy_clip_t;
static hb_tick_t hb_tick_gcd(hb_tick_t left,hb_tick_t right){
    while(right){hb_tick_t remainder=left%right;left=right;right=remainder;}return left;
}
/* Cap at exact-double integer range; refuse rather than silently shorten. */
static int hb_tick_lcm(hb_tick_t left,hb_tick_t right,hb_tick_t *result){
    if(!left||!right)return 0;
    hb_tick_t quotient=left/hb_tick_gcd(left,right);
    if(quotient>9007199254740991ULL/right)return 0;
    *result=quotient*right;return 1;
}
#endif
