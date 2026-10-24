#define main ignored_chord_player_main
#include "chord_player_test.c"
#undef main
static int functional;
static void select_harmony(Inst *conductor,int root){
    char text[80];snprintf(text,sizeof(text),"%d,1,0,1,0,1,12",root);
    if(functional)snprintf(text,sizeof(text),"0,0,3,3,0,1,12,3,%d",root?4:0);
    API.set_param(conductor,"hb_movy_input_chord",text);
    if(functional)root=conductor->selection_note-60;
    uint8_t message[3]={0x90,(uint8_t)(60+root),100};
    assert(API.process_midi(conductor,message,3,output,lengths,64)==0);
    hb_tick_conductor(conductor,128,48000);
    assert(hb_render_harmony(conductor).root_pc==root);
}
static void run(void){
    Inst *comp=fixture(),*conductor=API.create_instance("",0);
    API.set_param(conductor,"role","Conductor");API.set_param(conductor,"render_channel","Off");
    API.set_param(comp,"chord_mode","Conductor Chord");
    API.set_param(comp,"arp_playback","Repeat Arp");
    select_harmony(conductor,0);
    assert(advance(comp,0,64)==0); /* Selecting harmony never starts the comp actor. */
    midi(comp,1,60);assert(advance(comp,0,64)==1);
    int held=comp->player.arp_note;double next=comp->player.next_beat,gate=comp->player.gate_beat;
    assert(comp->retrigger_held==0);
    position=gate/2;select_harmony(conductor,1);
    assert(advance(comp,0,64)==0); /* Keep the existing gate even though C is not in Db. */
    assert(comp->player.arp_note==held&&comp->player.next_beat==next);
    position=next;int count=advance(comp,0,64),attacks=0;
    for(int index=0;index<count;index++)if(output[index][0]==0x90){
        attacks++;assert(hb_harmony_chord_mask(hb_render_harmony(comp))&(1u<<(output[index][1]%12)));
    }
    assert(attacks==1&&comp->player.next_beat==next+0.25);
    midi(comp,0,60);advance(comp,0,64);assert(!comp->player.sounding_count);
    API.destroy_instance(comp);API.destroy_instance(conductor);
}
static void conductor_output(void){
    Inst *conductor=fixture();API.set_param(conductor,"role","Conductor");
    API.set_param(conductor,"render_channel","3");API.set_param(conductor,"chord_mode","Off");
    API.set_param(conductor,"chord_form","Triad");API.set_param(conductor,"arp_playback","Together");
    API.set_param(conductor,"hb_movy_input_chord","10,1,0,1,0,1,12");
    midi(conductor,1,70);advance(conductor,1,64);
    assert(render_count>=3&&conductor->player.sounding_count==3);
    for(int index=0;index<render_count;index++){
        assert(rendered[index][1]==0x92);
        assert((0x424u&(1u<<(rendered[index][2]%12)))!=0); /* Bb D F */
    }
    assert(recorded_count==0); /* Semantic input is recorded once by Movy, never rendered voices. */
    int before=render_count;midi(conductor,0,70);advance(conductor,1,64);
    assert(render_count==before+3&&!conductor->player.sounding_count);
    API.set_param(conductor,"arp_playback","Repeat Arp");
    API.set_param(conductor,"hb_movy_input_chord","0,1,0,1,0,1,12");midi(conductor,1,60);
    before=render_count;advance(conductor,1,64);assert(render_count==before+1);
    assert(conductor->player.sounding_count==1);
    midi(conductor,0,60);advance(conductor,1000,64);assert(!conductor->player.sounding_count);
    API.destroy_instance(conductor);
}
int main(void){run();functional=1;run();conductor_output();
    puts("Chord Play literal/functional comping: independent attacks, unchanged arp phase, held gate and next-chord pool pass");
}
