#ifndef HB_SHARED_CONTEXT_H
#define HB_SHARED_CONTEXT_H
/* Owned contributions, normalized before master transpose. No audio-thread
   allocation. Replay updates are staged and resolved once before the MIDI batch. */
enum { HB_SC_KEY,HB_SC_PARALLEL,HB_SC_PARENT,HB_SC_KINDS };
typedef struct { int on,a,b,c; unsigned long long order; } hb_sc_value;
typedef struct { int owner,kind; hb_sc_value value; } hb_sc_event;
static hb_sc_value g_sc_live[HB_MAX_INSTANCES][HB_SC_KINDS],g_sc_replay[16][HB_SC_KINDS];
static int g_sc_manual[HB_MAX_INSTANCES],g_sc_latch[HB_MAX_INSTANCES],g_sc_lane[HB_MAX_INSTANCES];
static int g_sc_owner[HB_SC_KINDS],g_sc_recorded[HB_SC_KINDS],g_sc_ready,g_sc_dirty,g_sc_arm_owner=-1;
static unsigned long long g_sc_serial;
static hb_key_context g_sc_base;
static hb_sc_event g_sc_events[64];
static int g_sc_head,g_sc_count,g_sc_record_track=-1;
static void hb_sc_reset(void){
    memset(g_sc_live,0,sizeof(g_sc_live));memset(g_sc_replay,0,sizeof(g_sc_replay));
    memset(g_sc_manual,0,sizeof(g_sc_manual));memset(g_sc_latch,0,sizeof(g_sc_latch));memset(g_sc_lane,0,sizeof(g_sc_lane));
    memset(&g_sc_base,0,sizeof(g_sc_base));g_sc_ready=g_sc_dirty=g_sc_head=g_sc_count=0;g_sc_serial=0;g_sc_arm_owner=-1;g_sc_record_track=-1;
    for(int kind=0;kind<HB_SC_KINDS;kind++){g_sc_owner[kind]=-1;g_sc_recorded[kind]=0;}
}
#endif
