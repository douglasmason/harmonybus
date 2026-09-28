#define main existing_reference_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *instance=fixture();API.set_param(instance,"motion_host","movy-clip-v2");
    char snapshot[65536],value[128],expected[256],small[24];
    const char *keys[]={"motion_lane","motion_operation","motion_amount","motion_offset","motion_every","motion_from","motion_through","motion_condition_range","motion_condition_status"};
    for(int operation=0;operation<=HB_MO_MIXED_LAST;operation++){
        API.set_param(instance,"motion_operation",MO_OPERATIONS[operation]);
        API.set_param(instance,"motion_every","8");API.set_param(instance,"motion_from","3");
        int count=API.get_param(instance,"motion_editor",snapshot,sizeof(snapshot));
        assert(count>0&&count<(int)sizeof(snapshot)&&snapshot[count-1]=='}');
        for(unsigned index=0;index<sizeof(keys)/sizeof(keys[0]);index++){
            assert(API.get_param(instance,keys[index],value,sizeof(value))>=0);
            snprintf(expected,sizeof(expected),"\"%s\":\"%s\"",keys[index],value);
            assert(strstr(snapshot,expected));
        }
        memset(small,0x55,sizeof(small));
        assert(API.get_param(instance,"motion_editor",small,16)<0);
        for(int index=16;index<(int)sizeof(small);index++)assert(small[index]==0x55);
        puts(snapshot);
    }
    API.destroy_instance(instance);
}
