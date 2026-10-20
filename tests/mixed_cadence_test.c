#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static void programs(void){
    const int roots[14][6]={{8,10,0},{8,7,0},{3,5,0},{9,7,0},{4,9,2,7,0},{5,5,0},{2,7,0},{0,9,2,7,0},{2,7,0},{9,2,7,0},{9,2,7,0},{4,9,2,7,0},{6,7,0},{4,9,2,7,0}};
    const int qualities[14][6]={{5,6,5},{5,6,5},{5,5,5},{7,6,5},{7,7,7,6,5},{5,7,5},{8,6,7},{5,6,7,6,5},{6,6,5},{7,6,6,5},{6,7,6,5},{6,7,7,6,5},{9,6,5},{6,6,6,6,5}};
    for(int program=0;program<14;program++)for(int mode=0;mode<3;mode++)for(int transpose=0;transpose<12;transpose++){
        Inst *instance=setup();instance->player.config.mode=mode;
        char value[20];snprintf(value,sizeof(value),"%d",transpose);API.set_param(instance,"transpose",value);
        tap(instance,38+program);assert(instance->motion.enclosure==14);
        for(int step=0;step<HB_CADENCES[program].length;step++){
            int root=roots[program][step]+transpose,quality=qualities[program][step];
            unsigned expected=mode?tones(root,quality==7||quality==8||quality==9?3:4,quality==8||quality==9?6:7,quality==5?11:quality==9?9:10):1u<<mod12(root);
            unsigned actual=played(instance,60);
            if(actual!=expected)fprintf(stderr,"mixed %d step%d mode%d trans%d got%x expected%x\n",program,step,mode,transpose,actual,expected);
            assert(actual==expected);release(instance,60);
        }
        assert(!instance->motion.enclosure);API.destroy_instance(instance);
    }
}
static void recording_and_preview(void){
    Inst *instance=setup();instance->player.config.mode=1;
    tap(instance,51);char lights[1024];unsigned long long active,persistent,down;
    API.get_param(instance,"motion_named_lights",lights,sizeof(lights));
    assert(sscanf(lights,"%llu,%llu,%llu",&active,&persistent,&down)==3);assert(active&(1ULL<<34));
    char pads[8192];int pending=instance->motion.enclosure_step;
    API.get_param(instance,"pad_render",pads,sizeof(pads));assert(instance->motion.enclosure_step==pending);
    unsigned expected=tones(64,4,7,10);assert(played(instance,60)==expected);
    unsigned long long actions[HB_MOTION_LANES+1];memcpy(actions,instance->motion.events,sizeof(actions));release(instance,60);
    instance->motion.enclosure=0;instance->movy_playback=1;instance->recorded_action_valid[60]=1;memcpy(instance->recorded_actions[60],actions,sizeof(actions));
    assert(played(instance,60)==expected);release(instance,60);
    instance->movy_playback=0;API.set_param(instance,"motion_lane","51");API.set_param(instance,"motion_auto_off","Chord Change");tap(instance,51);
    for(int step=0;step<10;step++){played(instance,60);release(instance,60);}assert(instance->motion.enclosure==14&&instance->motion.enclosure_step==0);
    API.destroy_instance(instance);
}
int main(void){programs();recording_and_preview();puts("mixed cadences: all programs, twelve keys, raw/chord modes, immutable replay and non-consuming previews pass");return 0;}
