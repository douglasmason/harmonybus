/* Shared parameter lifecycle, legacy states, and tempo-relative capture. */
#include <assert.h>
#include <math.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../modules/harmonybus/dsp/harmonybus.c"
static float tempo=120.0f;
static double position=0.8;
static float bpm(void){return tempo;}
static double beat(void){return position;}
static int clock_state(void){return 2;}
static void expect(Inst *instance,const char *expected){
    char value[512];API.get_param(instance,"boundary_buffer_ms",value,sizeof(value));
    assert(!strcmp(value,expected));
}
static void musical_leading_edges(Inst *instance){
    for(int bpm_index=0;bpm_index<3;bpm_index++)for(int division=0;division<8;division++)
    for(int schedule=0;schedule<14;schedule++){
        tempo=bpm_index==0?60:(bpm_index==1?120:180);
        double width=0.0625*(1u<<division);
        g_bus.boundary_buffer_ms=-division-1;g_bus.anticipation=0;
        g_bus.chord_timing=schedule==0?6:0;instance->quant_timing=schedule==1?6:0;
        static const int offsets[]={0,0,3,10,13,14,15,16,17,18,19,20,21,22};
        g_bus.next_predict=1;g_bus.next_lookahead=offsets[schedule];
        g_bus.next_model_locked=schedule>=2;g_bus.next_model_count=1;
        g_bus.clip_loop_start=0;g_bus.clip_loop_end=32;
        double target=schedule>=2?16:8;
        g_bus.next_model[0].phase=target+hb_next_lookahead_beats();
        for(int edge=0;edge<3;edge++){
            /* At the stated edge and 0.5 ms inside it: no future capture.
               1.5 ms inside it: capture the unchanged target. */
            position=target-width+hb_ms_to_beats(edge==0?0:edge==1?1:3)/2;
            instance->follower_queue_count=0;hb_queue_follower_event(instance,60,100,1,0);
            double actual=instance->follower_queue_target_beat[0];
            if(edge<2)assert(actual<0||fabs(actual-position)<1e-6);
            else assert(fabs(actual-target)<1e-6);
        }
        position=target;instance->follower_queue_count=0;
        hb_queue_follower_event(instance,60,100,1,0);
        assert(fabs(instance->follower_queue_target_beat[0]-target)<1e-6);
    }
    g_bus.next_model_locked=0;g_bus.next_lookahead=0;
}
int main(void){
    hb_global_shared_t shared={0};g_global_shared=&shared;
    host_api_v1_t host={.sample_rate=48000,.get_bpm=bpm,.get_beat_position=beat,.get_clock_status=clock_state};
    move_midi_fx_init(&host);
    Inst *first=API.create_instance("",NULL),*second=API.create_instance("",NULL);
    expect(first,"1/16");expect(second,"1/16");
    API.set_param(first,"boundary_buffer_ms","1/8");expect(second,"1/8");
    char state[512];API.get_param(second,"state",state,sizeof(state));
    API.set_param(second,"state","hb16,1,0,0,25,2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,20,60,0,0,0,3");
    expect(first,"1/8"); /* stale per-instance rehydration cannot undo the edit */
    second->role=1;second->quant_timing=3;
    for(int trial=0;trial<2;trial++){
        tempo=trial?60.0f:180.0f;position=0.7;
        second->follower_queue_count=0;
        hb_queue_follower_event(second,60,100,1,0);
        assert(fabs(second->follower_queue_target_beat[0]-1.0)<1e-9);
    }
    API.set_param(first,"boundary_buffer_ms","350 ms");
    tempo=60;position=0.6;second->follower_queue_count=0;
    hb_queue_follower_event(second,60,100,1,0);assert(second->follower_queue_target_beat[0]<0);
    tempo=120;second->follower_queue_count=0;
    hb_queue_follower_event(second,60,100,1,0);assert(second->follower_queue_target_beat[0]==1.0);
    position=1.0;second->follower_queue_count=0;
    hb_queue_follower_event(second,60,100,1,0);assert(second->follower_queue_target_beat[0]==1.0);
    g_bus.next_lookahead=3;g_bus.next_model_locked=1;g_bus.next_model_count=1;
    g_bus.next_model[0].phase=1.5;g_bus.clip_loop_start=0;g_bus.clip_loop_end=4;
    assert(fabs(hb_next_effective_boundary(1.0)-1.0)<1e-9);
    /* Negative lookahead moves BOTH harmony and its pre-boundary capture.
       C at beat 0, D at beat 4; -1/4 shifts their boundaries to 1 and 5. */
    uint8_t c_notes[3]={60,64,67},d_notes[3]={62,66,69};
    hb_harmony_t c=hb_infer_harmony(c_notes,3),d=hb_infer_harmony(d_notes,3);
    g_bus.clip_loop_end=8;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=c};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=d};
    API.set_param(first,"next_lookahead","-1/4");
    char label[32];API.get_param(second,"next_lookahead",label,sizeof(label));
    assert(!strcmp(label,"-1/4"));
    API.set_param(first,"boundary_buffer_ms","1/16");
    second->quant_timing=0;g_bus.chord_timing=5;
    position=4.74;hb_next_apply_effective(position);
    assert(bus_read().root_pc==0);
    second->follower_queue_count=0;hb_queue_follower_event(second,60,100,1,0);
    assert(second->follower_queue_target_beat[0]<0);
    for(int trial=0;trial<2;trial++){
        tempo=trial?60:180;position=4.75;
        second->follower_queue_count=0;hb_queue_follower_event(second,60,100,1,0);
        assert(second->follower_queue_target_beat[0]<0); /* nominal leading edge is excluded */
        position=4.76;
        second->follower_queue_count=0;hb_queue_follower_event(second,60,100,1,0);
        assert(fabs(second->follower_queue_target_beat[0]-5.0)<1e-9);
    }
    shared.follower_root_policy=2;shared.follower_explicit_root=0;
    uint8_t out[16][3];int lengths[16];
    assert(hb_release_follower_queue(second,64,48000,out,lengths,16)==1);
    assert(out[0][1]==62&&bus_read().root_pc==0); /* immediate predicted D, private context */
    position=5.0;hb_next_apply_effective(position);
    assert(bus_read().root_pc==2);
    assert(hb_release_follower_queue(second,64,48000,out,lengths,16)==0);
    /* Exact shifted boundary is immediate; wrap preserves the previous chord. */
    second->follower_queue_count=0;hb_queue_follower_event(second,64,100,1,0);
    assert(second->follower_queue_target_beat[0]==5.0);
    hb_next_apply_effective(0.75);assert(bus_read().root_pc==2);
    hb_next_apply_effective(1.0);assert(bus_read().root_pc==0);
    /* Quant Grid competes independently, even inside the late-harmony window. */
    position=4.7;second->quant_timing=1;second->follower_queue_count=0;
    hb_queue_follower_event(second,60,100,1,0);
    assert(second->follower_queue_target_beat[0]==4.75);
    g_bus.observed_harmony=d;API.set_param(first,"next_predict","Off");
    hb_next_apply_effective(1.0);assert(bus_read().root_pc==2);
    assert(hb_next_effective_boundary(1.0)<0);
    g_bus.next_lookahead=0;g_bus.next_model_locked=0;
    musical_leading_edges(second);
    API.destroy_instance(first);API.destroy_instance(second);
    first=API.create_instance("",NULL);API.set_param(first,"state",state);expect(first,"1/8");
    API.destroy_instance(first);
    first=API.create_instance("",NULL);expect(first,"1/16");
    API.set_param(first,"state","hb16,1,0,0,25,2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,20,60,0,0,0,3");
    expect(first,"20 ms");API.destroy_instance(first);
    puts("global buffer: defaults, restore, shared edits, BPM and exact-grid capture pass");
}
