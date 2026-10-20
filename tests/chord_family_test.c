#define main previous_chord_player_tests
#include "chord_player_test.c"
#undef main
static unsigned note_mask(const int *notes,int count){unsigned mask=0;for(int index=0;index<count;index++)mask|=1u<<mod12(notes[index]);return mask;}
static void exact_families(void){
    const int intervals[][6]={{0,4,8,10,-1},{0,4,8,11,-1},{0,2,4,8,10,-1},{0,2,4,8,11,-1},
        {0,4,6,10,-1},{0,2,4,6,10,-1},{0,2,3,7,11,-1},{0,5,7,10,-1},{0,2,5,7,10,-1},
        {0,2,7,10,-1},{0,4,6,11,-1},{0,2,3,6,10,-1},{0,1,4,7,10,-1},{0,3,4,7,10,-1},
        {0,1,4,8,10,-1},{0,3,4,8,10,-1}};
    for(int family=0;family<16;family++)for(int root=0;root<12;root++){
        uint8_t notes[6];int count=0;unsigned expected=0;
        for(int index=0;index<6&&intervals[family][index]>=0;index++){notes[count++]=(uint8_t)(48+root+intervals[family][index]);expected|=1u<<mod12(notes[count-1]);}
        hb_harmony_t inferred=hb_infer_harmony(notes,count);
        if(inferred.root_pc!=root||inferred.chord_index!=26+family)fprintf(stderr,"family%d root%d => %s index%d\n",family,root,inferred.name,inferred.chord_index);
        assert(inferred.root_pc==root&&inferred.chord_index==26+family);
        assert(hb_harmony_chord_mask(inferred)==expected);
        for(int inversion=0;inversion<count;inversion++){
            uint8_t rotated[6];for(int index=0;index<count;index++)rotated[index]=notes[index]+(index<inversion?12:0);
            hb_harmony_t rooted=hb_refine_harmony_with_root(rotated,count,inferred);
            assert(rooted.chord_index==inferred.chord_index&&hb_harmony_chord_mask(rooted)==expected);
            hb_cp_config config;hb_cp_defaults(&config);config.mode=2;config.size=0;
            int output_notes[12];int voiced=hb_cp_voice(config,60+root,root,expected,hb_explicit_scale_mask(root,1),output_notes);
            assert(note_mask(output_notes,voiced)==expected);
            char display[64];hb_format_harmony(display,sizeof(display),rooted);assert(!strchr(display,'?'));
        }
    }
    /* Ambiguous fifth-less shells remain ordinary sevenths. */
    uint8_t major_shell[]={60,64,70},minor_shell[]={60,63,70};
    assert(hb_infer_harmony(major_shell,3).chord_index==10);assert(hb_infer_harmony(minor_shell,3).chord_index==11);
}
static void symmetric_scales(void){
    for(int scale=16;scale<=17;scale++)for(int root=0;root<12;root++){
        unsigned parent=hb_explicit_scale_mask(root,scale);assert(hb_popcount12(parent)==6);
        for(int offset=0;offset<12;offset++)if(parent&(1u<<mod12(root+offset))){
            hb_cp_config config;hb_cp_defaults(&config);config.mode=1;config.size=3;config.inversion=1;
            int output_notes[12];int count=hb_cp_voice(config,48+root+offset,root,0,parent,output_notes);
            unsigned actual=note_mask(output_notes,count);assert(!(actual&~parent));
            assert(actual&(1u<<mod12(root+offset+4)));assert(actual&(1u<<mod12(root+offset+8)));
            assert(count==(scale==16||offset%4==0?4:3));
        }
    }
    hb_cp_config config;hb_cp_defaults(&config);config.mode=1;config.size=3;config.inversion=1;
    int notes[12];int count=hb_cp_voice(config,63,3,0,hb_explicit_scale_mask(0,9),notes);
    assert(count==4&&note_mask(notes,count)==((1u<<3)|(1u<<7)|(1u<<11)|(1u<<2)));
}
static void persistence(void){
    for(int scale=16;scale<=17;scale++)for(int quality=10;quality<=12;quality++){
        Inst *instance=fixture();API.set_param(instance,"follower_scale",scale==16?"Whole Tone":"Augmented");
        const char *name=quality==10?"MinMaj7":quality==11?"AugMaj7":"Dom7b5";
        API.set_param(instance,"chord_quality",name);assert(instance->player.config.quality==quality);
        char state[16384];API.get_param(instance,"state",state,sizeof(state));API.destroy_instance(instance);
        g_scale_restored=0;instance=API.create_instance("",0);API.set_param(instance,"state",state);
        assert(hb_shared_follower_scale()==scale&&instance->player.config.quality==quality);API.destroy_instance(instance);
    }
}
int main(void){exact_families();symmetric_scales();persistence();puts("chord families: 16 altered/extended families in every key, rooted inversions, voicing, shells, symmetric scales, melodic minor and state roundtrip pass");}
