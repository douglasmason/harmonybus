#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static void fixed_cadences(void){
    const char *names[]={"II-V-Target","Backdoor II-V-Target","Tritone II-V-Target"};
    const int offsets[3][3]={{2,-5,0},{5,-2,0},{-4,1,0}};
    for(int family=0;family<3;family++)for(int mode=0;mode<3;mode++)for(int shift=-2;shift<=2;shift+=2){
        Inst *instance=setup();instance->player.config.mode=mode;hb_set_master_transpose(shift);
        API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation",names[family]);tap(instance,1);
        for(int step=0;step<3;step++){
            int root=60+shift+offsets[family][step];
            unsigned expected=mode?tones(root,step==0?3:4,7,step==2?11:10):1u<<mod12(root);
            unsigned actual=played(instance,60);
            if(actual!=expected)fprintf(stderr,"family %d mode %d step %d: %x expected %x\n",family,mode,step,actual,expected);
            assert(actual==expected);release(instance,60);
        }
        assert(!instance->motion.enclosure);API.destroy_instance(instance);
    }
}
static void tritone_gestures(void){
    Inst *instance=setup();instance->player.config.mode=1;
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Tritone II");
    API.set_param(instance,"motion_lane","2");API.set_param(instance,"motion_operation","Chrom Above");
    tap(instance,1);tap(instance,2);
    assert(played(instance,60)==tones(56,3,7,10));release(instance,60);
    assert(played(instance,60)==tones(61,4,7,10));release(instance,60);
    assert(played(instance,60)==tones(60,4,7,11));release(instance,60);
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Tritone II-V-Target");
    API.set_param(instance,"motion_gesture_1","Touch,1000");API.set_param(instance,"motion_gesture_1","Up,50,1050");
    API.set_param(instance,"motion_gesture_1","Touch,1100");API.set_param(instance,"motion_gesture_1","Up,50,1150");
    assert(instance->motion.gesture_persistent&1);
    for(int repeat=0;repeat<2;repeat++){
        assert(played(instance,60)==tones(56,3,7,10));release(instance,60);
        assert(played(instance,60)==tones(61,4,7,10));release(instance,60);
        assert(played(instance,60)==tones(60,4,7,11));release(instance,60);
    }
    API.set_param(instance,"motion_gesture_1","Touch,2000");API.set_param(instance,"motion_gesture_1","Up,50,2050");
    assert(!instance->motion.enclosure);
    API.destroy_instance(instance);
}
int main(void){fixed_cadences();tritone_gestures();puts("cadence sequences: three-press families, tritone pair, raw/chord, transpose, persistence pass");}
