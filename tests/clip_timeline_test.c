#define main loop_learning_main
#include "loop_learning_test.c"
#undef main
static void begin(void){
    reset_fixture();memset(g_timelines,0,sizeof(g_timelines));memset(g_timeline_owners,0,sizeof(g_timeline_owners));
    memset(g_clip_cache,0,sizeof(g_clip_cache));memset(&g_clip_cache_key,0,sizeof(g_clip_cache_key));
    g_clip_cache_key_valid=0;g_timeline_dirty=0;
    for(int index=2;index<4;index++)g_pool[index].role=1;
    for(int index=0;index<2;index++){
        g_pool[index].movy_track=index;
        g_movy_clips[index]=(hb_movy_clip_t){.present=1,.active=1,.running=1,.ppqn=96,.period=384,.slot=0,.revision=(hb_tick_t)(100+index)};
    }
    hb_movy_refresh();
}
static void timeline_tick(unsigned tick){
    for(int index=0;index<2;index++)g_movy_clips[index].tick=tick;
    hb_movy_refresh();
    hb_timeline_finalize();
}
static void hear(int index,int root){hb_timeline_observe(&g_pool[index],chord(root));}
int main(void){
    begin();hear(0,0);timeline_tick(96);hear(1,7);timeline_tick(192);hear(0,5);timeline_tick(288);hear(1,2);timeline_tick(384);
    assert(g_bus.next_model_locked&&g_bus.next_model_count==4);
    const int roots[]={0,7,5,2};
    for(int index=0;index<4;index++){assert(g_bus.next_model[index].phase==index);assert(g_bus.next_model[index].harmony.root_pc==roots[index]);}
    int first=g_timeline_owners[0].entry,second=g_timeline_owners[1].entry;
    assert(g_timelines[first-1].ready&&g_timelines[second-1].ready);
    /* A temporary conductor color suspends predictions, not the saved model. */
    API.set_param(&g_pool[0],"dominant_color","Down");hb_movy_refresh();
    assert(g_movy_blocked==3&&g_timelines[first-1].ready);
    API.set_param(&g_pool[0],"dominant_color","Up");hb_movy_refresh();
    assert(!g_movy_blocked&&g_bus.next_model_locked&&g_timelines[first-1].ready);
    /* Persist both tracks, then simulate a new process and the first Play. */
    char savedA[32768]={0},savedB[32768]={0};
    assert(hb_timeline_save(&g_pool[0],savedA,sizeof(savedA),0)>0);
    assert(hb_timeline_save(&g_pool[1],savedB,sizeof(savedB),0)>0);
    begin();
    /* Runtime edit counters deliberately differ after restoring settings. */
    g_pool[0].motion.revision[0]+=25;
    hb_timeline_restore(savedA);hb_timeline_restore(savedB);
    for(int lane=0;lane<2;lane++)g_movy_clips[lane].running=0;
    timeline_tick(0);assert(g_bus.next_model_locked&&g_bus.next_model_count==4);
    for(int lane=0;lane<2;lane++)g_movy_clips[lane].running=1;
    timeline_tick(0);assert(g_bus.next_model_locked&&g_bus.next_model_count==4);
    hear(0,0);timeline_tick(96);hear(1,7);timeline_tick(192);hear(0,5);timeline_tick(288);hear(1,2);
    assert(g_bus.next_model_locked);
    first=g_timeline_owners[0].entry;second=g_timeline_owners[1].entry;
    /* A truncated cache document never replaces a known valid timeline. */
    hb_timeline_restore(";tl1,0,0,100,384,1,0,2:0,0,0,145,0,0,100,0,0,0,0");
    assert(g_timelines[first-1].ready&&g_timelines[first-1].count==2);
    /* Change only A. B's independent timeline survives the unknown combination. */
    g_movy_clips[0].slot=1;g_movy_clips[0].revision=200;timeline_tick(384);hear(0,9);
    assert(!g_bus.next_model_locked&&g_timelines[second-1].ready);
    timeline_tick(480);hear(1,7);timeline_tick(576);hear(0,4);timeline_tick(672);hear(1,2);timeline_tick(768);
    assert(g_bus.next_model_locked);
    /* Recombine A with a shifted B, a combination never learned or cached. */
    g_movy_clips[0].slot=0;g_movy_clips[0].revision=100;
    g_movy_clips[1].origin=48;timeline_tick(768);
    assert(g_bus.next_model_locked&&g_timeline_owners[0].entry==first);
    assert(g_bus.next_model_count==4&&g_bus.next_model[1].phase==1.5);
    /* A surprise evicts the offending track only. */
    hear(0,3);assert(!g_timelines[first-1].ready&&g_timelines[second-1].ready);
    /* A silent/missing transition is also detected independently. */
    timeline_tick(940);assert(!g_timelines[second-1].ready);
    begin();g_pool[1].role=1;
    hb_motion_lane *lane=&g_pool[0].motion.lanes[0];
    hb_mo_lane_default(lane);lane->operation=HB_MO_CHORD_FORM;lane->amount=12;lane->every=2;lane->from=lane->through=2;
    hb_movy_refresh();assert(g_movy_period==768);hear(0,0);
    timeline_tick(192);hear(0,5);timeline_tick(384);
    assert(!g_timelines[g_timeline_owners[0].entry-1].ready);
    hear(0,2);timeline_tick(576);hear(0,7);timeline_tick(768);
    assert(g_timelines[g_timeline_owners[0].entry-1].ready);
    assert(g_bus.next_model_locked&&g_bus.next_model_count==4);
    assert(g_bus.next_model[2].phase==4&&g_bus.next_model[2].harmony.root_pc==2);

    /* Startup can miss the chord already sounding at phase zero. Metadata for
       the first wrap arrives before its conductor MIDI, so the boundary chord
       must join that first traversal before the model is promoted. */
    begin();g_pool[1].role=1;hb_movy_refresh();
    const unsigned ticks[]={96,192,288};const int learned[]={7,5,2};
    for(int index=0;index<3;index++){
        for(int lane=0;lane<2;lane++)g_movy_clips[lane].tick=ticks[index];
        hb_movy_refresh();hear(0,learned[index]);hb_timeline_finalize();
    }
    for(int lane=0;lane<2;lane++)g_movy_clips[lane].tick=384;
    hb_movy_refresh();
    assert(!g_bus.next_model_locked); /* Boundary MIDI has not arrived yet. */
    hear(0,0);hb_timeline_finalize();
    assert(g_bus.next_model_locked&&g_bus.next_model_count==4);
    assert(g_bus.next_model[0].phase==0&&g_bus.next_model[0].harmony.root_pc==0);
    puts("clip timelines: independent learning, unseen combinations, relative launches and per-track divergence pass");
    return 0;
}
