/* Shared tension rule, generated chord ownership and snapshot persistence. */
#define main legacy_chord_tests
#include "chord_player_test.c"
#undef main

static unsigned note_mask(const int *notes,int count){
    unsigned mask=0;
    for(int index=0;index<count;index++)mask|=1u<<mod12(notes[index]);
    return mask;
}
static unsigned output_mask(int count){
    unsigned mask=0;
    for(int index=0;index<count;index++)mask|=1u<<mod12(output[index][1]);
    return mask;
}
static void collections(void){
    const int roots[]={0,2,4,7};
    const int scales[]={5,3,3,12}; /* C Lydian, D/E Dorian, G Lydian dominant */
    for(int shift=0;shift<12;shift++)for(int row=0;row<4;row++)for(int mode=1;mode<=2;mode++){
        hb_cp_config config;hb_cp_defaults(&config);config.mode=1;config.size=3;
        unsigned parent=hb_explicit_scale_mask(shift,1),semantic=0;
        int root=mod12(roots[row]+shift),notes[12];
        int count=hb_cp_voice(config,60+root,root,0,parent,notes);
        unsigned core=note_mask(notes,count);
        config.mode=mode;config.size=9;config.extension_treatment=1;
        count=hb_cp_voice_semantic(config,60+root,root,core,parent,notes,&semantic);
        unsigned expected=hb_explicit_scale_mask(root,scales[row]);
        assert(note_mask(notes,count)==expected&&semantic==expected);
        config.size=2;
        count=hb_cp_voice(config,60+root,root,core,parent,notes);
        config.extension_treatment=0;int plain[12];
        int plain_count=hb_cp_voice(config,60+root,root,core,parent,plain);
        assert(count==plain_count&&note_mask(notes,count)==note_mask(plain,plain_count));
    }
    hb_cp_config config;hb_cp_defaults(&config);config.mode=2;config.size=4;config.extension_treatment=1;
    unsigned explicit_flat9=(1u<<0)|(1u<<1)|(1u<<4)|(1u<<7)|(1u<<10);
    unsigned phrygian_dominant=(1u<<0)|(1u<<1)|(1u<<4)|(1u<<5)|(1u<<7)|(1u<<8)|(1u<<10);
    int notes[12];int count=hb_cp_voice(config,60,0,explicit_flat9,phrygian_dominant,notes);
    assert(note_mask(notes,count)==explicit_flat9);
    config.size=HB_CP_FOLLOW_DETECTED;
    count=hb_cp_voice(config,60,0,explicit_flat9,phrygian_dominant,notes);
    assert(note_mask(notes,count)==explicit_flat9);
    config.mode=1;config.size=11;
    count=hb_cp_voice(config,60,0,0,0xab5,notes);
    assert(note_mask(notes,count)==((1u<<0)|(1u<<5)|(1u<<7))); /* Sus4 stays F. */
}
static void rendering_and_restore(void){
    Inst *instance=fixture();
    uint8_t em7[]={64,67,71,74};
    hb_harmony_t harmony=hb_infer_harmony(em7,4);hb_commit_observed_harmony(harmony);
    API.set_param(instance,"chord_mode","Conductor Chord");
    API.set_param(instance,"chord_form","Ninth");API.set_param(instance,"chord_inversion","Root");
    API.set_param(instance,"content_map","Raised-Avoid");
    const unsigned core=(1u<<4)|(1u<<7)|(1u<<11)|(1u<<2);
    midi(instance,1,60);int count=advance(instance,0,64);
    assert(count==5&&output_mask(count)==(core|(1u<<5)));
    API.set_param(instance,"chord_extensions","Raise Avoid");
    assert(advance(instance,0,64)==0); /* Existing held chord is not silently rewritten. */
    midi(instance,0,60);count=advance(instance,0,64);
    assert(count==5&&output_mask(count)==(core|(1u<<5)));
    midi(instance,1,60);count=advance(instance,0,64);
    assert(count==5&&output_mask(count)==(core|(1u<<6)));
    assert(hb_pad_chord_mask_base_form(instance,harmony,4)==(core|(1u<<6)));
    midi(instance,0,60);assert(advance(instance,0,64)==5);
    char state[131072],old_state[131072],value[32];
    API.get_param(instance,"state",state,sizeof(state));
    Inst *copy=API.create_instance("",NULL);API.set_param(copy,"state",state);
    API.get_param(copy,"chord_extensions",value,sizeof(value));assert(!strcmp(value,"Raise Avoid"));
    strcpy(old_state,state);char *extension=strstr(old_state,";ce1,1");assert(extension);
    memmove(extension,extension+6,strlen(extension+6)+1);
    API.set_param(copy,"state",old_state);assert(!copy->player.config.extension_treatment);
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Chord/Arp State");
    API.set_param(instance,"motion_enabled","Off");API.set_param(instance,"chord_edit_target","Lane 1");
    API.set_param(instance,"chord_state_copy","Copy");
    assert(instance->motion.lanes[0].chord_state.extension_treatment==1);
    API.set_param(instance,"chord_extensions","As Derived");assert(instance->player.config.extension_treatment==1);
    assert(!instance->motion.lanes[0].chord_state.extension_treatment);
    API.set_param(instance,"chord_extensions","Raise Avoid");
    API.get_param(instance,"state",state,sizeof(state));API.set_param(copy,"state",state);
    assert(copy->motion.lanes[0].chord_state.extension_treatment==1);
    assert(strstr(state,";cs1,")&&strstr(state,";ce2,0,1"));
    API.destroy_instance(copy);API.destroy_instance(instance);
}
int main(void){
    collections();rendering_and_restore();
    puts("Raised chord extensions: transposition, identity, suspensions, held notes, pads and state pass");
}
