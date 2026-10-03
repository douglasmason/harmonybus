#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static unsigned press_mask(Inst *instance,int note){
    render_count=0;midi(instance,1,note);advance(instance,2,64);
    unsigned mask=0;for(int index=0;index<render_count;index++)if((rendered[index][1]&0xf0)==0x90&&rendered[index][3])mask|=1u<<mod12(rendered[index][2]);
    return mask;
}
static void single_setup(Inst *instance){instance->travel_map=7;instance->content_map=1;instance->player.config.mode=0;instance->boundary_buffer_ms=0;}
static void new_key_major(Inst *instance,int target){
    API.set_param(instance,"parallel_scale","Major");API.set_param(instance,"key_center_scale","Use Parallel Scale");API.set_param(instance,"key_center","On");
    assert(press_mask(instance,target)==(1u<<mod12(target)));release(instance,target);
}
static void blues_mode(void){
    const int major[]={60,62,64,65,67,69,71},dorian[]={60,62,63,65,67,69,70};
    for(int library=0;library<2;library++)for(int role=0;role<2;role++){
        Inst *instance=fixture();single_setup(instance);
        API.set_param(instance,"role",role?"Follower":"Conductor");
        API.set_param(instance,"source_channel","1");API.set_param(instance,"render_channel","4");
        if(library)API.set_param(instance,"follower_scale","Blues");
        else {API.set_param(instance,"parallel_scale","Blues");API.set_param(instance,"parallel_mode","Down");}
        assert(hb_key_for(instance).blues);
        assert(hb_key_for(instance).target_mask==hb_explicit_scale_mask(0,3));
        for(int form=0;form<3;form++){
            instance->player.config.mode=1;
            API.set_param(instance,"chord_form",form==0?"Triad":form==1?"Seventh":"Ninth");
            for(int degree=0;degree<7;degree++){
                int root=mod12(dorian[degree]),input=library?dorian[degree]:major[degree];
                unsigned expected=(1u<<root)|(1u<<mod12(root+4))|(1u<<mod12(root+7));
                if(form>=1)expected|=1u<<mod12(root+10);
                if(form>=2)expected|=1u<<mod12(root+2);
                unsigned actual=press_mask(instance,input);
                if(actual!=expected)fprintf(stderr,"blues library%d role%d form%d degree%d: %x != %x\n",library,role,form,degree,actual,expected);
                assert(actual==expected);release(instance,input);
            }
        }
        char state[32768],label[32];API.get_param(instance,"state",state,sizeof(state));
        API.destroy_instance(instance);instance=API.create_instance("",NULL);API.set_param(instance,"state",state);
        API.get_param(instance,library?"follower_scale":"parallel_scale",label,sizeof(label));assert(!strcmp(label,"Blues"));
        API.destroy_instance(instance);
    }
    Inst *instance=fixture();single_setup(instance);
    API.set_param(instance,"parallel_scale","Blues");API.set_param(instance,"parallel_mode","Down");
    hb_harmony_t transformed=hb_key_harmony(instance,hb_render_harmony(instance));
    assert(hb_harmony_chord_mask(transformed)==((1u<<0)|(1u<<4)|(1u<<7)|(1u<<10)));
    API.set_param(instance,"parallel_mode","Up");assert(!hb_key_for(instance).blues);
    API.set_param(instance,"follower_scale","Dorian");
    API.set_param(instance,"parallel_mode","Down");
    assert(g_key_context.source_mask==g_key_context.target_mask);
    transformed=hb_key_harmony(instance,hb_render_harmony(instance));
    assert(hb_harmony_chord_mask(transformed)==((1u<<0)|(1u<<4)|(1u<<7)|(1u<<10)));
    API.set_param(instance,"key_center_scale","Use Parallel Scale");
    API.set_param(instance,"key_center","On");press_mask(instance,62);release(instance,62);
    assert(g_key_context.blues&&g_key_context.target_root==2);
    assert(g_key_context.target_mask==hb_explicit_scale_mask(2,3));
    API.destroy_instance(instance);
}

static void functional_key_change(void){
    Inst *instance=fixture();
    API.set_param(instance,"parallel_scale","Natural Minor");API.set_param(instance,"parallel_mode","Down");
    hb_harmony_t dominant=infer4(55,59,62,65);
    assert(hb_harmony_chord_mask(hb_key_harmony(instance,dominant))==tones(7,4,7,10));
    hb_harmony_t secondary=infer4(57,61,64,67);secondary.intent_kind=2;secondary.intent_target=2;secondary.intent_minor=1;
    hb_harmony_t mapped=hb_key_harmony(instance,secondary);
    assert(mapped.root_pc==9&&mapped.intent_target==2&&mapped.intent_minor);
    assert(hb_harmony_chord_mask(mapped)==tones(9,4,7,10));
    hb_harmony_t leading=infer4(59,62,65,68);leading.intent_kind=3;leading.intent_target=0;
    mapped=hb_key_harmony(instance,leading);
    assert(mapped.root_pc==11&&hb_harmony_chord_mask(mapped)==tones(11,3,6,9));
    API.destroy_instance(instance);
    instance=fixture();API.set_param(instance,"role","Conductor");
    API.set_param(instance,"source_channel","1");API.set_param(instance,"render_channel","4");
    API.set_param(instance,"chord_mode","Scale Degree");API.set_param(instance,"chord_form","Seventh");
    API.set_param(instance,"parallel_scale","Natural Minor");API.set_param(instance,"parallel_mode","Down");
    assert(press_mask(instance,67)==tones(7,4,7,10));release(instance,67);
    API.destroy_instance(instance);
}

static void unchanged_key_voicings(void){
    /* Arming or landing on the same key must preserve complete voicings,
       including octave placement, rather than running another harmonization. */
    const char *forms[]={"Triad","Seventh","Ninth"};
    for(int minor=0;minor<2;minor++)for(int mode=0;mode<3;mode++)
    for(int form=0;form<3;form++)for(int note=59;note<=72;note++){
        int expected[128]={0};
        for(int state=0;state<3;state++){
            Inst *instance=fixture();single_setup(instance);
            API.set_param(instance,"role","Conductor");
            API.set_param(instance,"follower_scale",minor?"Natural Minor":"Major");
            API.set_param(instance,"chord_form",forms[form]);
            instance->player.config.mode=mode;
            instance->movy_playback=1;
            if(state==1)API.set_param(instance,"key_center","On");
            if(state==2)g_key_context=hb_key_baseline(instance);
            render_count=0;midi(instance,1,note);advance(instance,2,64);
            int actual[128]={0};
            for(int event=0;event<render_count;event++)
                if((rendered[event][1]&0xf0)==0x90&&rendered[event][3])actual[rendered[event][2]]++;
            if(!state)memcpy(expected,actual,sizeof(expected));
            else assert(!memcmp(expected,actual,sizeof(expected)));
            release(instance,note);API.destroy_instance(instance);
        }
    }
}
int main(void){unchanged_key_voicings();functional_key_change();blues_mode();
    Inst *instance=fixture();single_setup(instance);
    new_key_major(instance,62);
    assert(g_key_context.target_root==2&&!g_key_armed);
    const int inputs[]={60,69,62,67},expected[]={62,71,64,69};
    for(int index=0;index<4;index++){assert(press_mask(instance,inputs[index])==(1u<<mod12(expected[index])));release(instance,inputs[index]);}
    /* Existing recorded degrees take the same path, unchanged. */
    instance->movy_playback=1;assert(press_mask(instance,64)==(1u<<6));release(instance,64);instance->movy_playback=0;
    Inst *other=API.create_instance("",NULL);API.set_param(other,"role","Follower");API.set_param(other,"render_channel","5");single_setup(other);
    assert(press_mask(other,60)==(1u<<2));release(other,60);
    API.set_param(instance,"parallel_scale","Natural Minor");API.set_param(instance,"parallel_mode","Down");
    {unsigned actual=press_mask(other,64);assert(actual==(1u<<5));}release(other,64);
    API.set_param(instance,"parallel_mode","Up");assert(press_mask(other,64)==(1u<<6));release(other,64);
    /* Conductor output changes too; classifier remains in source-key space. */
    API.set_param(other,"role","Conductor");single_setup(other);API.set_param(other,"source_channel","1");API.set_param(other,"render_channel","5");{unsigned actual=press_mask(other,60);assert(actual==(1u<<2));}release(other,60);
    API.destroy_instance(other);API.destroy_instance(instance);
    instance=fixture();single_setup(instance);instance->boundary_buffer_ms=100;
    API.set_param(instance,"key_center_scale","Use Parallel Scale");API.set_param(instance,"parallel_scale","Major");API.set_param(instance,"key_center","On");
    midi(instance,1,62);assert(g_key_context.target_root==2);
    render_count=0;advance(instance,150,64);
    for(int index=0;index<render_count;index++)if((rendered[index][1]&0xf0)==0x90&&rendered[index][3])assert(rendered[index][2]==62);
    release(instance,62);API.destroy_instance(instance);
    /* Auto-chord retains the degree progression and new-key chord qualities. */
    instance=fixture();single_setup(instance);new_key_major(instance,62);instance->player.config.mode=1;
    API.set_param(instance,"chord_form","Triad");
    const unsigned chords[]={ (1u<<2)|(1u<<6)|(1u<<9),(1u<<11)|(1u<<2)|(1u<<6),(1u<<4)|(1u<<7)|(1u<<11),(1u<<9)|(1u<<1)|(1u<<4)};
    for(int index=0;index<4;index++){unsigned actual=press_mask(instance,inputs[index]);if(actual!=chords[index])fprintf(stderr,"progression %d: %x != %x\n",index,actual,chords[index]);assert(actual==chords[index]);release(instance,inputs[index]);}
    API.destroy_instance(instance);
    /* The motif's anchor lands before the shared change is committed. */
    instance=fixture();single_setup(instance);API.set_param(instance,"approach_mode_active","1");API.set_param(instance,"approach_bank_1","Stock: ii-V-Target");API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,50");API.set_param(instance,"key_center","On");
    for(int step=0;step<3;step++){press_mask(instance,62);assert(g_key_armed==(step<2));release(instance,62);}
    assert(g_key_context.target_root==2);API.destroy_instance(instance);
    /* Closest uses the NEW chord, not a transposed old closest result. */
    instance=fixture();single_setup(instance);new_key_major(instance,62);instance->travel_map=1;instance->content_map=0;
    assert(press_mask(instance,60)==(1u<<1));release(instance,60); /* Dmaj7's C#, nearest C. */
    instance->travel_map=0;assert(press_mask(instance,60)==(1u<<2));release(instance,60);
    API.destroy_instance(instance);
    /* All three target-scale policies and their persistence. */
    for(int mode=0;mode<3;mode++){
        instance=fixture();single_setup(instance);
        API.set_param(instance,"key_center_scale",mode==0?"Simplified Major/Minor":mode==1?"Mode from Parent":"Use Parallel Scale");
        API.set_param(instance,"parallel_scale","Major");API.set_param(instance,"key_center","On");press_mask(instance,64);release(instance,64);
        unsigned expected_scale=mode==0?hb_explicit_scale_mask(4,2):mode==1?hb_explicit_scale_mask(0,1):hb_explicit_scale_mask(4,1);
        assert(g_key_context.target_root==4&&g_key_context.target_mask==expected_scale);
        API.set_param(instance,"conductor_key_travel","Closest Chord Tone");
        char saved[32768];API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";kc1,"));
        g_key_settings_restored=0;g_key_scale_mode=99;API.set_param(instance,"state",saved);assert(g_key_scale_mode==mode&&g_key_conductor_travel==1);
        API.destroy_instance(instance);
    }
    instance=fixture();single_setup(instance);new_key_major(instance,65);
    API.set_param(instance,"role","Conductor");API.set_param(instance,"source_channel","1");API.set_param(instance,"render_channel","4");single_setup(instance);
    API.set_param(instance,"conductor_key_travel","Closest Chord Tone");
    assert(press_mask(instance,60)==1u);release(instance,60); /* C is already a tone of the new F chord. */
    API.destroy_instance(instance);
    instance=fixture();single_setup(instance);API.set_param(instance,"motion_operation_1","Key Center");API.set_param(instance,"motion_gesture_1","Touch");
    press_mask(instance,62);assert(g_key_context.target_root==2);release(instance,62);API.destroy_instance(instance);
    instance=fixture();single_setup(instance);new_key_major(instance,71);
    assert(hb_key_pitch(instance,60)==59); /* C4's relative B chooses B3. */
    instance->travel_map=0;
    uint8_t pitches[3]={65,69,72};hb_commit_observed_harmony(hb_infer_harmony(pitches,3));
    API.set_param(instance,"key_center","Off");
    for(int note=36;note<96;note++){
        int mapped=hb_map_follower_note_unoperated(instance,note);
        assert(mapped-note<=6&&mapped-note>=-6);
    }
    API.destroy_instance(instance);
    /* Stock-track receivers hear exactly one transformation; conductor evidence
       remains in reference-key space for every follower and loop prediction. */
    instance=fixture();single_setup(instance);new_key_major(instance,62);
    Inst *leader=API.create_instance("",NULL);API.set_param(leader,"role","Conductor");API.set_param(leader,"source_channel","1");API.set_param(leader,"render_channel","4");
    leader->player.config.mode=1;API.set_param(leader,"chord_form","Triad");
    Inst *receiver=API.create_instance("",NULL);API.set_param(receiver,"role","Receiver");API.set_param(receiver,"source_channel","4");
    assert(press_mask(leader,60)==((1u<<2)|(1u<<6)|(1u<<9)));
    int delivered=advance(receiver,1,64);unsigned heard=0;
    for(int event=0;event<delivered;event++)if((output[event][0]&0xf0)==0x90&&output[event][2])heard|=1u<<mod12(output[event][1]);
    assert(heard==((1u<<2)|(1u<<6)|(1u<<9)));
    advance(leader,100,64);assert(bus_read().root_pc==0);
    instance->travel_map=0;assert(press_mask(instance,64)==(1u<<6));release(instance,64);release(leader,60);
    API.destroy_instance(receiver);API.destroy_instance(leader);API.destroy_instance(instance);
    /* An automatically scheduled motif commits at its future anchor, never
       at its initial trigger; cancellation cannot leave a delayed key change. */
    instance=fixture();single_setup(instance);API.set_param(instance,"motif_preset","ii-V-Target");API.set_param(instance,"motif_arrival","Next Bar");API.set_param(instance,"motif_arm","1");API.set_param(instance,"key_center","On");
    midi(instance,1,62);assert(instance->key_pending&&!g_key_context.active&&g_key_armed);
    advance(instance,2100,64);assert(g_key_context.active&&g_key_context.target_root==2&&!g_key_armed);
    API.destroy_instance(instance);
    /* Background conductor clips: saved voices and raw chord input both
       reach the stock receiver in the new key with matched note-offs. */
    for(int baked=0;baked<2;baked++){
        instance=fixture();single_setup(instance);new_key_major(instance,62);
        leader=API.create_instance("",NULL);API.set_param(leader,"role","Conductor");API.set_param(leader,"source_channel","1");API.set_param(leader,"render_channel","4");
        leader->player.config.mode=1;API.set_param(leader,"chord_form","Triad");
        leader->movy_playback=1;leader->movy_passthrough=baked;
        receiver=API.create_instance("",NULL);API.set_param(receiver,"role","Receiver");API.set_param(receiver,"source_channel","4");
        if(baked){press_mask(leader,60);press_mask(leader,64);press_mask(leader,67);}else press_mask(leader,60);
        delivered=advance(receiver,1,64);heard=0;
        for(int event=0;event<delivered;event++)if((output[event][0]&0xf0)==0x90&&output[event][2])heard|=1u<<mod12(output[event][1]);
        assert(heard==((1u<<2)|(1u<<6)|(1u<<9)));
        release(leader,60);if(baked){release(leader,64);release(leader,67);}
        delivered=advance(receiver,1,64);heard=0;
        for(int event=0;event<delivered;event++)if((output[event][0]&0xf0)==0x80||((output[event][0]&0xf0)==0x90&&!output[event][2]))heard|=1u<<mod12(output[event][1]);
        assert(heard==((1u<<2)|(1u<<6)|(1u<<9)));
        API.destroy_instance(receiver);API.destroy_instance(leader);API.destroy_instance(instance);
    }
    instance=fixture();single_setup(instance);
    char view[128];
    API.set_param(instance,"parallel_mode","Down");
    API.get_param(instance,"key_center",view,sizeof(view));assert(!strcmp(view,"Off"));
    API.set_param(instance,"parallel_mode","Up");
    API.set_param(instance,"parallel_scale","Relative Major/Minor");
    API.set_param(instance,"parallel_mode","Down");
    assert(g_key_context.target_root==9&&g_key_context.target_mask==hb_explicit_scale_mask(9,2));
    assert(hb_key_pitch(instance,60)==57);
    API.set_param(instance,"parallel_scale","Relative Major/Minor");assert(g_key_context.target_root==9);
    API.set_param(instance,"transpose","2");
    assert(g_key_context.source_root==2&&g_key_context.target_root==11);
    API.set_param(instance,"parallel_mode","Up");assert(!g_key_context.active);
    API.set_param(instance,"transpose","0");
    API.set_param(instance,"follower_explicit_root","A");API.set_param(instance,"follower_scale","Natural Minor");
    API.set_param(instance,"parallel_mode","Down");assert(g_key_context.target_root==0);
    API.set_param(instance,"parallel_mode","Up");API.destroy_instance(instance);
    instance=fixture();single_setup(instance);
    API.set_param(instance,"key_center","On");press_mask(instance,62);release(instance,62);
    API.get_param(instance,"key_center_view",view,sizeof(view));assert(strstr(view,"C>Dm"));
    API.destroy_instance(instance);
    puts("key context: progression degrees, shared live/recorded/conductor output, parallel restore, delayed landing and motif anchor pass");
}
