/* Exercise the production predictor with a deterministic shared transport. */
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../modules/harmonybus/dsp/harmonybus.c"

static double test_beat;
static double transport_beat(void){return test_beat;}
static float transport_bpm(void){return 120.0f;}
static host_api_v1_t test_host={.get_beat_position=transport_beat,.get_bpm=transport_bpm};
static hb_global_shared_t test_globals;

static void reset_fixture(void){
    memset(&g_bus,0,sizeof(g_bus));
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
    for(int conductor=0;conductor<4;conductor++)hb_next_update_playhead(64,48000);
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
    puts("loop_learning_test: valid harmony latch, stable loops, shared timing, transient filtering and changes pass");
    return 0;
}
