#define main loop_learning_main
#include "loop_learning_test.c"
#undef main

static void launch(int slot,unsigned revision,unsigned tick,unsigned origin){
    char message[128];
    snprintf(message,sizeof(message),"%u,384,%u,%u,1,1,96,0,%d",tick,origin,revision,slot);
    API.set_param(&g_pool[0],"hb_movy_clip",message);
    hb_movy_refresh();
}
static void learn(int root){
    g_bus.next_learning_count=2;
    g_bus.next_learning[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(root)};
    g_bus.next_learning[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(root+5)};
    hb_next_promote_learning();
}
static void reset_cache(void){
    reset_fixture();
    memset(g_clip_cache,0,sizeof(g_clip_cache));
    memset(&g_clip_cache_key,0,sizeof(g_clip_cache_key));
    g_clip_cache_key_valid=0;
    for(int index=1;index<4;index++)g_pool[index].role=1;
}
int main(void){
    reset_cache();
    launch(0,100,0,0);learn(0);
    launch(1,200,384,0);assert(!g_bus.next_model_locked);learn(2);
    /* Relaunch A half a bar later: phases remain clip-relative. */
    launch(0,100,960,192);
    assert(g_bus.next_model_locked&&g_bus.next_model_count==2);
    assert(g_bus.next_model[0].harmony.root_pc==0);
    assert(hb_next_phase(hb_clip_playhead())==0);
    assert(g_bus.next_model[hb_next_model_event_for_phase(2,0)].harmony.root_pc==5);
    /* A confirmed surprise evicts A but does not evict B. */
    hb_next_begin_relearning();
    launch(1,200,1152,0);assert(g_bus.next_model_locked);
    launch(0,100,1536,0);assert(!g_bus.next_model_locked);
    learn(0);
    /* Replacement of slot A drops its earlier content, even if later undone. */
    launch(0,101,1920,0);assert(!g_bus.next_model_locked);
    launch(1,200,2304,0);assert(g_bus.next_model_locked);
    launch(0,100,2688,0);assert(!g_bus.next_model_locked);
    learn(0);
    /* Form changes never resurrect a stale rendered harmony. */
    g_pool[0].player.config.size=3;
    hb_movy_refresh();assert(!g_bus.next_model_locked);
    learn(0);
    API.set_param(&g_pool[0],"next_reset","Reset");
    launch(1,200,3072,0);
    launch(0,100,3456,0);assert(!g_bus.next_model_locked);
    /* Initial silence does not invent a previous-loop chord on launch. */
    reset_cache();launch(0,100,0,0);
    g_bus.next_learning_count=2;
    g_bus.next_learning[0]=(hb_loop_harmony_event_t){.phase=1,.harmony=chord(0)};
    g_bus.next_learning[1]=(hb_loop_harmony_event_t){.phase=3,.harmony=chord(5)};
    hb_next_promote_learning();
    launch(1,200,384,0);launch(0,100,768,0);
    assert(g_bus.next_model_locked);
    assert(hb_next_model_event_for_phase(0,0)==-1);
    assert(hb_next_model_event_for_phase(1,0)==0);
    /* An absent transition must invalidate without receiving a new chord. */
    reset_cache();launch(0,100,0,0);learn(0);
    g_bus.observed_harmony=chord(0);
    g_movy_tick=220;hb_next_update_playhead(64,48000);
    assert(!g_bus.next_model_locked);
    launch(1,200,384,0);launch(0,100,768,0);
    assert(!g_bus.next_model_locked);
    puts("clip_cache_test: launch phase, retained clips, divergence, edits, forms and reset pass");
    return 0;
}
