#define main stock_field_fixture_main
#include "chord_player_test.c"
#undef main
int main(void){
    Inst *a=fixture(),*b=API.create_instance("",NULL);
    API.set_param(a,"hb_movy_block","1,128,48000,0");double first=g_hb_field.position[0];
    API.set_param(b,"hb_movy_block","1,128,48000,0");assert(g_hb_field.position[0]==first);
    advance(a,3,64);advance(b,3,64);assert(g_hb_field.position[0]==first);
    /* Bypass both chains: their heartbeat still drives one shared clock. */
    API.set_param(b,"hb_movy_block","2,128,48000,1");double second=g_hb_field.position[0];assert(second>first);
    API.set_param(a,"hb_movy_block","2,128,48000,1");assert(g_hb_field.position[0]==second);
    API.destroy_instance(a);API.set_param(b,"hb_movy_block","3,128,48000,0");assert(g_hb_field.position[0]>second);
    API.set_param(b,"field_lfo1:shape","21");API.set_param(b,"field_lfo1:enabled","1");
    API.set_param(b,"field_lfo1:target","synth");API.set_param(b,"field_lfo1:target_param","gain");API.set_param(b,"field_lfo1:depth","-0.2345");
    API.set_param(b,"field_lfo2:shape","6");API.set_param(b,"field_lfo2:target","fx2");API.set_param(b,"field_lfo2:target_param","mix");
    char state[131072];assert(API.get_param(b,"state",state,sizeof(state))>0);
    API.set_param(b,"field_lfo1:shape","0");API.set_param(b,"state",state);
    assert(b->field_mod[0].shape==21&&b->field_mod[0].enabled&&b->field_mod[0].depth==-2345);
    assert(b->field_mod[1].shape==6&&!strcmp(b->field_mod[1].target,"fx2"));
    API.set_param(b,"state","hb16,2,0,0,25,2,0,0,0,0,0,0,-1,-1,0,0,0,0,0,-3,60,0,0,1,0");
    assert(!b->field_mod[0].shape&&!b->field_mod[1].shape);
    API.destroy_instance(b);assert(!g_field_managed_clock);
    puts("Field managed clock: one advance, bypass, removal, route persistence and legacy state pass");
}
