#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"

static void source_and_quality(void){
    Inst *instance=setup();API.set_param(instance,"dominant_scale","Simplified Target");
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
    Inst *instance=setup();API.set_param(instance,"dominant_scale","Simplified Target");
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
    for(int field=0;field<4;field++)assert(other->target_scale_policy[field]==(field==2?2:0));
    strcat(state,";ts1,2,1,99,1");API.set_param(other,"state",state);
    for(int field=0;field<4;field++)assert(other->target_scale_policy[field]==(field==2?2:0));
    API.destroy_instance(other);API.destroy_instance(instance);
}

static void plain_and_dominant_degrees(void){
    Inst *instance=setup();instance->player.config.mode=1;
    API.set_param(instance,"dominant_minor_scale","Harmonic Minor");
    API.set_param(instance,"motion_lane","1");
    API.set_param(instance,"motion_operation","Secondary Fifth");
    assert(instance->motion.lanes[0].operation==HB_MO_SECONDARY_FIFTH);
    tap(instance,1);assert(played(instance,71)==tones(65,4,7,11));release(instance,71); /* fifth of B Locrian is F, not F# */
    API.set_param(instance,"motion_operation","Secondary II");
    tap(instance,1);assert(played(instance,62)==tones(64,3,7,10));release(instance,62); /* parent C gives Em7 */
    API.set_param(instance,"motion_operation","Secondary II (Dom)");
    tap(instance,1);assert(played(instance,62)==tones(64,3,6,10));release(instance,62); /* D harmonic minor gives E halfdim */
    API.set_param(instance,"motion_operation","Secondary IV (Dom)");
    tap(instance,1);assert(played(instance,62)==tones(67,3,7,10));release(instance,62);
    API.set_param(instance,"motion_operation","Secondary VI (Dom)");
    tap(instance,1);assert(played(instance,62)==tones(58,4,7,11));release(instance,62);
    API.set_param(instance,"dominant_minor_scale","Altered V");
    assert(hb_secondary_collection(instance,15,62,hb_explicit_scale_mask(0,1))==hb_explicit_scale_mask(2,8));
    for(int role=14;role<=17;role++){
        instance->motion.events[HB_MOTION_LANES]=hb_mo_role_word(role);
        assert(hb_mo_source_secondary(&instance->motion)==role);
    }
    API.set_param(instance,"approach_bank_1","Secondary IV (Dom)");
    assert(instance->approach_rows.bank[0]==-34);
    assert(hb_ar_intent(hb_ar_code(&instance->approach_rows,0))==hb_mo_role_word(16));
    API.destroy_instance(instance);
}
static void dominant_family_contract(void){
    Inst *instance=setup();
    unsigned parent=hb_explicit_scale_mask(0,1);
    API.set_param(instance,"dominant_minor_scale","None");
    assert(hb_secondary_collection(instance,15,62,parent)==parent);
    const char *families[]={"Simplified Target","Harmonic Minor","Melodic Minor","Altered V","Major","Harmonic Major"};
    for(int family=0;family<6;family++){
        API.set_param(instance,"dominant_minor_scale",families[family]);
        unsigned collection=hb_function_family(instance,2,parent,1,1);
        assert((collection&((1u<<9)|(1u<<1)|(1u<<7)))==((1u<<9)|(1u<<1)|(1u<<7)));
    }
    API.set_param(instance,"dominant_minor_scale","Simplified Target");
    assert(hb_function_family(instance,2,parent,1,1)==hb_explicit_scale_mask(2,8));
    API.set_param(instance,"target_scale_minor","Natural Minor");
    assert(hb_function_family(instance,2,parent,1,1)==hb_explicit_scale_mask(2,8)); /* preserve A7 even with a custom minor baseline */
    API.destroy_instance(instance);
}
static void dominant_color_performance(void){
    Inst *instance=setup(),*other=API.create_instance("",0);
    API.set_param(instance,"dominant_scale","Simplified Target");
    API.set_param(instance,"dominant_minor_scale","Harmonic Minor");
    unsigned parent=hb_explicit_scale_mask(0,1);
    API.set_param(instance,"dominant_color","Down");
    assert(hb_policy_value(instance,HB_P_DOMINANT)==3&&hb_policy_value(instance,HB_P_DOMINANT_MINOR)==3);
    assert(hb_policy_value(other,HB_P_DOMINANT)==6);
    assert(hb_function_family(instance,2,parent,1,1)==hb_explicit_scale_mask(10,9));
    assert(hb_function_family(instance,2,parent,1,0)==hb_explicit_scale_mask(2,8));
    API.set_param(instance,"dominant_minor_scale","Melodic Minor");
    API.set_param(instance,"dominant_color","Up");
    assert(hb_policy_value(instance,HB_P_DOMINANT)==6&&hb_policy_value(instance,HB_P_DOMINANT_MINOR)==2);
    API.set_param(instance,"dominant_color","LatchOn");API.set_param(instance,"dominant_color","Down");API.set_param(instance,"dominant_color","Up");
    assert(hb_policy_value(instance,HB_P_DOMINANT)==3);
    API.set_param(instance,"dominant_color_family","Harmonic Major");
    assert(hb_policy_value(instance,HB_P_DOMINANT)==5);
    char state[32768],label[64];API.get_param(instance,"state",state,sizeof(state));
    API.set_param(instance,"state",state);API.get_param(instance,"dominant_color",label,sizeof(label));
    assert(!strcmp(label,"Off")&&instance->dominant_color_family==5);
    API.set_param(instance,"dominant_color","Down");hb_stop_instance_note_state(instance);
    assert(!instance->dominant_color_held&&!instance->dominant_color_latched);
    API.set_param(instance,"dominant_color","LatchOn");API.set_param(instance,"performance_reset","1");
    assert(!instance->dominant_color_latched);
    API.set_param(instance,"dominant_color","1");assert(instance->dominant_color_held);
    API.set_param(instance,"dominant_color","0");assert(!instance->dominant_color_held);
    API.destroy_instance(other);API.destroy_instance(instance);
}
static void diminished_destination(void){
    Inst *instance=setup();unsigned parent=hb_explicit_scale_mask(0,1);
    hb_cadence_step step={.kind=HB_CAD_DOMINANT};
    hb_cadence_result result=hb_resolve_cadence(instance,&step,71,parent);
    assert(result.root==70&&result.destination==71&&result.quality==9);
    assert(hb_relative_approach_offset(2,71,parent)==-1);
    assert(hb_relative_approach_offset(14,71,parent)!=-1); /* plain Fifth stays distinct */
    hb_cp_config config=instance->player.config;
    hb_harmony_t harmony={0};
    hb_approach_result approach=hb_resolve_chord_approach(instance,71,parent,config,harmony,2,0,0,0);
    assert(approach.root==70&&approach.config.quality==9&&approach.intent_kind==3);
    for(int target=60;target<=62;target+=2){result=hb_resolve_cadence(instance,&step,target,parent);assert(result.root==target-5&&result.quality==6);}
    API.destroy_instance(instance);
}
int main(void){diminished_destination();dominant_color_performance();dominant_family_contract();plain_and_dominant_degrees();
    source_and_quality();musical_output();cadence_and_dominant_family();persistence_and_track_isolation();
    puts("target scales: Auto/Parent/Simplified, quality families, MIDI, cadences, precedence and persistence pass");
}
