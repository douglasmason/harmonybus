/* Compare tonal harmony with non-diatonic parallel extended chords.
   Use the production root-establishment + color-refinement path explicitly. */
#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
static hb_harmony_t extended(int root,int quality){
    static const int intervals[3][6]={{0,2,3,5,7,10},{0,2,4,7,11,-1},{0,2,4,7,10,-1}};
    uint8_t notes[6];int count=quality==0?6:5;
    for(int n=0;n<count;n++)notes[n]=48+root+intervals[quality][n];
    hb_harmony_t established=chord(root,quality==0,quality!=1);
    hb_harmony_t h=hb_refine_harmony_with_root(notes,count,established);
    assert(h.valid&&h.root_pc==root);
    uint16_t expected=0;for(int n=0;n<count;n++)expected|=1u<<mod12(notes[n]);
    assert(hb_harmony_chord_mask(h)==expected);
    return h;
}
static void print_mask(uint16_t mask){
    for(int pc=0;pc<12;pc++)if(mask&(1u<<pc))printf(" %s",hb_pc_name(pc));
}
static void run(const char *name,const int roots[4],const int qualities[4],int tonal){
    Inst *i=fixture();hb_next_reset_knowledge();
    API.set_param(i,"follower_scale","Major");
    API.set_param(i,"dominant_scale","Off");API.set_param(i,"borrowed_scale","Minimal");
    i->content_map=1;i->boundary_buffer_ms=0;i->next_anti_buffer_ms=0;
    hb_harmony_t harmonies[4];uint16_t masks[4],union_mask=0;
    for(int event=0;event<4;event++){
        harmonies[event]=extended(roots[event],qualities[event]);
        masks[event]=hb_follower_scale_target(i,harmonies[event]).pitch_mask;
        uint16_t chord_mask=hb_harmony_chord_mask(harmonies[event]);
        assert((masks[event]&chord_mask)==chord_mask);
        union_mask|=chord_mask;
        if(tonal)assert(masks[event]==hb_explicit_scale_mask(0,1));
        /* Learn actual chord events, including their extensions. */
        hb_commit_observed_harmony(harmonies[event]);next_pending_phase=event*4;
        hb_next_record_observed(harmonies[event]);
        printf("%s / %s:",name,harmonies[event].name);print_mask(masks[event]);puts("");
    }
    if(!tonal){
        int pcs=0;for(int pc=0;pc<12;pc++)pcs+=!!(union_mask&(1u<<pc));
        assert(pcs>7); /* Cannot all belong to any single seven-note parent. */
        for(int event=1;event<4;event++)assert(masks[event]!=masks[event-1]);
    }
    g_bus.clip_loop_end=16;hb_next_promote_learning();
    assert(g_bus.next_model_locked&&g_bus.next_model_count==4);
    for(int event=0;event<4;event++){
        assert(hb_harmony_chord_mask(g_bus.next_model[event].harmony)==hb_harmony_chord_mask(harmonies[event]));
        g_bus.observed_harmony=harmonies[event];
        /* One beat early: before/after render edge and actual chord boundary. */
        API.set_param(i,"next_lookahead","1/4");
        for(int edge=0;edge<3;edge++){
            position=event*4+(edge==0?2.99:edge==1?3.01:4.0);
            hb_next_apply_effective(position);
            hb_harmony_t render=hb_render_harmony(i);
            int expected=(event+(edge>0))%4;
            assert(hb_follower_input_scale(i,0)==hb_explicit_scale_mask(0,1));
            assert(render.root_pc==harmonies[expected].root_pc);
            assert(hb_harmony_chord_mask(render)==hb_harmony_chord_mask(harmonies[expected]));
            char view[8192];unsigned cur,eff,scale;
            assert(API.get_param(i,"pad_harmony",view,sizeof(view))>0);
            assert(sscanf(view,"%u,%u,%u",&cur,&eff,&scale)==3);
            assert(eff==hb_harmony_chord_mask(render)&&scale==masks[expected]);
            for(int travel=0;travel<5;travel++){
                i->travel_map=travel;i->chromatic_map=0;
                API.get_param(i,"pad_view",view,sizeof(view));
                assert(sscanf(view,"%u,%u,%u",&cur,&eff,&scale)==3);
                unsigned full;const char *full_field=strstr(view,"|full1,1,");
                assert(full_field&&sscanf(full_field,"|full1,1,%u",&full)==1);
                Inst preview=*i;preview.render_harmony_active=1;
                preview.render_harmony=harmonies[(event+(edge==2?2:1))%4];
                for(int pc=0;pc<12;pc++){
                    int next_output=hb_map_follower_note_now(&preview,60+pc);
                    assert(!!(full&(1u<<pc))==!!(hb_harmony_chord_mask(preview.render_harmony)&(1u<<mod12(next_output))));
                    int output=hb_map_follower_note_now(i,60+pc);
                    assert(!!(eff&(1u<<pc))==!!(hb_harmony_chord_mask(render)&(1u<<mod12(output))));
                    assert(!!(scale&(1u<<pc))==!!(masks[expected]&(1u<<mod12(output))));
                }
                for(int note=48;note<84;note++){
                    int output=hb_map_follower_note_now(i,note);
                    assert(masks[expected]&(1u<<mod12(output)));
                }
                i->chromatic_map=1;
                static const int degrees[]={0,2,4,5,7,9,11};
                for(int degree=0;degree<7;degree++){
                    int target=60+degrees[degree],alias=target<64?target+36:target-36;
                    int output=hb_map_follower_note_now(i,target);
                    i->movy_pad_shift[alias]=target-alias;
                    assert(hb_map_follower_note_now(i,alias)==output-1);
                    i->movy_pad_shift[alias]=0;
                }
            }
        }
    }
    API.destroy_instance(i);
}
int main(void){
    const int tonal_roots[]={2,7,0,9},tonal_qualities[]={0,2,1,0};
    const int parallel_roots[]={0,1,4,3},minor[]={0,0,0,0},major[]={1,1,1,1};
    run("ii-V-I-vi",tonal_roots,tonal_qualities,1);
    run("Parallel minor 11",parallel_roots,minor,0);
    run("Parallel major 9",parallel_roots,major,0);
    puts("PASS: chord preservation, changing collections, learned loop, early rendering, pad scale, five travels and approach pads");
}
