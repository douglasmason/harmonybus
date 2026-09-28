#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static void choose(Inst *instance,const char *operation,const char *mode){
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation",operation);
    if(mode)API.set_param(instance,"motion_amount",mode);
}
static void degrees(void){
    const char *names[]={"Secondary II","Secondary III","Secondary IV","Secondary VI","Secondary VII"};
    const int parent_roots[]={64,65,67,59,60},simple_roots[]={64,65,67,58,60};
    const unsigned parent_masks[]={ (1<<4)|(1<<7)|(1<<11)|(1<<2), (1<<5)|(1<<9)|(1<<0)|(1<<4), (1<<7)|(1<<11)|(1<<2)|(1<<5), (1<<11)|(1<<2)|(1<<5)|(1<<9), (1<<0)|(1<<4)|(1<<7)|(1<<11)};
    const unsigned simple_masks[]={ (1<<4)|(1<<7)|(1<<10)|(1<<2), (1<<5)|(1<<9)|(1<<0)|(1<<4), (1<<7)|(1<<10)|(1<<2)|(1<<5), (1<<10)|(1<<2)|(1<<5)|(1<<9), (1<<0)|(1<<4)|(1<<7)|(1<<10)};
    for(int index=0;index<5;index++)for(int simple=0;simple<3;simple++)for(int chord=0;chord<3;chord++)for(int held=0;held<2;held++){
        Inst *instance=setup();instance->player.config.mode=chord;
        choose(instance,names[index],simple==2?"Simple Scale":simple==1?"Simple Chord":"Parent Scale");
        if(held)API.set_param(instance,"motion_gesture_1","Touch");else tap(instance,1);
        unsigned expected=chord?(simple?simple_masks[index]:parent_masks[index]):1u<<mod12(simple?simple_roots[index]:parent_roots[index]);
        unsigned actual=played(instance,62);
        if(actual!=expected)fprintf(stderr,"degree %s simple%d chord%d held%d: %x expected %x\n",names[index],simple,chord,held,actual,expected);
        assert(actual==expected);release(instance,62);
        if(held)API.set_param(instance,"motion_gesture_1","Up,500");
        API.destroy_instance(instance);
    }
    for(int chord=0;chord<3;chord++)for(int target=60;target<72;target++)if((0xab5u>>(target%12))&1){
        unsigned masks[2];
        for(int operation=0;operation<2;operation++){
            Inst *instance=setup();instance->player.config.mode=chord;choose(instance,operation?"Secondary II":"Scale Above",operation?"Parent Scale":NULL);
            tap(instance,1);masks[operation]=played(instance,target);release(instance,target);API.destroy_instance(instance);
        }
        assert(masks[0]==masks[1]);
    }
}
static void connector_and_substitute(void){
    for(int quality=1;quality<=6;quality++){
        unsigned masks[2];
        for(int substitute=0;substitute<2;substitute++){
            Inst *instance=setup();instance->player.config.mode=1;instance->player.config.chromatic_quality=quality;
            choose(instance,substitute?"Tritone V":"Chrom Above",NULL);tap(instance,1);
            masks[substitute]=played(instance,60);release(instance,60);API.destroy_instance(instance);
        }
        assert(masks[1]==tones(61,4,7,10));
        if(quality==3)assert(masks[0]==tones(61,3,6,9));
    }
}
static void recording_compatibility(void){
    for(int legacy_lanes=16;legacy_lanes<=33;legacy_lanes+=17)for(int marker=3;marker<=16;marker+=13){
        Inst *instance=setup();instance->player.config.mode=1;instance->movy_playback=1;
        char message[2048];int used=snprintf(message,sizeof(message),"64");
        for(int lane=0;lane<=legacy_lanes;lane++)used+=snprintf(message+used,sizeof(message)-used,",%d",lane==legacy_lanes?marker:0);
        API.set_param(instance,"hb_movy_actions",message);
        assert(instance->recorded_action_valid[64]);
        unsigned expected=marker==3?tones(65,4,7,10):tones(66,3,6,10);
        assert(played(instance,64)==expected);release(instance,64);API.destroy_instance(instance);
    }
    for(int simple=0;simple<3;simple++)for(int index=0;index<3;index++){
        const char *names[]={"Secondary III","Secondary IV","Secondary VII"};
        Inst *instance=setup();instance->player.config.mode=1;choose(instance,names[index],simple==2?"Simple Scale":simple==1?"Simple Chord":"Parent Scale");
        API.set_param(instance,"motion_gesture_1","Touch");
        hb_mo_input(&instance->motion,62,0,0);unsigned long long words[HB_MOTION_LANES+1];hb_mo_capture(&instance->motion,0,0,62,words);
        unsigned expected=played(instance,62);release(instance,62);API.set_param(instance,"motion_gesture_1","Up,500");
        char message[4096];int used=snprintf(message,sizeof(message),"62");
        for(int lane=0;lane<=HB_MOTION_LANES;lane++)used+=snprintf(message+used,sizeof(message)-used,",%llu",words[lane]);
        API.set_param(instance,"hb_movy_actions",message);instance->movy_playback=1;
        assert(instance->recorded_action_valid[62]);assert(played(instance,62)==expected);release(instance,62);
        API.destroy_instance(instance);
    }
}
static void simplified_extensions(void){
    for(int simple=0;simple<3;simple++){
        Inst *instance=setup();instance->player.config.mode=1;
        API.set_param(instance,"chord_form","Ninth");
        choose(instance,"Secondary III",simple==2?"Simple Scale":simple==1?"Simple Chord":"Parent Scale");tap(instance,1);
        /* F Lydian from C major has B; simplified F major requires Bb,
           heard as the ninth above this A-minor secondary chord. */
        unsigned actual=played(instance,65);
        unsigned expected=tones(69,3,7,10)|(1u<<(simple==2?10:11));
        assert(actual==expected);release(instance,65);API.destroy_instance(instance);
    }
}
int main(void){degrees();simplified_extensions();connector_and_substitute();recording_compatibility();puts("secondary degrees: parent vs simplified, neighbor equivalence, connectors, tritone and recording compatibility pass");}
