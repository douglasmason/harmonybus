/* Lookahead LEDs predict the input keys that work when the chord arrives. */
#define main reference_suite_main
#include "follower_reference_test.c"
#undef main

static void default_and_saved_colors(void){
    Inst *instance=fixture();char bare[8192],saved[8192],value[128];
    API.get_param(instance,"pad_state_base",bare,sizeof(bare));
    API.set_param(instance,"state",bare); /* Movy's bare factory presets. */
    API.get_param(instance,"pad_display",value,sizeof(value));assert(!strcmp(value,"Both Full Lookahead"));
    API.get_param(instance,"pad_current_color",value,sizeof(value));assert(!strcmp(value,"Yellow"));
    API.get_param(instance,"pad_play_color",value,sizeof(value));assert(!strcmp(value,"Green"));
    API.get_param(instance,"pad_tonic_color",value,sizeof(value));assert(!strcmp(value,"Grey"));
    API.get_param(instance,"state",saved,sizeof(saved));
    hb_pad_defaults();API.set_param(instance,"state",saved);
    API.get_param(instance,"pad_both_color",value,sizeof(value));assert(!strcmp(value,"Orange"));
    API.get_param(instance,"pad_display",value,sizeof(value));assert(!strcmp(value,"Both Full Lookahead"));
    /* 0.2.176 omitted default pad settings and Blend from its saved state. */
    strcat(bare,";pc2,4;pp1,8");
    hb_pad_defaults();API.set_param(instance,"state",bare);
    API.get_param(instance,"pad_display",value,sizeof(value));assert(!strcmp(value,"Effective"));
    API.get_param(instance,"pad_lookahead_color",value,sizeof(value));assert(!strcmp(value,"Yellow"));
    API.get_param(instance,"pad_current_color",value,sizeof(value));assert(!strcmp(value,"Cyan"));
    API.get_param(instance,"pad_both_color",value,sizeof(value));assert(!strcmp(value,"Blend"));
    API.get_param(instance,"pad_play_color",value,sizeof(value));assert(!strcmp(value,"Track"));
    API.get_param(instance,"state",saved,sizeof(saved));
    hb_pad_defaults();API.set_param(instance,"state",saved);
    API.get_param(instance,"pad_both_color",value,sizeof(value));assert(!strcmp(value,"Blend"));
    API.destroy_instance(instance);
}

static void arrival_membership(void){
    Inst *instance=fixture();
    hb_set_shared_follower_scale(1);
    instance->boundary_buffer_ms=0;
    instance->next_anti_buffer_ms=0;
    g_bus.clip_loop_end=4;
    g_bus.next_model_locked=1;g_bus.next_model_count=2;
    char before[2048],after[2048];
    for(int travel=0;travel<8;travel++)for(int split=0;split<4;split++)
    for(int content=0;content<3;content++)for(int mode=0;mode<3;mode++)
    for(int transition=0;transition<3;transition++)for(int approach=0;approach<3;approach++){
        instance->travel_map=travel;instance->follower_split_map=split;
        instance->content_map=content;instance->player.config.mode=mode;
        instance->approach_control=approach;
        instance->next_lookahead=0;
        g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
        g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(transition==0?2:transition==1?5:9,1,transition==2)};
        for(int wrap=0;wrap<2;wrap++){
            int destination=wrap?0:1;
            position=wrap?3.5:1.5;
            g_bus.observed_harmony=g_bus.next_model[wrap?1:0].harmony;
            hb_effective_write(g_bus.observed_harmony);
            Inst unchanged=*instance;
            hb_harmony_t bus_before=bus_read();
            API.get_param(instance,"pad_view",before,sizeof(before));
            assert(!memcmp(&unchanged,instance,sizeof(unchanged)));
            assert(hb_harmony_equal_effective(bus_before,bus_read()));
            const char *full=strstr(before,"|full1,");assert(full);
            int known;unsigned predicted;
            assert(sscanf(full,"|full1,%d,%u",&known,&predicted)==2&&known);

            /* Advance the real bus and transport, then ask what inputs work
               now. This uses the production player, including auto chords. */
            position=wrap?4.0:2.0;
            g_bus.observed_harmony=g_bus.next_model[destination].harmony;
            hb_effective_write(g_bus.observed_harmony);
            API.get_param(instance,"pad_view",after,sizeof(after));
            unsigned current,actual;
            assert(sscanf(after,"%u,%u",&current,&actual)==2);
            if(predicted!=actual){
                fprintf(stderr,"arrival mismatch travel=%d split=%d content=%d mode=%d approach=%d transition=%d wrap=%d: %u != %u\n",
                    travel,split,content,mode,approach,transition,wrap,predicted,actual);
                assert(predicted==actual);
            }
            if(!mode&&approach==HB_APPROACH_OFF){
                unsigned chord_mask=hb_harmony_chord_mask(g_bus.observed_harmony);
                for(int pitch=60;pitch<72;pitch++){
                    int rendered=hb_map_follower_note_now(instance,pitch);
                    assert(!!(predicted&(1u<<mod12(pitch)))==!!(chord_mask&(1u<<mod12(rendered))));
                }
            }
            /* Configured lookahead uses the same target-specific mapping. */
            position=wrap?3.5:1.5;instance->next_lookahead=4;
            g_bus.observed_harmony=g_bus.next_model[wrap?1:0].harmony;
            hb_effective_write(g_bus.observed_harmony);
            API.get_param(instance,"pad_view",before,sizeof(before));
            unsigned scale,lookahead;int ready;
            assert(sscanf(before,"%u,%u,%u,%d,%u",&current,&actual,&scale,&ready,&lookahead)==5);
            assert(ready&&lookahead==predicted);
            instance->next_lookahead=0;
        }
    }
    API.destroy_instance(instance);
}

int main(void){
    default_and_saved_colors();
    arrival_membership();
    puts("pad arrival: all travel modes, split modes, content, chord generation, approaches, and wrap pass");
    return 0;
}
