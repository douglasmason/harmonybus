#define main original_chord_suite
#include "chord_player_test.c"
#undef main
static void opening_and_cycles(void){
    const int pitches[]={55,60,67};
    for(int start=1;start<=6;start++)for(int phase=0;phase<3;phase++)for(int order=0;order<6;order++){
        hb_chord_player player={0};hb_cp_defaults(&player.config);
        player.config.playback=1;player.config.start=start;player.config.phase=phase;player.config.order=order;
        player.beat=.13;assert(hb_cp_on(&player,60,0,100,pitches,3));
        int count=hb_cp_tick(&player,output,lengths,64);
        if(phase==1){assert(!count);player.beat=.25;count=hb_cp_tick(&player,output,lengths,64);}
        assert(count==1&&output[0][1]==(start>=5?60:(start==1||start==3)?55:67));
        double first=player.beat;unsigned seen=1u<<(output[0][1]==55?0:output[0][1]==60?1:2);
        int cycle=order==2?4:3;
        for(int step=1;step<=cycle;step++){
            player.beat=player.next_beat;hb_cp_tick(&player,output,lengths,64);
            int pitch=player.arp_note;
            if(step==1)assert(pitch!=(start>=5?60:(start==1||start==3)?55:67));
            if(order==5&&step<cycle){unsigned bit=1u<<(pitch==55?0:pitch==60?1:2);assert(!(seen&bit));seen|=bit;}
            if(step==cycle)assert(pitch==(start>=5?60:(start==1||start==3)?55:67));
        }
        assert(first==(phase==1?.25:.13));
    }
}
static void harmony_anchor_and_state(void){
    for(int start=1;start<=6;start++){
        Inst *instance=fixture();API.set_param(instance,"arp_playback","Repeat Arp");
        API.set_param(instance,"arp_start",CP_ARP_START[start]);
        instance->player.config.phase=0;instance->retrigger_held=0;
        const int notes[]={55,60,67};hb_cp_on(&instance->player,60,0,100,notes,3);
        hb_player_tick(instance,output,lengths,64);
        instance->player.beat=instance->player.next_beat;hb_player_tick(instance,output,lengths,64);
        double due=instance->player.next_beat;
        uint8_t harmony_notes[]={62,65,69};hb_commit_observed_harmony(hb_infer_harmony(harmony_notes,3));
        instance->player.beat=due-.01;hb_player_tick(instance,output,lengths,64);
        assert(instance->player.next_beat==due);
        assert(!!instance->player.anchor_pending==(start==3||start==4||start==6));
        instance->player.beat=due;hb_player_tick(instance,output,lengths,64);
        if(start==3||start==4||start==6)assert(instance->player.arp_note==(start==6?60:start==3?55:67));
        char state[8192],value[40];API.get_param(instance,"state",state,sizeof(state));
        API.set_param(instance,"arp_start","Order");API.set_param(instance,"state",state);
        API.get_param(instance,"arp_start",value,sizeof(value));assert(!strcmp(value,CP_ARP_START[start]));
        API.set_param(instance,"arp_clear","Clear");
        for(int index=0;index<HB_CP_KEYS;index++)assert(!instance->player.keys[index].used);
        API.destroy_instance(instance);
    }
}
int main(void){opening_and_cycles();harmony_anchor_and_state();puts("Arp Start: all orders, phases, cycle anchors, shuffle uniqueness, harmony reanchor, state and clear pass");}
