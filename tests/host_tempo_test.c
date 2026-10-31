/* Real plugin consumers must agree on the optional host tempo contract. */
#define main host_tempo_fixture_main
#include "chord_player_test.c"
#undef main

int main(void){
    Inst *instance=fixture();
    const float invalid_tempos[]={NAN,INFINITY,-INFINITY,0.0f,-1.0f,19.0f,1000.0f};
    char display[32];
    g_bus.chord_timescale=3; /* quarter-note confirmation window */
    for(unsigned index=0;index<sizeof(invalid_tempos)/sizeof(invalid_tempos[0]);index++){
        tempo=invalid_tempos[index];
        assert(hb_host_bpm()==120.0);
        assert(hb_ms_to_beats(500)==1.0);
        assert(hb_timescale_frames(48000)==24000);
        double previous_motion=instance->motion_beat;
        API.tick(instance,128,48000,output,lengths,64);
        assert(isfinite(instance->motion_beat)&&instance->motion_beat>previous_motion);
        assert(isfinite(instance->player.release_beats));
        API.get_param(instance,"hb_tempo",display,sizeof(display));
        assert(!strcmp(display,"120"));
    }
    const float valid_tempos[]={20.0f,90.0f,119.49f,120.19f,137.25f,999.0f};
    for(unsigned index=0;index<sizeof(valid_tempos)/sizeof(valid_tempos[0]);index++){
        tempo=valid_tempos[index];
        assert(hb_host_bpm()==(double)tempo); /* no smoothing of either host */
        assert(fabs(hb_ms_to_beats(60000)-(double)tempo)<1e-9);
    }
    host.get_bpm=NULL;
    assert(hb_host_bpm()==120.0);
    g_host=NULL;
    assert(hb_host_bpm()==120.0);
    g_host=&host;host.get_bpm=test_bpm;
    API.destroy_instance(instance);
    puts("Host tempo: absent callbacks, invalid reads, fractional BPM and real timing consumers pass");
    return 0;
}
