/* Exercise the real note owner, emitted MIDI, and saved selection descriptor. */
#define main ignored_range_main
#include "chord_player_test.c"
#undef main

static int highest(const hb_chord_player *player){
    for(int note=127;note>=0;note--)for(int channel=0;channel<16;channel++)if(player->sounding[channel][note])return note;
    return -1;
}
static void top_range(int conductor,int arp){
    Inst *instance=fixture();
    API.set_param(instance,"chord_mode",conductor?"Off":"Conductor Chord");
    if(conductor){API.set_param(instance,"role","Conductor");API.set_param(instance,"render_channel","3");}
    API.set_param(instance,"chord_form","Triad");API.set_param(instance,"chord_inversion","Root");
    API.set_param(instance,"arp_playback",arp?"Repeat Arp":"Together");
    API.set_param(instance,"chord_register","0");
    if(conductor)API.set_param(instance,"hb_movy_input_chord","0,1,2,1,0,1,12");
    midi(instance,1,60);advance(instance,1,64);
    const int tops[]={72,76,79,84,79,76,72,67,64,60};
    for(int index=0;index<10;index++){
        int actions=instance->action_count,recorded=recorded_count,step=instance->player.order_state.step;
        double boundary=instance->player.next_beat,gate=instance->player.gate_beat;
        char parameter[80];
        if(conductor)snprintf(parameter,sizeof(parameter),"0,1,2,1,0,1,12,0,0,%d",tops[index]+8);
        else snprintf(parameter,sizeof(parameter),"Top %d",tops[index]);
        API.set_param(instance,conductor?"hb_chord_range":"chord_register",parameter);
        advance(instance,1,64);
        if(!arp)assert(highest(&instance->player)==tops[index]);
        assert(instance->player.sounding_count==(arp?1:3));
        assert(instance->action_count==actions&&recorded_count==recorded);
        if(arp){assert(instance->player.next_beat==boundary&&instance->player.gate_beat==gate);assert(instance->player.order_state.step==step);}
        int emitted=render_count;API.set_param(instance,conductor?"hb_chord_range":"chord_register",parameter);advance(instance,1,64);assert(render_count==emitted);
    }
    midi(instance,0,60);advance(instance,1,64);assert(!instance->player.sounding_count);
    API.destroy_instance(instance);
}
static void mapped_top_range(void){
    for(int selection=0;selection<2;selection++){
        Inst *instance=fixture();API.set_param(instance,"role","Conductor");
        API.set_param(instance,"render_channel","3");API.set_param(instance,"chord_mode","Scale Degree");
        API.set_param(instance,"chord_form","Triad");API.set_param(instance,"chord_inversion","Root");
        g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=2,
            .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(2,1)};
        g_key_conductor_travel=1;
        if(selection)API.set_param(instance,"hb_movy_input_chord","0,1,2,1,0,1,12,0,0,77");
        else API.set_param(instance,"chord_register","Top 69");
        midi(instance,1,60);advance(instance,1,64);
        assert(highest(&instance->player)==69);
        const int tops[]={74,78,81,78,74,69};
        for(int i=0;i<6;i++){
            char parameter[80];
            if(selection)snprintf(parameter,sizeof(parameter),"0,1,2,1,0,1,12,0,0,%d",tops[i]+8);
            else snprintf(parameter,sizeof(parameter),"Top %d",tops[i]);
            API.set_param(instance,selection?"hb_chord_range":"chord_register",parameter);
            advance(instance,1,64);assert(highest(&instance->player)==tops[i]);
            assert(instance->player.sounding_count==3);
        }
        hb_harmony_t harmony=hb_selected_harmony(instance);
        if(selection){harmony=hb_key_harmony(instance,harmony);assert(harmony.register_valid==2&&harmony.register_octaves==1069);}
        midi(instance,0,60);advance(instance,1,64);assert(!instance->player.sounding_count);
        API.destroy_instance(instance);
    }
}
static void root_below_voicing(void){
    /* Compare upper voices against Close across roots, forms, anchors and edges. */
    for(int root=0;root<12;root++)for(int form=1;form<HB_CP_FORMS;form++)
    for(int inversion=0;inversion<9;inversion++)for(int input=0;input<128;input++){
        hb_cp_config config={0};config.mode=2;config.size=form;config.inversion=inversion;
        unsigned chord=(1u<<root)|(1u<<mod12(root+4))|(1u<<mod12(root+7))|(1u<<mod12(root+11));
        int plain[HB_CP_VOICES],rooted[HB_CP_VOICES];
        int count=hb_cp_voice(config,input,root,chord,hb_explicit_scale_mask(root,1),plain);
        config.voicing=4;int actual=hb_cp_voice(config,input,root,chord,hb_explicit_scale_mask(root,1),rooted);
        int bass=count?plain[0]-mod12(plain[0]-root):-1;
        int extra=count&&count<HB_CP_VOICES&&mod12(plain[0])!=root&&bass>=0;
        assert(actual==count+extra);
        if(extra)assert(rooted[0]==bass);
        assert(!memcmp(rooted+extra,plain,(size_t)count*sizeof(int)));
    }
    Inst *instance=fixture();API.set_param(instance,"role","Conductor");
    API.set_param(instance,"render_channel","3");API.set_param(instance,"chord_form","Triad");
    API.set_param(instance,"chord_voicing","Root Below");
    API.set_param(instance,"hb_movy_input_chord","0,1,2,1,0,1,12,0,0,80");
    midi(instance,1,60);advance(instance,1,64);assert(highest(&instance->player)==72&&instance->player.sounding_count==4);
    API.set_param(instance,"hb_chord_range","0,1,2,1,0,1,12,0,0,75");advance(instance,1,64);
    assert(highest(&instance->player)==67&&instance->player.sounding_count==3);
    midi(instance,0,60);advance(instance,1,64);assert(!instance->player.sounding_count);
    char state[65536],value[64];API.get_param(instance,"state",state,sizeof(state));
    API.set_param(instance,"chord_voicing","Close");API.set_param(instance,"state",state);
    API.get_param(instance,"chord_voicing",value,sizeof(value));assert(!strcmp(value,"Root Below"));
    API.set_param(instance,"follower_default_chord_voicing","Root Below");
    API.get_param(instance,"follower_default_chord_voicing",value,sizeof(value));assert(!strcmp(value,"Root Below"));
    API.destroy_instance(instance);
}
int main(void){
    for(int conductor=0;conductor<2;conductor++)for(int arp=0;arp<2;arp++)top_range(conductor,arp);
    mapped_top_range();root_below_voicing();
    Inst *instance=fixture();API.set_param(instance,"role","Conductor");
    for(int intent=0;intent<2;intent++)for(int top=0;top<128;top++){
        int fields[]={0,1,2,1,0,1,12,intent?3:0,4},note=0;
        unsigned long long base=hb_selection_resolve(instance,fields,&note),word=hb_selection_with_register(base,&note,top+8);
        assert(hb_selection_word(word)&&hb_selection_top(word)==top&&note>=60&&note<72);
        assert((hb_selection_base_word(word)&~HB_SELECTION_REGISTER)==base);
        hb_harmony_t harmony=hb_selection_decode(word,note);
        assert(harmony.register_valid==2&&harmony.register_octaves==1000+top);
        assert(!hb_selection_word(word&~HB_SELECTION_REGISTER));
    }
    API.set_param(instance,"chord_register","Top 76");char state[65536],value[64];
    API.get_param(instance,"state",state,sizeof(state));assert(strstr(state,";cr1,1076"));
    API.set_param(instance,"chord_register","0");API.set_param(instance,"state",state);
    API.get_param(instance,"chord_register",value,sizeof(value));assert(!strcmp(value,"Top 76"));
    hb_cp_config config={0};config.top_note=73;int notes[12];
    assert(hb_cp_voice_semantic(config,60,0,0,0,notes,0)==1&&notes[0]==60);
    API.destroy_instance(instance);
    puts("Top range: successive inversions, immediate audible retuning, original releases, arp clock, no extra attacks, versioned words and persistence pass");
}
