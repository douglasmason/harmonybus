#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
static void event(Inst *instance,int pitch,int on){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)},output[256][3];int lengths[256];
    assert(API.process_midi(instance,message,3,output,lengths,256)==0);
    g_conductor_block_ready=0;
    assert(API.tick(instance,24000,48000,output,lengths,256)==0);
}
int main(void){
    Inst *conductor=fixture();API.set_param(conductor,"role","Conductor");
    hb_set_shared_follower_scale(1);
    const int roots[]={10,10,3,8},qualities[]={1,6,1,1};
    const int scales[]={6,13,3,8};
    char parameter[128],recorded[4096],payload[4096],opening[4096];
    unsigned long long captured=0;
    for(int test=0;test<4;test++){
        snprintf(parameter,sizeof(parameter),"%d,%d,0,1,0,1,12",roots[test],qualities[test]);
        API.set_param(conductor,"hb_movy_input_chord",parameter);
        assert(hb_selection_word(conductor->selection_pending));
        event(conductor,60+roots[test],1);
        hb_harmony_t harmony=hb_render_harmony(conductor);
assert(harmony.valid&&harmony.root_pc==roots[test]&&harmony.intent_kind==7);
        assert((harmony.intent_scale&hb_harmony_chord_mask(harmony))==hb_harmony_chord_mask(harmony));
        assert(harmony.intent_scale==hb_explicit_scale_mask(0,scales[test]));
        API.get_param(conductor,"hb_record_action",recorded,sizeof(recorded));
        assert(!strncmp(recorded,"ra4,",4));
        if(!test){strcpy(payload,recorded+4);captured=conductor->player.keys[0].semantic_mask;}
        event(conductor,60+roots[test],0);
    }
    /* Frozen selections replay through the same conductor path after settings change. */
    hb_set_shared_follower_scale(2);
    API.set_param(conductor,"hb_movy_actions",payload);
    assert(conductor->recorded_action_valid[70]);
    conductor->movy_playback=1;event(conductor,70,1);
    hb_harmony_t replay=hb_render_harmony(conductor);
    assert(hb_harmony_chord_mask(replay)==captured);
    assert(replay.intent_scale==hb_explicit_scale_mask(0,6));
    event(conductor,70,0);conductor->movy_playback=0;
    /* The opening preview uses the same snapshot and includes the local scale. */
    int used=snprintf(opening,sizeof(opening),"1;70,0");
    for(int index=0;index<=HB_MOTION_LANES;index++)used+=snprintf(opening+used,sizeof(opening)-used,",%llx",conductor->recorded_actions[70][index]);
    API.set_param(conductor,"hb_opening_preview",opening);
    hb_harmony_t preview=hb_opening_harmony();
    assert(preview.valid&&preview.root_pc==10&&preview.intent_scale==replay.intent_scale);
    /* An invalid command cannot reuse the last prepared selection. */
    API.set_param(conductor,"hb_movy_input_chord","12,1,0,1,0,1,12");assert(!conductor->selection_pending);
    API.destroy_instance(conductor);
    puts("Chord selection: silent input, parent accommodation, immutable recording and opening preview pass");
}
