/* Live arp controls preserve raw latched ownership and balance note lifetimes. */
#define main original_chord_player_tests
#include "chord_player_test.c"
#undef main
static void assert_pool(Inst *instance){
    int seen=0;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *owner=&instance->player.keys[index];if(!owner->used)continue;
        assert(owner->source==60||owner->source==64||owner->source==67);
        assert(owner->channel==0&&!owner->held&&owner->count==1);
        assert(owner->notes[0]==owner->source);seen++;
    }
    if(seen!=3)fprintf(stderr,"pool=%d playback=%d latch=%d rate=%d phase=%d queued=%d\n",seen,instance->player.config.playback,instance->player.config.latch,instance->player.config.rate,instance->player.config.phase,instance->follower_queue_count);
    assert(seen==3);
}
int main(void){
    Inst *instance=fixture();
    API.set_param(instance,"render_channel","Off");
    API.set_param(instance,"boundary_buffer_ms","0 ms");
    API.set_param(instance,"travel_map","None");
    API.set_param(instance,"arp_playback","Repeat Arp");
    API.set_param(instance,"arp_hold","Latch Acc. with Off");
    midi(instance,1,60);midi(instance,1,64);midi(instance,1,67);
    midi(instance,0,60);midi(instance,0,64);midi(instance,0,67);
    advance(instance,0,64);advance(instance,0,64);
    assert_pool(instance);
    const char *names[]={"arp_rate","arp_gate","arp_order","arp_phase","arp_hold","arp_playback"};
    const char **options[]={CP_ARP_RATE,CP_ARP_GATE,CP_ARP_ORDER,CP_ARP_PHASE,CP_ARP_HOLD,CP_ARP_PLAYBACK};
    const int counts[]={18,4,6,3,6,3};
    int balance[128]={0};
    for(int pitch=0;pitch<128;pitch++)balance[pitch]=instance->player.sounding[0][pitch]?1:0;
    for(int control=0;control<6;control++)for(int selected=0;selected<counts[control];selected++){
        API.set_param(instance,names[control],options[control][selected]);assert_pool(instance);
        for(int tick=0;tick<6;tick++){
            int emitted=advance(instance,25,1);
            for(int event=0;event<emitted;event++){
                int pitch=output[event][1];
                if((output[event][0]&0xf0)==0x90){assert(!balance[pitch]);balance[pitch]++;}
                if((output[event][0]&0xf0)==0x80){assert(balance[pitch]);balance[pitch]--;}
            }
            assert_pool(instance);
        }
    }
    API.set_param(instance,"arp_note_phase","1");assert_pool(instance);
    API.set_param(instance,"strum_spread","150");assert_pool(instance);
    API.set_param(instance,"arp_clear_harmony","On");assert_pool(instance);
    API.set_param(instance,"arp_clear_harmony","Off");assert_pool(instance);
    API.set_param(instance,"arp_clear","Clear");
    for(int tick=0;tick<16;tick++){
        int emitted=advance(instance,1,1);
        for(int event=0;event<emitted;event++){
            assert((output[event][0]&0xf0)==0x80);
            assert(balance[output[event][1]]);balance[output[event][1]]--;
        }
    }
    for(int pitch=0;pitch<128;pitch++)assert(!balance[pitch]);
    for(int index=0;index<HB_CP_KEYS;index++)assert(!instance->player.keys[index].used);
    assert(!instance->player.sounding_count);
    instance->player.config.rate=4;instance->player.beat=10;
    instance->player.next_beat=12;instance->player.gate_beat=11;
    API.set_param(instance,"arp_rate",CP_ARP_RATE[2]);
    assert(fabs(instance->player.next_beat-10.5)<1e-9);
    assert(fabs(instance->player.gate_beat-10.25)<1e-9);
    API.destroy_instance(instance);
    puts("arp edits: every option retains raw notes; Off/re-enable and clear balance voices under one-event capacity");
    return 0;
}
