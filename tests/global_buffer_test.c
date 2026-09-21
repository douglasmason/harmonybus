/* Shared quantization grid, independent follower capture and lookahead. */
#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
static void expect(Inst *instance,const char *key,const char *expected){
    char value[128];API.get_param(instance,key,value,sizeof(value));assert(!strcmp(value,expected));
}
int main(void){
    Inst *first=fixture(),*second=API.create_instance("",NULL);
    API.set_param(second,"role","Follower");
    API.set_param(first,"boundary_buffer_ms","350 ms");
    API.set_param(second,"boundary_buffer_ms","20 ms");
    expect(first,"boundary_buffer_ms","350 ms");expect(second,"boundary_buffer_ms","20 ms");
    API.set_param(first,"quant_timing","1/4");
    expect(first,"quant_timing","1/4");expect(second,"quant_timing","1/4");
    API.set_param(first,"next_lookahead","3/4");
    expect(second,"next_lookahead","Off");expect(first,"boundary_buffer_ms","0 ms");
    expect(second,"boundary_buffer_ms","20 ms");
    API.set_param(first,"next_anti_buffer_ms","0 ms");expect(first,"boundary_buffer_ms","350 ms");
    g_bus.next_model_locked=1;g_bus.next_model_count=2;g_bus.clip_loop_end=8;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=chord(2,0,0)};
    g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);
    position=1.5;assert(hb_render_harmony(first).root_pc==2);assert(hb_render_harmony(second).root_pc==0);
    assert(bus_read().root_pc==0); /* private track choice never shifts the conductor bus */
    char saved[16384];API.get_param(first,"state",saved,sizeof(saved));
    API.destroy_instance(first);API.destroy_instance(second);
    first=API.create_instance("",NULL);API.set_param(first,"state",saved);
    expect(first,"next_lookahead","3/4");expect(first,"boundary_buffer_ms","350 ms");
    API.destroy_instance(first);
    puts("scope: global grids, independent capture/lookahead, private harmony and state round-trip pass");
}
