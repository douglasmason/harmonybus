/* Exercise the production predictor with a deterministic shared transport. */
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../modules/harmonybus/dsp/harmonybus.c"

static double test_beat;
static int test_transport_active=1;
static int test_clock_status=2; /* Schwung ABI: 0 unavailable, 1 stopped, 2 running. */
static int transport_status(void){return test_clock_status;}
static double transport_beat(void){return test_transport_active?test_beat:-1.0;}
static float transport_bpm(void){return 120.0f;}
static host_api_v1_t test_host={.get_beat_position=transport_beat,.get_bpm=transport_bpm,.get_clock_status=transport_status};
static hb_global_shared_t test_globals;

static void reset_fixture(void){
    memset(&g_bus,0,sizeof(g_bus));
    memset(g_pool,0,sizeof(g_pool));
    for(int conductor=0;conductor<4;conductor++)g_pool[conductor].used=1;
    test_clock_status=2;
    test_transport_active=1;
    g_host=&test_host;
    g_global_shared=&test_globals;
    g_bus.clip_loop_end=4.0;
    g_bus.inference_window_ms=25;
    test_beat=0.0;
    hb_next_reset_knowledge();
}
static void advance(double beat){
    test_beat=beat;
    /* Four conductor callbacks at the same transport position. */
    uint8_t output[MIDI_FX_MAX_OUT_MSGS][3];
    int lengths[MIDI_FX_MAX_OUT_MSGS];
    for(int conductor=0;conductor<4;conductor++)
        API.tick(&g_pool[conductor],64,48000,output,lengths,MIDI_FX_MAX_OUT_MSGS);
}
static hb_harmony_t chord(int root){
    uint8_t notes[3]={(uint8_t)(60+root),(uint8_t)(64+root),(uint8_t)(67+root)};
    return hb_infer_harmony(notes,3);
}
int main(void){
    hb_harmony_t tonic=chord(0), dominant=chord(7), other=chord(2);
    hb_harmony_t unknown={0}, unresolved={.valid=1,.chord_index=-1};
    assert(tonic.valid&&dominant.valid&&other.valid);
    reset_fixture();
    hb_commit_observed_harmony(unknown);
    hb_commit_observed_harmony(unresolved);
    advance(0.1);
    assert(!g_bus.observed_harmony.valid&&!g_bus.next_model_locked);

    reset_fixture();
    hb_commit_observed_harmony(tonic);
    advance(0.06);
    hb_commit_observed_harmony(unknown);
    hb_commit_observed_harmony(unresolved);
    assert(hb_harmony_equal_effective(g_bus.observed_harmony,tonic));
    advance(1.0);
    assert(!g_bus.next_model_locked);
    assert(g_bus.next_learning_progress_beats<1.0);
    advance(2.0);
    hb_commit_observed_harmony(dominant);
    advance(2.06);
    advance(3.0);
    advance(4.06);
    assert(g_bus.next_model_locked&&g_bus.next_model_count==2);
    hb_loop_harmony_event_t original[2];
    memcpy(original,g_bus.next_model,sizeof(original));

    /* Repeated passes retain the exact model, including transition phases. */
    for(int loop=1;loop<4;loop++){
        advance(loop*4.0+0.07);
        hb_commit_observed_harmony(tonic);
        advance(loop*4.0+0.13);
        advance(loop*4.0+2.0);
        hb_commit_observed_harmony(dominant);
        advance(loop*4.0+2.06);
        hb_commit_observed_harmony(unresolved);
        advance(loop*4.0+3.0);
        assert(g_bus.next_model_locked);
        assert(memcmp(original,g_bus.next_model,sizeof(original))==0);
    }
    char position[64];
    API.get_param(&g_pool[0],"next_position",position,sizeof(position));
    assert(strcmp(position,"0.75 / 1.00 Bars")==0);
    char harmony_before[64], harmony_after[64];
    API.get_param(&g_pool[0],"next_harmony",harmony_before,sizeof(harmony_before));
    advance(16.0);
    API.get_param(&g_pool[0],"next_harmony",harmony_after,sizeof(harmony_after));
    assert(strcmp(harmony_before,harmony_after)!=0);
    API.get_param(&g_pool[0],"next_position",position,sizeof(position));
    assert(strcmp(position,"0.00 / 1.00 Bars")==0);
    /* Stop freezes displayed position and clears held notes exactly at stop. */
    g_pool[0].held_count[60]=1;
    test_clock_status=1;
    test_transport_active=0;
    advance(16.5);
    assert(g_pool[0].held_count[60]==0);
    API.get_param(&g_pool[0],"next_position",position,sizeof(position));
    assert(strcmp(position,"0.00 / 1.00 Bars")==0);
    test_clock_status=2;
    test_transport_active=1;
    advance(16.75);
    API.get_param(&g_pool[0],"next_position",position,sizeof(position));
    assert(strcmp(position,"0.19 / 1.00 Bars")==0);
    /* Movy private-chain status can be stale while shared transport runs. */
    test_clock_status=1;
    advance(17.0);
    API.get_param(&g_pool[0],"next_position",position,sizeof(position));
    assert(strcmp(position,"0.25 / 1.00 Bars")==0);
    assert(g_pool[0].last_transport_playing);
    test_clock_status=0;
    advance(17.25);
    assert(g_pool[0].last_transport_playing);
    /* Hosts without beat-position support use the actual ABI status values. */
    test_host.get_beat_position=NULL;
    test_clock_status=2;
    assert(hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING);
    test_clock_status=1;
    assert(hb_clock_status()==MOVE_CLOCK_STATUS_STOPPED);
    test_clock_status=0;
    assert(hb_clock_status()==MOVE_CLOCK_STATUS_UNAVAILABLE);
    test_host.get_beat_position=transport_beat;
    test_clock_status=2;
    /* Silence and loop wrap retain the last valid harmony, with lookahead on. */
    g_bus.next_lookahead=3;
    hb_next_apply_effective(3.0);
    assert(hb_harmony_equal_effective(bus_read(),dominant));
    int wrapped=hb_next_model_event_for_phase(3.9,1);
    assert(wrapped>=0&&hb_harmony_equal_effective(g_bus.next_model[wrapped].harmony,tonic));

    /* A brief valid intermediate interpretation must not erase the model. */
    advance(15.1);
    hb_commit_observed_harmony(other);
    advance(15.12);
    hb_commit_observed_harmony(dominant);
    advance(15.18);
    assert(g_bus.next_model_locked);
    hb_commit_observed_harmony(other);
    advance(15.24);
    assert(!g_bus.next_model_locked&&g_bus.next_learning_count==1);
    assert(hb_harmony_equal_effective(g_bus.next_learning[0].harmony,other));

    /* Explicit clip changes invalidate knowledge; duplicate callbacks do not. */
    hb_next_promote_learning();
    assert(g_bus.next_model_locked);
    g_bus.cache_rev++;
    advance(15.3);
    assert(!g_bus.next_model_locked);
    advance(15.36);
    advance(16.0);
    assert(g_bus.next_learning_progress_beats<1.0);
    advance(0.0);
    assert(!g_bus.next_model_locked&&g_bus.next_learning_progress_beats==0.0);
    hb_commit_observed_harmony(tonic);
    advance(0.06);
    hb_next_promote_learning();
    g_bus.clip_loop_end=8.0;
    advance(0.1);
    assert(!g_bus.next_model_locked);
    advance(0.16);
    hb_next_promote_learning();
    g_bus.global_transpose=2;
    advance(0.2);
    assert(!g_bus.next_model_locked);
    puts("loop_learning_test: MIDI FX ticks, position, next harmony, stop/resume, private-chain status and learning pass");
    return 0;
}
