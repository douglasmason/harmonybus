#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"

static void source_and_quality(void){
    Inst *instance=setup();
    unsigned parent=hb_explicit_scale_mask(0,1);
    /* F is Lydian in C: Auto keeps it for ordinary secondary degrees, but
       tonicizes it for explicit dominant/leading-tone function. */
    assert(hb_relative_target_scale(instance,65,parent,0)==parent);
    assert(hb_relative_target_scale(instance,65,parent,1)==hb_explicit_scale_mask(5,1));
    API.set_param(instance,"target_scale_source","Parent");
    assert(hb_relative_target_scale(instance,65,parent,1)==parent);
    API.set_param(instance,"target_scale_source","Simplified");
    API.set_param(instance,"target_scale_major","Lydian");
    assert(hb_relative_target_scale(instance,65,parent,0)==parent);
    API.set_param(instance,"target_scale_major","Harmonic Major");
    assert(hb_relative_target_scale(instance,65,parent,0)==hb_explicit_scale_mask(5,19));
    API.set_param(instance,"target_scale_minor","Melodic Minor");
    assert(hb_relative_target_scale(instance,62,parent,0)==hb_explicit_scale_mask(2,9));
    assert(hb_relative_target_scale(instance,71,parent,0)==hb_explicit_scale_mask(11,7));
    API.set_param(instance,"target_scale_diminished","Locrian #2");
    assert(hb_relative_target_scale(instance,71,parent,0)==hb_explicit_scale_mask(11,14));
    /* Explicit Parent wins over a saved legacy Simple Scale operation. */
    instance->motion.render_flags=HB_MO_SIMPLE|HB_MO_SIMPLE_SCALE;
    API.set_param(instance,"target_scale_source","Parent");
    assert(hb_relative_target_scale(instance,62,parent,0)==parent);
    assert(!hb_target_simple_chord(instance));
    API.destroy_instance(instance);
}

static void musical_output(void){
    Inst *instance=setup();instance->player.config.mode=1;
    API.set_param(instance,"target_scale_source","Simplified");
    API.set_param(instance,"target_scale_minor","Melodic Minor");
    /* Secondary VI of D melodic minor is B half-diminished, not Bb major. */
    tap(instance,3);
    assert(played(instance,62)==tones(59,3,6,10));release(instance,62);
    /* Explicit V remains dominant, even when its parent is preserved. */
    API.set_param(instance,"target_scale_source","Parent");
    tap(instance,2);
    assert(played(instance,62)==tones(57,4,7,10));release(instance,62);
    API.destroy_instance(instance);
}

static void cadence_and_dominant_family(void){
    Inst *instance=setup();
    unsigned parent=hb_explicit_scale_mask(0,1);
    hb_cadence_step step={.kind=HB_CAD_DEGREE,.degree=6};
    API.set_param(instance,"target_scale_source","Simplified");
    API.set_param(instance,"target_scale_minor","Melodic Minor");
    hb_cadence_result result=hb_resolve_cadence(instance,&step,62,parent);
    assert(result.root==59&&result.scale==hb_explicit_scale_mask(2,9));
    step.kind=HB_CAD_DOMINANT;
    result=hb_resolve_cadence(instance,&step,62,parent);
    assert(result.root==57&&result.quality==6&&result.scale==hb_explicit_scale_mask(2,9));
    API.set_param(instance,"dominant_minor_scale","Harmonic Minor");
    result=hb_resolve_cadence(instance,&step,62,parent);
    assert(result.scale==hb_explicit_scale_mask(2,8));
    step.kind=HB_CAD_LEADING;
    result=hb_resolve_cadence(instance,&step,62,parent);
    assert(result.root==61&&result.quality==9&&result.scale==hb_explicit_scale_mask(2,8));
    API.destroy_instance(instance);
}

static void persistence_and_track_isolation(void){
    Inst *instance=setup();
    API.set_param(instance,"target_scale_source","Simplified");
    API.set_param(instance,"target_scale_major","Lydian");
    API.set_param(instance,"target_scale_minor","Melodic Minor");
    API.set_param(instance,"target_scale_diminished","Locrian #2");
    char state[131072],value[64];
    assert(API.get_param(instance,"state",state,sizeof(state))>0);
    assert(strstr(state,";ts1,2,1,3,1"));
    Inst *other=API.create_instance(0,0);assert(other);
    assert(other->target_scale_policy[0]==0);
    API.set_param(other,"state",state);
    for(int field=0;field<4;field++)assert(other->target_scale_policy[field]==instance->target_scale_policy[field]);
    API.set_param(other,"target_scale_minor","Dorian");
    API.get_param(instance,"target_scale_minor",value,sizeof(value));assert(!strcmp(value,"Melodic Minor"));
    char *suffix=strstr(state,";ts1,");assert(suffix);*suffix=0;
    API.set_param(other,"state",state);
    for(int field=0;field<4;field++)assert(other->target_scale_policy[field]==0);
    strcat(state,";ts1,2,1,99,1");API.set_param(other,"state",state);
    for(int field=0;field<4;field++)assert(other->target_scale_policy[field]==0);
    API.destroy_instance(other);API.destroy_instance(instance);
}

int main(void){
    source_and_quality();musical_output();cadence_and_dominant_family();persistence_and_track_isolation();
    puts("target scales: Auto/Parent/Simplified, quality families, MIDI, cadences, precedence and persistence pass");
}
