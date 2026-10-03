/* Temporary harmonic authority; the conductor timeline continues underneath. */
typedef struct {
    int active,pending;
    uint8_t held[2][128],eligible[2][128];
    uint16_t masks[2][128];
    hb_harmony_t harmony,next;
    unsigned long long order,next_order,generation;
} hb_override_owner;
static hb_override_owner g_override[HB_MAX_INSTANCES];
static int g_override_winner=-1;
static unsigned long long g_override_order;
static unsigned long long g_override_generation;
static hb_harmony_t g_override_effective;
static int hb_override_index(const Inst *instance){
    /* Pad/opening previews use temporary Inst copies outside the pool.
       Pointer subtraction on those copies is undefined, even before indexing. */
    __UINTPTR_TYPE__ address=(__UINTPTR_TYPE__)instance,base=(__UINTPTR_TYPE__)g_pool;
    if(address<base||address-base>=sizeof(g_pool)||(address-base)%sizeof(Inst))return -1;
    return (int)((address-base)/sizeof(Inst));
}
static void hb_override_choose(void){
    int winner=-1;
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&&g_pool[index].role==1&&g_override[index].active&&g_override[index].harmony.valid)
        if(winner<0||g_override[index].order>g_override[winner].order)winner=index;
    hb_harmony_t effective=winner<0?(hb_harmony_t){0}:g_override[winner].harmony;
    if(memcmp(&effective,&g_override_effective,sizeof(effective))){
        /* Wake existing held-note revoicing without rewriting conductor
           evidence or making an unchanged authority a new performance event. */
        for(int index=0;index<HB_MAX_INSTANCES;index++)
            if(g_pool[index].used&&g_pool[index].role==1&&g_override[index].active!=2)
                g_pool[index].play_revision++;
        g_override_effective=effective;
    }
    g_override_winner=winner;
}
static void hb_override_clear(Inst *instance){
    int index=hb_override_index(instance);if(index<0)return;
    memset(&g_override[index],0,sizeof(g_override[0]));
    g_override[index].generation=++g_override_generation;hb_override_choose();
}
static void hb_override_reset(void){
    memset(g_override,0,sizeof(g_override));memset(&g_override_effective,0,sizeof(g_override_effective));g_override_winner=-1;g_override_order=0;
}
static int hb_override_read(Inst *instance,hb_harmony_t *harmony){
    /* An authority resolves against the conductor, never itself or another
       authority. This prevents a cyclic chain between two overriding tracks. */
    if(g_override_winner<0||instance->role!=1)return 0;
    int index=hb_override_index(instance);
    if(index>=0&&(g_override[index].active==2||(g_override[index].active==1&&!instance->movy_playback)))return 0;
    *harmony=g_override[g_override_winner].harmony;return 1;
}
static void hb_override_commit(void){
    int changed=0;
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_override[index].pending){
        g_override[index].harmony=g_override[index].next;g_override[index].order=g_override[index].next_order;
        g_override[index].pending=0;changed=1;
    }
    if(changed)hb_override_choose();
}
static void hb_override_set_scope(Inst *instance,int active){
    int index=hb_override_index(instance);if(index<0)return;
    if(active!=g_override[index].active){hb_override_clear(instance);g_override[index].active=active;}
}
static void hb_override_sync(void){
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *instance=&g_pool[index];int active=0;
        if(instance->used&&instance->role==1&&!instance->motion.bypass)
            for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)if(instance->motion.held&(1ULL<<lane)){
                int operation=instance->motion.lanes[lane].operation;
                if(operation==HB_MO_HARMONY_OVERRIDE)active=2;
                else if(operation==HB_MO_LIVE_HARMONY_OVERRIDE&&!active)active=1;
            }
        hb_override_set_scope(instance,active);
    }
}
static void hb_override_capture(Inst *instance,int source,int on,int root,const int *pitches,int count,unsigned semantic);

static void hb_override_mark(Inst *instance,const uint8_t *input,int length){
    int index=hb_override_index(instance);if(index<0)return;
    hb_override_owner *owner=&g_override[index];int origin=instance->movy_playback!=0;
    if(owner->active&&(owner->active==2||!origin)&&length>=3&&(input[0]&0xf0)==0x90&&input[2])
        owner->eligible[origin][input[1]&127]=2;
}
