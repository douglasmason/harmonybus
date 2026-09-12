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
int main(void){
    hb_global_shared_t shared={0};g_global_shared=&shared;
    host_api_v1_t host={.sample_rate=48000,.get_bpm=bpm,.get_beat_position=beat,.get_clock_status=clock_state};
    move_midi_fx_init(&host);
    Inst *first=API.create_instance("",NULL),*second=API.create_instance("",NULL);
    expect(first,"350 ms");expect(second,"350 ms");
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
    g_bus.next_lookahead=0;g_bus.next_model_locked=0;
    API.destroy_instance(first);API.destroy_instance(second);
    first=API.create_instance("",NULL);API.set_param(first,"state",state);expect(first,"1/8");
    API.destroy_instance(first);
    first=API.create_instance("",NULL);expect(first,"350 ms");
    API.set_param(first,"state","hb16,1,0,0,25,2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,20,60,0,0,0,3");
    expect(first,"20 ms");API.destroy_instance(first);
    puts("global buffer: defaults, restore, shared edits, BPM and exact-grid capture pass");
}
