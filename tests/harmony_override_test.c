#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
static void choose(Inst *instance,int operation){
    instance->motion.lanes[0].operation=operation;
    API.set_param(instance,"motion_gesture_1","LatchOn");
}
static void release(Inst *instance){
    API.set_param(instance,"motion_gesture_1","LatchOff");
}
static void pending_input(Inst *instance,int pitch,int on,int recorded){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)},output[256][3];int lengths[256];
    instance->movy_playback=recorded;
    API.process_midi(instance,message,3,output,lengths,256);
    instance->movy_playback=0;
    g_conductor_block_ready=1;
    API.tick(instance,24000,48000,output,lengths,256);
}
static void input(Inst *instance,int pitch,int on,int recorded){
    pending_input(instance,pitch,on,recorded);
    hb_override_commit();
}
static void resolved_intent(void){
    Inst *source=fixture(),*follower=API.create_instance("",0);
    API.set_param(follower,"role","Follower");
    API.set_param(source,"chord_mode","Scale Root");
    API.set_param(source,"track_chord_form","Rootless 7");
    hb_commit_observed_harmony(chord(0,0,0));
    choose(source,HB_MO_LIVE_HARMONY_OVERRIDE);
    /* A post-mapping transpose must affect the published chord, including
       its absent root. The raw C press still owns release. */
    source->motion.lanes[1].operation=HB_MO_TRANSPOSE;
    source->motion.lanes[1].amount=2;
    source->motion.lanes[1].offset=0;
    API.set_param(source,"motion_gesture_2","LatchOn");
    pending_input(source,60,1,0);
    assert(g_override[0].pending);
    assert(g_override_winner==-1);
    API.set_param(source,"hb_movy_block","900,128,48000");
    hb_harmony_t result=hb_render_harmony(follower);
    assert(result.valid&&result.root_pc==2);
    assert(hb_harmony_chord_mask(result)&(1u<<6)); /* F# from C's E. */
    assert(g_bus.observed_harmony.root_pc==0);
    assert(follower->play_revision>0);
    unsigned long long capture_order=g_override_order;
    int replayed_pitch=60;
    hb_override_capture(source,60,1,0,&replayed_pitch,1,0);
    assert(g_override_order==capture_order&&!g_override[0].pending);
    /* Visual previews are copies, never additional authority owners. */
    Inst *preview=malloc(sizeof(*preview));assert(preview);
    memcpy(preview,follower,sizeof(*preview));
    assert(hb_override_index(preview)==-1);
    unsigned long long order=g_override_order;
    int preview_pitch=71;
    hb_override_capture(preview,71,1,11,&preview_pitch,1,0);
    assert(g_override_order==order);
    assert(hb_render_harmony(preview).root_pc==2);
    free(preview);
    /* The second chain's heartbeat cannot publish a newer event halfway
       through the same block. */
    pending_input(source,67,1,0);
    API.set_param(follower,"hb_movy_block","900,128,48000");
    assert(hb_render_harmony(follower).root_pc==2);
    assert(g_override[0].pending);
    API.set_param(follower,"hb_movy_block","901,128,48000");
    assert(!g_override[0].pending);
    input(source,60,0,0);
    assert(!g_override[0].eligible[0][60]);
    release(source);
    assert(hb_render_harmony(follower).root_pc==0);
    API.destroy_instance(source);API.destroy_instance(follower);
}
static void scheduled_motif_intent(void){
    Inst *source=fixture(),*follower=API.create_instance("",0);
    API.set_param(follower,"role","Follower");
    source->travel_map=7;
    API.set_param(source,"track_chord_form","Rootless 7");
    hb_commit_observed_harmony(chord(0,0,0));
    choose(source,HB_MO_LIVE_HARMONY_OVERRIDE);
    hb_mt_phrase phrase={0};phrase.count=1;phrase.anchor=0;
    phrase.events[0].count=1;phrase.events[0].duration=24;
    phrase.events[0].chord_mode=1;
    phrase.events[0].notes[0]=(hb_mt_note){60,100};
    position=0;
    assert(hb_mt_schedule(source,&phrase,67,100,0,0,1,2,-1,0));
    assert(!g_override[0].pending&&g_override_winner==-1);
    uint8_t output[64][3];int lengths[64];
    position=1;hb_mt_tick(source,output,lengths,64);
    assert(g_override_winner==-1&&!g_override[0].pending);
    position=2;hb_mt_tick(source,output,lengths,64);
    assert(g_override[0].pending&&g_override_winner==-1);
    hb_override_commit();
    assert(hb_render_harmony(follower).root_pc==7);
    unsigned long long order=g_override_order;
    hb_mt_tick(source,output,lengths,64);
    assert(g_override_order==order); /* Sustaining isn't another intent. */
    assert(hb_mt_schedule(source,&phrase,62,100,0,0,1,4,-1,0));
    release(source);choose(source,HB_MO_LIVE_HARMONY_OVERRIDE);
    position=4;hb_mt_tick(source,output,lengths,64);hb_override_commit();
    assert(g_override_winner==-1); /* Old queued phrase cannot rearm it. */
    API.destroy_instance(source);API.destroy_instance(follower);position=0;
}
static void leading_tone_dominant(void){
    Inst *source=fixture();
    hb_cp_config config=source->player.config;config.size=3;
    hb_approach_result result=hb_resolve_chord_approach(source,71,hb_explicit_scale_mask(0,1),config,chord(0,0,0),2,0,0,0);
    assert(result.root==70&&result.config.quality==9&&result.intent_minor);
    int pitches[HB_CP_VOICES];unsigned semantic=0;
    int count=hb_cp_voice_semantic(result.config,result.root,0,0,result.scale,pitches,&semantic);
    unsigned mask=0;for(int index=0;index<count;index++)mask|=1u<<mod12(pitches[index]);
    assert(mask==((1u<<7)|(1u<<10)|(1u<<1)|(1u<<4)));
    API.destroy_instance(source);
}
int main(void){
    Inst *first=fixture(),*second=API.create_instance("",0);
    API.set_param(second,"role","Follower");
    first->movy_track=4;second->movy_track=5;
    API.set_param(first,"chord_mode","Scale Root");
    API.set_param(first,"track_chord_form","Triad");
    API.set_param(second,"chord_mode","Scale Root");
    API.set_param(second,"track_chord_form","Triad");
    hb_commit_observed_harmony(chord(0,0,0));
    choose(first,HB_MO_HARMONY_OVERRIDE);
    input(first,67,1,1);
    assert(g_override_winner==0);
    assert(g_override[g_override_winner].harmony.root_pc==7);
    assert(g_bus.observed_harmony.root_pc==0);
    input(first,67,1,0);input(first,67,0,1);
    assert(g_override[0].held[0][67]==1&&g_override[0].held[1][67]==0);
    hb_commit_observed_harmony(chord(2,1,0));
    assert(g_override[g_override_winner].harmony.root_pc==7);
    release(first);
    assert(hb_render_harmony(second).root_pc==2);

    choose(first,HB_MO_LIVE_HARMONY_OVERRIDE);
    input(first,65,1,1);assert(g_override_winner==-1);
    int expected=mod12(hb_map_follower_note_unoperated(first,65));
    input(first,65,1,0);assert(g_override[g_override_winner].harmony.root_pc==expected);
    input(first,65,0,1);assert(g_override[0].held[0][65]==1);
    choose(second,HB_MO_HARMONY_OVERRIDE);
    input(second,67,1,1);assert(g_override_winner==1);
    release(second);assert(g_override_winner==0&&g_override[0].harmony.root_pc==expected);
    g_bus.next_model_locked=1;g_bus.next_model_count=1;g_bus.next_model[0].harmony=chord(11,0,0);
    second->next_lookahead=1;
    assert(g_override[g_override_winner].harmony.root_pc==expected);
    API.set_param(first,"transpose","2");
    assert(g_bus.global_transpose==2);
    assert(hb_render_harmony(second).root_pc==mod12(expected+g_bus.global_transpose));
    unsigned long long captured[HB_MOTION_LANES+1];
    hb_mo_capture(&first->motion,0,0,0,captured);
    assert(!(captured[0]&HB_MO_RECORDED));
    char status[1024];API.get_param(first,"shared_context_snapshot",status,sizeof(status));
    assert(strstr(status,"Override T5 LIVE"));
    hb_stop_instance_note_state(first);assert(g_override_winner==-1);
    API.destroy_instance(first);API.destroy_instance(second);
    resolved_intent();
    scheduled_motif_intent();
    leading_tone_dominant();
    puts("Harmony override: live/recorded ownership, overlap, underlying progression, lookahead, transpose and stop pass");
    return 0;
}
