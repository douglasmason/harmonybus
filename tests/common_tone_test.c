/* Exercise the production owners and MIDI diff, including output backpressure. */
#define main ignored_chord_player_main
#include "chord_player_test.c"
#undef main

int main(void){
    const uint8_t suspended[3]={60,65,67};
    for(int preserve=0;preserve<2;preserve++)for(int capacity=1;capacity<=64;capacity+=63){
        Inst *instance=fixture();
        API.set_param(instance,"chord_mode","Conductor Chord");
        API.set_param(instance,"chord_form","Triad");
        API.set_param(instance,"retrigger_held",preserve?"Preserve Common":"On");
        assert(instance->retrigger_held==(preserve?2:1));
        char saved[65536];API.get_param(instance,"state",saved,sizeof(saved));
        Inst *restored=API.create_instance("",NULL);API.set_param(restored,"state",saved);
        assert(restored->retrigger_held==(preserve?2:1));API.destroy_instance(restored);
        midi(instance,1,60);assert(advance(instance,0,64)==3);
        hb_commit_observed_harmony(hb_infer_harmony(suspended,3));
        int offs=0,ons=0;
        for(int tick=0;tick<8;tick++){
            int count=advance(instance,0,capacity);
            for(int event=0;event<count;event++){
                if(output[event][0]==0x80)offs++;else if(output[event][0]==0x90)ons++;
                if(preserve)assert(output[event][1]!=60&&output[event][1]!=67);
            }
        }
        assert(offs==(preserve?1:3)&&ons==(preserve?1:3));
        assert(advance(instance,0,64)==0);
        assert(!instance->player.preserve_revoice);
        midi(instance,0,60);assert(advance(instance,0,64)==3);
        assert(instance->player.sounding_count==0);
        midi(instance,1,60);assert(advance(instance,0,64)==3);
        midi(instance,0,60);assert(advance(instance,0,64)==3);
        assert(advance(instance,0,64)==0);
        API.destroy_instance(instance);
    }
    puts("common-tone continuity: opt-in preservation, legacy retrigger, bounded output and release pass");
    return 0;
}
