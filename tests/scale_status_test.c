#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"

static hb_harmony_t explicit_chord(int root,unsigned mask){
    hb_harmony_t harmony={.valid=1,.root_pc=root,.chord_index=HB_HARMONY_EXPLICIT_TONES,
        .pitch_mask=mask,.detected_mask=mask};return harmony;
}
static unsigned resolved(Inst *instance,hb_harmony_t harmony,int *origin){
    int root=0;hb_resolve_follower_reference_root(instance,&root);
    unsigned parent=hb_explicit_scale_mask(root,hb_parent_scale_index(instance,harmony));
    unsigned mask=hb_output_chord_scale_resolved(instance,harmony,root,parent,g_bus.global_transpose,origin);
    assert(mask==hb_output_chord_scale(instance,harmony,root,parent));
    assert((mask|hb_harmony_chord_mask(harmony))==hb_follower_scale_target(instance,harmony).pitch_mask);
    return mask;
}
static void minor_tonic(void){
    for(int tonic=0;tonic<12;tonic++){
        Inst *instance=setup();int origin=-1;
        g_bus.next_model_locked=0;
        char root[8];snprintf(root,sizeof(root),"%d",tonic);
        API.set_param(instance,"follower_explicit_root",root);
        API.set_param(instance,"follower_scale","Natural Minor");
        API.set_param(instance,"gap_scale","Auto Local");
        hb_harmony_t triad=explicit_chord(tonic,(1u<<tonic)|(1u<<mod12(tonic+3))|(1u<<mod12(tonic+7)));
        API.set_param(instance,"target_scale_minor","Melodic Minor");
        API.set_param(instance,"target_scale_source","Auto");
        unsigned original=resolved(instance,triad,&origin);assert(origin==HB_SCALE_LOCAL);
        API.set_param(instance,"target_scale_source","Parent");assert(resolved(instance,triad,&origin)==original);
        API.set_param(instance,"target_scale_source","Simplified");
        assert(resolved(instance,triad,&origin)==hb_explicit_scale_mask(tonic,9));assert(origin==HB_SCALE_MINOR_TONIC);
        hb_harmony_t minor7=triad;minor7.pitch_mask|=1u<<mod12(tonic+10);minor7.detected_mask=minor7.pitch_mask;
        unsigned mask=resolved(instance,minor7,&origin);
        assert(mask==hb_explicit_scale_mask(tonic,3)); /* Explicit b7 stays b7. */
        API.set_param(instance,"target_scale_minor","Harmonic Minor");
        assert(resolved(instance,minor7,&origin)==hb_explicit_scale_mask(tonic,2));
        assert(resolved(instance,triad,&origin)==hb_explicit_scale_mask(tonic,8));
        API.set_param(instance,"gap_scale","Strict Local");assert(resolved(instance,triad,&origin)==original);
        assert(origin==HB_SCALE_LOCAL);
        API.destroy_instance(instance);
    }
}
static void context_and_parallel(void){
    Inst *instance=setup();int origin=-1;char status[512];
    API.set_param(instance,"gap_scale","Auto Local");API.set_param(instance,"target_scale_source","Simplified");
    API.set_param(instance,"target_scale_minor","Melodic Minor");
    hb_harmony_t minor=explicit_chord(0,tones(0,3,7,10));
    hb_harmony_t dominant=explicit_chord(7,tones(7,4,7,10));
    /* A minor chord at the end of a loop is not sufficient evidence. */
    API.set_param(instance,"follower_explicit_root","Eb");
    g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.harmony=explicit_chord(8,tones(8,4,7,11))};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.harmony=minor};
    resolved(instance,minor,&origin);assert(origin==HB_SCALE_LOCAL);
    g_bus.next_model[0].harmony=dominant;
    assert(resolved(instance,minor,&origin)==hb_explicit_scale_mask(0,3));assert(origin==HB_SCALE_MINOR_TONIC);
    /* ii of a major cadence retains that context, not Simple Minor. */
    hb_harmony_t supertonic=explicit_chord(2,tones(2,3,7,10));
    g_bus.next_model_count=3;g_bus.next_model[0].harmony=supertonic;g_bus.next_model[1].harmony=dominant;
    g_bus.next_model[2].harmony=explicit_chord(0,tones(0,4,7,11));
    resolved(instance,supertonic,&origin);assert(origin!=HB_SCALE_MINOR_TONIC);
    API.destroy_instance(instance);

    instance=setup();API.set_param(instance,"gap_scale","Parent");
    hb_commit_observed_harmony(dominant);
    API.set_param(instance,"parallel_scale","Melodic Minor");API.set_param(instance,"parallel_mode","Down");
    API.set_param(instance,"track_dominant_minor_scale","Lydian Dominant");
    API.get_param(instance,"scale_status",status,sizeof(status));
    assert(strstr(status,"G Lydian Dominant")&&strstr(status,"Dom minor")&&strstr(status,"C Melodic Minor (Parallel)"));
    API.set_param(instance,"dominant_color_family","Half-Whole Dim");API.set_param(instance,"dominant_color","Down");
    API.get_param(instance,"scale_status",status,sizeof(status));assert(strstr(status,"Dom Color")&&strstr(status,"Half-Whole Dim"));
    API.set_param(instance,"dominant_color","Up");API.set_param(instance,"parallel_mode","Up");
    API.get_param(instance,"scale_status",status,sizeof(status));assert(!strstr(status,"(Parallel)")&&!strstr(status,"Dom Color"));
    API.set_param(instance,"track_dominant_scale","Major");API.set_param(instance,"content_map","Raised-Avoid");
    API.get_param(instance,"scale_status",status,sizeof(status));assert(strstr(status,"Foll raise: C>C#"));
    API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Thirteenth");
    API.set_param(instance,"chord_extensions","Raise Avoid");
    API.get_param(instance,"scale_status",status,sizeof(status));assert(strstr(status,"Chord preview: C>C#"));
    char saved[32768];API.get_param(instance,"state",saved,sizeof(saved));
    API.set_param(instance,"state",saved);API.get_param(instance,"scale_status",status,sizeof(status));
    assert(strstr(status,"Foll raise: C>C#")&&strstr(status,"Chord preview: C>C#"));
    /* Small UI buffers terminate safely and report the required capacity. */
    char tiny[12];memset(tiny,'X',sizeof(tiny));int length=API.get_param(instance,"scale_status",tiny,sizeof(tiny));
    assert(length>=(int)sizeof(tiny)&&tiny[sizeof(tiny)-1]==0);
    API.destroy_instance(instance);
}
int main(void){minor_tonic();context_and_parallel();puts("Scale status: shared resolution, parallel precedence, tonic evidence and explicit chord preservation pass");return 0;}
