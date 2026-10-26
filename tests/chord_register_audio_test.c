/* Verify audible MIDI, not just the edited key's stored note pool. */
#define main ignored_chord_player_main
#include "chord_player_test.c"
#undef main
static void retune(int conductor,int arp){
    Inst *instance=fixture();
    API.set_param(instance,"chord_mode",conductor?"Off":"Conductor Chord");
    if(conductor){API.set_param(instance,"role","Conductor");API.set_param(instance,"render_channel","3");}
    API.set_param(instance,"chord_form","Triad");API.set_param(instance,"chord_inversion","Root");
    API.set_param(instance,"arp_playback",arp?"Repeat Arp":"Together");
    API.set_param(instance,"chord_register","0");
    if(conductor)API.set_param(instance,"hb_movy_input_chord","0,1,2,1,0,1,12");
    midi(instance,1,60);advance(instance,1,64);
    assert(instance->player.sounding_count==(arp?1:3));
    int pitches[12],count=0;
    for(int note=0;note<128;note++)if(instance->player.sounding[0][note])pitches[count++]=note;
    int first=render_count,actions=instance->action_count,recorded_before=recorded_count;
    double boundary=instance->player.next_beat,gate=instance->player.gate_beat;
    int step=instance->player.order_state.step;
    API.set_param(instance,conductor?"hb_chord_range":"chord_register",conductor?"0,1,2,1,0,1,12,0,0,5":"1");
    advance(instance,1,64);
    assert(render_count==first+count*2);
    for(int index=0;index<count;index++){
        assert((rendered[first+index][1]&240)==128);
        assert((rendered[first+count+index][1]&240)==144);
        assert(instance->player.sounding[0][pitches[index]+12]);
        assert(!instance->player.sounding[0][pitches[index]]);
    }
    assert(instance->action_count==actions&&recorded_count==recorded_before);
    if(arp){assert(instance->player.next_beat==boundary&&instance->player.gate_beat==gate);assert(instance->player.order_state.step==step);}
    first=render_count;
    API.set_param(instance,conductor?"hb_chord_range":"chord_register",conductor?"0,1,2,1,0,1,12,0,0,5":"1");
    advance(instance,1,64);assert(render_count==first); /* An unchanged range is inert. */
    midi(instance,0,60);advance(instance,1,64);assert(!instance->player.sounding_count);
    API.destroy_instance(instance);
}
static void inheritance(void){
    Inst *comp=fixture(),*conductor=API.create_instance("",0);
    API.set_param(conductor,"role","Conductor");API.set_param(conductor,"render_channel","Off");
    API.set_param(conductor,"hb_movy_input_chord","0,1,2,1,0,1,12");midi(conductor,1,60);
    API.set_param(comp,"chord_mode","Conductor Chord");API.set_param(comp,"arp_playback","Together");API.set_param(comp,"chord_register","Conductor");
    midi(comp,1,60);advance(comp,1,64);int first=render_count;
    API.set_param(conductor,"chord_register","1");advance(comp,1,64);assert(render_count>first);
    API.set_param(comp,"chord_register","-1");advance(comp,1,64);first=render_count;
    API.set_param(conductor,"chord_register","2");advance(comp,1,64);assert(render_count==first); /* Explicit comp range is independent. */
    midi(comp,0,60);
    if(comp->follower_queue_count)position=comp->follower_queue_target_beat[0]+.01;
    advance(comp,10,64);assert(!comp->player.sounding_count);
    API.destroy_instance(comp);API.destroy_instance(conductor);
}
int main(void){for(int conductor=0;conductor<2;conductor++)for(int arp=0;arp<2;arp++)retune(conductor,arp);inheritance();
    puts("Audible range: next-tick note replacement, conductor/comp routing, Off retrigger policy, active arp gates, inheritance and original-owner release pass");}
