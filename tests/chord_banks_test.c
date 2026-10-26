#define main ignored_intent_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *conductor=API.create_instance("",0);API.set_param(conductor,"role","Conductor");
    API.set_param(conductor,"follower_root_policy","Explicit");API.set_param(conductor,"follower_explicit_root","C");
    API.set_param(conductor,"follower_scale","Major");API.set_param(conductor,"render_channel","Off");
    const int minor_roots[]={0,2,3,5,7,8,10},major_roots[]={0,2,4,5,7,9,11};
    for(int minor=0;minor<2;minor++){
        API.set_param(conductor,"follower_scale",minor?"Natural Minor":"Major");
        for(int bank=0;bank<3;bank++)for(int degree=0;degree<7;degree++){
            int fields[]={0,0,0,bank==2?1:3,0,1,12,bank==0?1:bank==1?12:13,degree},note=0;
            unsigned long long word=hb_selection_resolve(conductor,fields,&note);assert(hb_selection_word(word));
            const int *roots=bank==2?minor_roots:(bank==0?minor:!minor)?minor_roots:major_roots;
            assert(note==60+roots[degree]);hb_harmony_t harmony=hb_selection_decode(word,note);
            unsigned mask=hb_transpose_mask(hb_harmony_chord_mask(harmony),-roots[degree]);
            if(bank==2)assert(mask==0x491); /* Every blues chord is a dominant seventh. */
            if(bank==1&&degree==0)assert(mask==(minor?0x91:0x89));
            const int major_families[]={1,2,2,1,3,2,4},minor_families[]={2,4,1,2,2,1,3};
            int expected_family=bank==2?HB_FAMILY_DOMINANT:
                ((bank==0?minor:!minor)?minor_families:major_families)[degree];
            assert(hb_selection_family(harmony)==expected_family);
        }
    }
    API.destroy_instance(conductor);puts("Relative home/parallel/blues banks follow parent major/minor quality");
}
