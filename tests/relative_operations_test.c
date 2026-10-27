/* Exercise actual selectors, rendered harmony, preview and saved-note replay. */
#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main

static void catalog_contract(void){
    assert(HB_MO_LEADING_TONE==51&&HB_MO_UPPER_DIM==52&&HB_MO_DRUM_FILL==66);
    for(int choice=0;choice<HB_AR_CHOICE_COUNT;choice++){
        const hb_ro_definition *definition=hb_ro_choice(choice);
        if(!definition)continue;
        assert(hb_ro_token(definition->token)==definition);
        if(definition->motion>=0&&!definition->preferred_token){
            assert(hb_ro_motion(definition->motion)==definition);
            assert(hb_ro_trigger(hb_mo_trigger_bit(definition->motion))==definition);
        }
        if(definition->role)assert(hb_ro_role(definition->role)==definition);
    }
    assert(hb_ar_intent(14)==hb_mo_role_word(12));
    assert(hb_ar_intent(60)==hb_mo_role_word(13));
    assert(hb_ar_intent(1)==1&&hb_ar_intent(2)==(3|HB_MO_CONNECTOR_ABOVE));
    assert(hb_ar_intent(10)==3);
    assert(!strcmp(hb_ro_canonical_name("LT"),"Leading Tone"));
    assert(!strcmp(hb_ro_canonical_name("Secondary LT"),"Connector Below"));
    assert(!strcmp(hb_ro_canonical_name("UCA"),"Connector Above"));
}

static void gap_selection(void){
    /* Validate actual missing pitch classes, including both chromatic notes
       inside each augmented second, in every transposition. */
    const unsigned collections[]={0x9adu,0x9b5u,0x999u,0x555u,0x6dbu};
    for(unsigned collection=0;collection<sizeof(collections)/sizeof(collections[0]);collection++)
        for(int tonic=0;tonic<12;tonic++)for(int direction=0;direction<2;direction++){
            unsigned scale=hb_transpose_mask(collections[collection],tonic),covered=scale;
            for(int degree=0;degree<12;degree++)if(scale&(1u<<degree)){
                unsigned token=hb_ro_resolve_selector(4096+direction,60+degree,scale);
                assert(token==14||token==60);
                const hb_ro_definition *operation=hb_ro_token(token);
                covered|=1u<<mod12(degree+operation->semitones);
                assert(operation->quality==9);
            }
            assert(covered==0xfffu);
        }
    for(unsigned token=4096;token<=4097;token++){
        assert(hb_ro_resolve_selector(token,0,0)==60);
        assert(hb_ro_resolve_selector(token,127,0)==14);
        assert(hb_ro_resolve_selector(token,60,0)==(token==4096?14u:60u));
        assert(hb_ro_resolve_selector(token,60,0xfff)==(token==4096?14u:60u));
    }
    for(unsigned token=1;token<2048;token++)assert(hb_ro_resolve_selector(token,60,0x9ad)==token);
}

static void note(Inst *instance,int source,int on){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];
    int lengths[64];API.process_midi(instance,message,3,output,lengths,64);
    API.tick(instance,64,48000,output,lengths,64);
}
static unsigned sounding_mask(Inst *instance,int source){
    unsigned mask=0;
    for(int owner=0;owner<HB_CP_KEYS;owner++){
        hb_cp_key *key=&instance->player.keys[owner];
        if(key->used&&key->source==source)for(int voice=0;voice<key->count;voice++)mask|=1u<<mod12(key->notes[voice]);
    }
    return mask;
}
static Inst *approach_fixture(void){
    Inst *instance=fixture();instance->travel_map=7;instance->content_map=1;
    instance->chromatic_map=1;instance->boundary_buffer_ms=0;instance->next_anti_buffer_ms=0;
    instance->player.config.mode=1;instance->player.config.size=3;
    instance->approach_layout=1;instance->preview_count=32;
    API.set_param(instance,"follower_explicit_root","A");hb_set_shared_follower_scale(8);
    g_bus.observed_harmony=chord(9,1,0);hb_effective_write(g_bus.observed_harmony);
    return instance;
}
static void production_paths(void){
    for(int direction=0;direction<2;direction++)for(int transpose=0;transpose<=5;transpose+=5)for(int target_index=0;target_index<2;target_index++){
        Inst *instance=approach_fixture();g_bus.global_transpose=transpose;
        g_bus.observed_harmony=hb_transpose_harmony(g_bus.observed_harmony,transpose);hb_effective_write(g_bus.observed_harmony);
        API.set_param(instance,"approach_bank_1",direction?"Smart Above":"Smart Below");
        API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,50");
        const int target=target_index?68:65,source=target-36,root=target_index?67:66;
        const unsigned expected_token=target_index?14:60;
        unsigned expected=0;for(int tone=0;tone<4;tone++)expected|=1u<<mod12(root+transpose+3*tone);
        char message[64];snprintf(message,sizeof(message),"%d,36,3",source);
        API.set_param(instance,"hb_movy_input_approach",message);note(instance,source,1);
        assert(instance->approach_rows.tokens[source]==expected_token);
        assert(sounding_mask(instance,source)==expected);
        unsigned long long captured[HB_MOTION_LANES+1];hb_capture_input_intent(instance,source,captured);
        assert(((captured[HB_MOTION_LANES]>>HB_AR_SHIFT)&2047)==expected_token);
        assert(hb_ar_intent(expected_token)==(captured[HB_MOTION_LANES]&0x573ULL));

        Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
        preview->approach_rows.preview_row=3;
        unsigned mask=hb_pad_render_mask(preview,instance,g_bus.observed_harmony,source,0,0,0);
        assert(mask==expected&&preview->approach_rows.tokens[source]==expected_token);free(preview);
        note(instance,source,0);assert(!instance->player.sounding_count);

        /* Persist the selected concrete operation through the real decoder.
           Editing the live bank cannot change what the recording means. */
        char actions[8192];int used=snprintf(actions,sizeof(actions),"%d",source);
        for(int lane=0;lane<=HB_MOTION_LANES;lane++)used+=snprintf(actions+used,sizeof(actions)-used,",%llu",captured[lane]);
        API.set_param(instance,"hb_movy_actions",actions);assert(instance->recorded_action_valid[source]);
        API.set_param(instance,"approach_bank_1",target_index?"Upper Dim":"Leading Tone");
        instance->movy_playback=1;note(instance,source,1);
        assert(instance->approach_rows.tokens[source]==expected_token&&sounding_mask(instance,source)==expected);
        note(instance,source,0);instance->movy_playback=0;assert(!instance->player.sounding_count);

        API.set_param(instance,"approach_bank_1","Smart Below");API.set_param(instance,"approach_bank_2","Smart Above");
        char saved[65536],label[128];API.get_param(instance,"state",saved,sizeof(saved));
        Inst *restored=API.create_instance("",0);API.set_param(restored,"state",saved);
        API.get_param(restored,"approach_bank_1",label,sizeof(label));assert(!strcmp(label,"Smart Below"));
        API.get_param(restored,"approach_bank_2",label,sizeof(label));assert(!strcmp(label,"Smart Above"));
        API.get_param(instance,"approach_sequence_view",label,sizeof(label));assert(strstr(label,"S Below"));
        API.destroy_instance(restored);API.destroy_instance(instance);
    }
}
static void fixed_operation_previews(void){
    const unsigned tokens[]={61,62,63,59};
    for(unsigned index=0;index<4;index++){
        Inst *instance=approach_fixture();
        const hb_ro_definition *definition=hb_ro_token(tokens[index]);assert(definition);
        /* Use the canonical catalog name, including its established capitalization. */
        API.set_param(instance,"approach_bank_1",definition->name);
        API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,50");
        API.set_param(instance,"hb_movy_input_approach","33,36,3");note(instance,33,1);
        unsigned expected=sounding_mask(instance,33);assert(expected);
        Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
        preview->approach_rows.preview_row=3;
        assert(hb_pad_render_mask(preview,instance,g_bus.observed_harmony,33,0,0,0)==expected);
        free(preview);note(instance,33,0);assert(!instance->player.sounding_count);API.destroy_instance(instance);
    }
}
static void motif_capture(void){
    Inst *instance=approach_fixture();API.set_param(instance,"approach_bank_1","Smart Below");
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,50");
    API.set_param(instance,"motif_record","Edit");assert(instance->motif.editor.recording>=0);
    API.set_param(instance,"hb_movy_input_approach","29,36,3");note(instance,29,1);note(instance,29,0);
    const hb_mt_event *event=&instance->motif.editor.draft.events[0];
    assert(instance->motif.editor.draft.count==1&&event->secondary==13);
    assert(((event->actions[HB_MOTION_LANES]>>HB_AR_SHIFT)&2047)==60);
    API.set_param(instance,"motif_record","Done");assert(instance->motif.editor.recording<0);
    API.destroy_instance(instance);
}
static unsigned rendered_mask;
static int capture_render(const uint8_t *packet,int length){
    if(length==4&&(packet[0]&0xf0)!=0x30&&(packet[1]&0xf0)==0x90&&packet[3])rendered_mask|=1u<<mod12(packet[2]);
    return length;
}
static void single_note_paths(void){
    host.midi_inject_to_move=capture_render;
    for(int role=0;role<2;role++)for(int target_index=0;target_index<2;target_index++){
        Inst *instance=approach_fixture();instance->player.config.mode=0;
        API.set_param(instance,"role",role?"Follower":"Conductor");
        API.set_param(instance,"source_channel","1");API.set_param(instance,"render_channel","4");
        API.set_param(instance,"approach_bank_1","Smart Below");
        instance->approach_rows.enabled=1;instance->approach_layout=0;
        API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,50");
        int target=target_index?68:65,root=target_index?67:66;
        rendered_mask=0;note(instance,target,1);unsigned actual=rendered_mask;
        assert(instance->approach_rows.tokens[target]==(target_index?14:60));
        if(actual!=(1u<<mod12(root)))fprintf(stderr,"single role%d target%d: mask%x expected%x\n",role,target,actual,1u<<mod12(root));
        assert(actual==(1u<<mod12(root))&&instance->player.config.mode==0);
        note(instance,target,0);API.destroy_instance(instance);
    }
    host.midi_inject_to_move=0;
}
#ifndef HB_RELATIVE_OPERATIONS_FIXTURE
int main(void){
    catalog_contract();gap_selection();production_paths();fixed_operation_previews();motif_capture();single_note_paths();
    puts("Relative operations: stable identities, chromatic gap coverage, complete harmony, preview, recording and persistence pass");
}
#endif
