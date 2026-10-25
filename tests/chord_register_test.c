#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
static hb_cp_key *selected(Inst *instance){for(int i=0;i<HB_CP_KEYS;i++)if(instance->player.keys[i].used&&instance->player.keys[i].selection)return &instance->player.keys[i];return 0;}
static int select_chord(Inst *instance,const char *payload){
    char text[64];API.set_param(instance,"hb_movy_input_chord",payload);API.get_param(instance,"hb_chord_note",text,sizeof(text));int pitch=atoi(text);
    uint8_t on[]={0x90,(uint8_t)pitch,100};assert(hb_selection_input(instance,on,3));return pitch;
}
int main(void){
    Inst *follower=fixture(),*conductor=API.create_instance("",0);API.set_param(conductor,"role","Conductor");API.set_param(conductor,"render_channel","3");
    API.set_param(conductor,"chord_form","Triad");API.set_param(conductor,"chord_inversion","Root");
    int pitch=select_chord(conductor,"0,1,2,1,0,1,12");hb_cp_key *key=selected(conductor);assert(key&&pitch==60);int base=key->notes[0];
    API.set_param(conductor,"chord_register","1");assert(key->notes[0]==base+12&&key->source==60);
    hb_harmony_t harmony=hb_live_selection(conductor);assert(harmony.register_valid&&harmony.register_octaves==1);
    assert(hb_chord_register_for(follower,harmony)==1);
    API.set_param(follower,"chord_register","-1");assert(hb_chord_register_for(follower,harmony)==-1);
    API.set_param(follower,"chord_register","Conductor");assert(hb_chord_register_for(follower,harmony)==1);
    /* Per-chord range edits preserve the original MIDI owner and add no attack. */
    int count=conductor->action_count;
    API.set_param(conductor,"hb_chord_range","0,1,2,1,0,1,12,0,0,3");
    assert(key->notes[0]==base-12&&key->source==60&&key->selection_pitch==48&&conductor->action_count==count);
    assert(hb_live_selection(conductor).register_octaves==-1);
    API.set_param(follower,"chord_register","2");assert(hb_chord_register_for(follower,hb_live_selection(conductor))==-1);
    API.set_param(conductor,"chord_register","2");assert(key->notes[0]==base-12);
    API.set_param(conductor,"hb_chord_range","0,1,2,1,0,1,12");assert(key->notes[0]==base+24);
    uint8_t off[]={0x80,(uint8_t)pitch,0};assert(hb_selection_input(conductor,off,3));assert(!selected(conductor));
    /* Explicit zero is an override, distinct from inheriting the default. */
    pitch=select_chord(conductor,"7,1,2,1,0,1,12,0,0,4");key=selected(conductor);assert(pitch==67&&key->selection_word&HB_SELECTION_REGISTER);
    assert(hb_live_selection(conductor).register_octaves==0);
    unsigned long long word=key->selection_word;assert(hb_selection_word(word));
    conductor->recorded_actions[pitch][0]=word;conductor->recorded_action_valid[pitch]=1;assert(hb_selection_actions(conductor->recorded_actions[pitch]));
    /* Pending/cycle entries are edited in place without adding queue items. */
    API.set_param(conductor,"hb_chord_queue","7,1,2,1,0,1,12");int queued=g_chord_queue.count;
    API.set_param(conductor,"hb_chord_range","7,1,2,1,0,1,12,0,0,6");assert(g_chord_queue.count==queued&&g_chord_queue.pending[0].note==91);
    char state[65536];API.get_param(conductor,"state",state,sizeof(state));assert(strstr(state,";cr1,2"));
    API.set_param(conductor,"chord_register","0");API.set_param(conductor,"state",state);assert(conductor->chord_register==2);
    /* Voice register changes preserve harmonic pitch classes and raw melody. */
    hb_cp_config config={0};int notes[12];config.register_octaves=2;assert(hb_cp_voice_semantic(config,60,0,0,0,notes,0)==1&&notes[0]==60);
    API.destroy_instance(conductor);API.destroy_instance(follower);puts("Chord range: inherited conductor register, track/choice overrides, live owner-safe edits, pending updates and persistence pass");
}
