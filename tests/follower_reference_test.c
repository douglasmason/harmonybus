/* Input-role invariance and output transposition through the production mapper. */
#include <assert.h>
#include <math.h>
#include <sys/mman.h>
#include <unistd.h>
#ifndef HB_DSP_SOURCE
#define HB_DSP_SOURCE "../modules/harmonybus/dsp/harmonybus.c"
#endif
#include HB_DSP_SOURCE
static hb_global_shared_t globals;
static double position;
static float tempo=120;
static double beat(void){return position;}
static float bpm(void){return tempo;}
static int clock_status(void){return 2;}
static host_api_v1_t host={.sample_rate=48000,.get_beat_position=beat,.get_bpm=bpm,.get_clock_status=clock_status};
static Inst *fixture(void){
    g_init=0;g_global_shared=&globals;memset(&globals,0,sizeof(globals));
    g_movy_present=g_movy_blocked=0;g_monitor=0;
    move_midi_fx_init(&host);
    Inst *instance=API.create_instance("",0);
    API.set_param(instance,"role","Follower");
    API.set_param(instance,"follower_root_policy","Explicit");
    API.set_param(instance,"follower_explicit_root","C");
    return instance;
}
static hb_harmony_t chord(int root,int minor,int seventh){
    uint8_t notes[4]={(uint8_t)(60+root),(uint8_t)(60+root+(minor?3:4)),(uint8_t)(67+root),(uint8_t)(70+root)};
    return hb_infer_harmony(notes,seventh?4:3);
}
static void reference_invariance(void){
    Inst *instance=fixture();
    assert(instance->content_map==1);
    g_bus.global_transpose=2;hb_effective_write((hb_harmony_t){0});
    assert(hb_map_follower_note_now(instance,67)==69);
    g_bus.global_transpose=0;
    /* All input pitch classes, roots, explicit scales, harmonies, travel modes
       and transpositions. The displayed role must never depend on output. */
    for(int input_root=0;input_root<12;input_root++)for(int scale=1;scale<=9;scale++){
        globals.follower_explicit_root=input_root;hb_set_shared_follower_scale(scale);
        for(int pitch=60;pitch<72;pitch++){
            int expected=hb_source_degree_from_parent_scale(mod12(pitch-input_root),input_root,
                hb_explicit_scale_mask(input_root,scale?scale:1));
            for(int target_root=0;target_root<12;target_root++)for(int shift=-12;shift<=12;shift++){
                g_bus.global_transpose=shift;
                g_bus.observed_harmony=hb_transpose_harmony(chord(target_root,0,1),shift);
                hb_effective_write(hb_transpose_harmony(chord((target_root+5)%12,1,1),shift));
                for(int travel=0;travel<8;travel++){
                    instance->travel_map=travel;
                    assert(!strcmp(hb_follower_degree_role_for_note(instance,pitch),hb_role_name_for_degree(expected)));
                }
            }
        }
    }
    API.destroy_instance(instance);
}
static void transpose_equivariance(void){
    Inst *instance=fixture();
    for(int scale=0;scale<=9;scale++)for(int root=0;root<12;root++)for(int minor=0;minor<2;minor++){
        hb_set_shared_follower_scale(scale);
        hb_harmony_t harmony=chord(root,minor,1);
        for(int content=0;content<9;content++)for(int travel=0;travel<8;travel++)for(int split=0;split<4;split++){
            instance->content_map=content;instance->travel_map=travel;instance->follower_split_map=split;
            for(int pitch=60;pitch<72;pitch++){
                g_bus.global_transpose=0;hb_effective_write(harmony);
                int baseline=hb_map_follower_note_now(instance,pitch);
                for(int shift=-12;shift<=12;shift++){
                    g_bus.global_transpose=shift;hb_effective_write(hb_transpose_harmony(harmony,shift));
                    int actual=hb_map_follower_note_now(instance,pitch);
                    if(actual!=baseline+shift){
                        fprintf(stderr,"transpose mismatch: scale %d root %d minor %d content %d travel %d split %d input %d shift %d: %d vs %d\n",scale,root,minor,content,travel,split,pitch,shift,actual,baseline+shift);
                        assert(0);
                    }
                }
            }
        }
    }
    API.destroy_instance(instance);
}
static void transpose_last_at_limits(void){
    Inst *instance=fixture();
    API.set_param(instance,"travel_map","Closest Split 2");
    char label[64];API.get_param(instance,"travel_map",label,sizeof(label));
    assert(!strcmp(label,"Closest Split Chromatic"));
    const int pitches[]={0,1,7,12,60,67,115,120,126,127};
    for(int root=0;root<12;root++)for(int travel=0;travel<8;travel++){
        instance->travel_map=travel;hb_set_shared_follower_scale(1);
        hb_harmony_t harmony=chord(root,1,1);
        for(int index=0;index<10;index++){
            int pitch=pitches[index];g_bus.global_transpose=0;hb_effective_write(harmony);
            int baseline=hb_map_follower_note_now(instance,pitch);
            int role=mod12(baseline-harmony.root_pc);
            for(int shift=-24;shift<=24;shift++){
                g_bus.global_transpose=shift;
                hb_harmony_t shifted=hb_transpose_harmony(harmony,shift);hb_effective_write(shifted);
                int actual=hb_map_follower_note_now(instance,pitch);
                assert(actual>=0&&actual<=127);
                assert(mod12(actual-shifted.root_pc)==role);
                int expected=baseline+shift;
                while(expected<0)expected+=12;
                while(expected>127)expected-=12;
                assert(actual==expected);
            }
        }
    }
    API.destroy_instance(instance);
}
static void coherent_display_and_none(void){
    Inst *instance=fixture();
    API.set_param(instance,"travel_map","None");
    assert(instance->travel_map==7);
    char state[8192],display[256];API.get_param(instance,"state",state,sizeof(state));
    instance->travel_map=0;API.set_param(instance,"state",state);assert(instance->travel_map==7);
    for(int root=0;root<12;root++){
        hb_effective_write(chord(root,1,1));
        assert(hb_map_follower_note_now(instance,67)==67);
    }
    for(int travel=3;travel<=6;travel+=3){
        instance->travel_map=travel;
        for(int root=0;root<12;root++)for(int shift=-24;shift<=24;shift++){
            g_bus.global_transpose=shift;
            hb_harmony_t target=hb_transpose_harmony(chord(root,1,1),shift);
            hb_effective_write(target);
            memset(instance->published_follower,0,sizeof(instance->published_follower));
            instance->published_follower[67]=1;
            instance->mapped[67]=hb_map_follower_note_now(instance,67);
            instance->follower_path_harmony[67]=target;
            API.get_param(instance,"follower_snapshot",display,sizeof(display));
            assert(strstr(display,"|5th|")!=NULL);
            API.get_param(instance,"fpath_0_0_0",display,sizeof(display));
            /* A lower note arrives between the host's raw-note and role reads.
               The role must stay attached to the raw note already displayed. */
            instance->published_follower[60]=1;
            API.get_param(instance,"fpath_0_0_1",display,sizeof(display));
            assert(!strcmp(display,"5th"));
            API.get_param(instance,"follower_snapshot",display,sizeof(display));
            assert(strstr(display,"|Root|")!=NULL&&strstr(display,"|5th|")!=NULL);
        }
    }
    API.destroy_instance(instance);
}
static void harmony_flow(void){
    Inst *instance=fixture();
    g_bus.global_transpose=2;
    g_bus.observed_harmony=hb_transpose_harmony(chord(0,0,0),2);
    hb_effective_write(hb_transpose_harmony(chord(9,1,0),2));
    instance->render_harmony=hb_transpose_harmony(chord(9,1,0),2);instance->render_harmony_active=1;
    char snapshot[256],expected[256],detected[48],selected[48],rendered[48];
    hb_format_harmony(detected,48,chord(0,0,0));
    hb_format_harmony(selected,48,chord(9,1,0));
    hb_format_harmony(rendered,48,hb_transpose_harmony(chord(9,1,0),2));
    snprintf(expected,sizeof(expected),"hp1|--|%s|%s|%s",detected,selected,rendered);
    API.get_param(instance,"harmony_snapshot",snapshot,sizeof(snapshot));
    assert(!strcmp(snapshot,expected));
    API.get_param(instance,"hpath_0",snapshot,sizeof(snapshot));
    hb_effective_write(chord(7,0,1));
    API.get_param(instance,"hpath_3",snapshot,sizeof(snapshot));
    assert(!strcmp(snapshot,rendered)); /* one stock-host sweep retains its frame */
    g_bus.observed_harmony=(hb_harmony_t){0};instance->render_harmony_active=0;hb_effective_write((hb_harmony_t){0});
    API.get_param(instance,"harmony_snapshot",snapshot,sizeof(snapshot));
    assert(!strcmp(snapshot,"hp1|--|--|--|--"));
    API.destroy_instance(instance);
}
static void lookahead_disables_follower_buffer(void){
    Inst *instance=fixture();
    tempo=120;position=0.9;
    g_bus.quant_timing=3; /* quarter-note grid */
    instance->boundary_buffer_ms=100;
    instance->next_predict=1;instance->next_lookahead=14;
    const int guards[]={25,-3};
    for(int index=0;index<2;index++){
        instance->next_anti_buffer_ms=guards[index];
        for(int locked=0;locked<2;locked++){
            g_bus.next_model_locked=locked;
            assert(hb_effective_follower_buffer_for(instance)==0);
            assert(hb_follower_capture_beats_for(instance)==0.0);
            char value[64];API.get_param(instance,"boundary_buffer_ms",value,sizeof(value));
            assert(!strcmp(value,"0 ms"));
            instance->follower_queue_count=0;
            assert(hb_queue_follower_event(instance,67,100,1,0));
            assert(instance->follower_queue_target_beat[0]<0.0);
            assert(instance->boundary_buffer_ms==100);
        }
    }
    instance->next_anti_buffer_ms=0;
    assert(hb_effective_follower_buffer_for(instance)==100);
    instance->follower_queue_count=0;
    assert(hb_queue_follower_event(instance,67,100,1,0));
    assert(instance->follower_queue_target_beat[0]>position);
    instance->next_anti_buffer_ms=25;instance->next_lookahead=0;
    assert(hb_effective_follower_buffer_for(instance)==100);
    instance->next_lookahead=14;instance->next_predict=0;
    assert(hb_effective_follower_buffer_for(instance)==100);
    instance->boundary_buffer_ms=-3;
    assert(hb_follower_capture_beats_for(instance)>0.0);
    char value[64];API.get_param(instance,"boundary_buffer_ms",value,sizeof(value));
    assert(!strcmp(value,"1/16"));
    API.destroy_instance(instance);
}
static void anti_buffer(void){
    Inst *instance=fixture();
    g_bus.clip_loop_end=8;g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=chord(2,0,0)};
    g_bus.observed_harmony=chord(0,0,0);
    API.set_param(instance,"next_lookahead","3/4");
    for(int tempo_index=0;tempo_index<3;tempo_index++){
        tempo=60+60*tempo_index;
        const char *guards[]={"25 ms","100 ms","1/64","1/16","1/4","4 Bars"};
        for(int guard_index=0;guard_index<6;guard_index++){
            API.set_param(instance,"next_anti_buffer_ms",guards[guard_index]);
            double boundary=4-hb_next_shift_beats_for(instance);
            assert(boundary>1&&boundary<=4);
            for(int edge=0;edge<4;edge++){
                position=edge==0?1:edge==1?boundary-0.00001:edge==2?boundary:boundary+0.00001;
                hb_next_apply_effective(position);
                assert(hb_render_harmony(instance).root_pc==(edge<2?0:2));
                /* A large Follower Buffer cannot bypass the start guard. */
                instance->follower_queue_count=0;instance->boundary_buffer_ms=1000;
                assert(hb_queue_follower_event(instance,67,100,1,0));
                if(edge<2)assert(instance->follower_queue_harmony_beat[0]<0);
                assert(!strcmp(hb_follower_degree_role_for_note(instance,67),"5th"));
            }
        }
    }
    API.set_param(instance,"next_anti_buffer_ms","0 ms");
    position=1;hb_next_apply_effective(position);assert(hb_render_harmony(instance).root_pc==2);
    API.set_param(instance,"next_lookahead","-1/4");
    API.set_param(instance,"next_anti_buffer_ms","100 ms");
    position=4.9;hb_next_apply_effective(position);assert(hb_render_harmony(instance).root_pc==0);
    position=5;hb_next_apply_effective(position);assert(hb_render_harmony(instance).root_pc==2);
    API.set_param(instance,"next_lookahead","Off");
    hb_next_apply_effective(position);assert(hb_render_harmony(instance).root_pc==0);
    API.set_param(instance,"next_lookahead","3/4");
    API.set_param(instance,"next_anti_buffer_ms","1/16");
    API.set_param(instance,"pad_display","Both");
    API.set_param(instance,"motion_operation","Velocity");
    char saved[8192],restored[8192];API.get_param(instance,"state",saved,sizeof(saved));
    assert(strstr(saved,";la1,")&&strstr(saved,";pd1,"));
    API.destroy_instance(instance);
    instance=API.create_instance("",0);API.set_param(instance,"state",saved);
    assert(instance->next_lookahead==14&&instance->next_anti_buffer_ms==-3);
    API.get_param(instance,"state",restored,sizeof(restored));assert(!strcmp(saved,restored));
    API.set_param(instance,"next_anti_buffer_ms","50 ms");API.set_param(instance,"state",saved);
    assert(instance->next_anti_buffer_ms==50); /* stale per-track snapshot cannot undo global edit */
    API.destroy_instance(instance);
}
static void follower_rows(void){
    Inst *first=fixture(),*second=API.create_instance("",0);
    API.set_param(second,"role","Follower");
    first->published_follower[71]=first->follower_held[71]=1;
    second->published_follower[67]=second->follower_held[67]=1;
    first->mapped[71]=69;first->follower_path_harmony[71]=chord(2,0,0);
    second->mapped[67]=67;second->follower_path_harmony[67]=chord(9,1,1);
    char display[32];
    API.get_param(first,"fpath_1_0_1",display,sizeof(display));assert(!strcmp(display,"7th"));
    API.get_param(first,"fpath_1_1_1",display,sizeof(display));assert(!strcmp(display,"5th"));
    API.get_param(first,"fpath_1_1_2",display,sizeof(display));assert(!strcmp(display,"7th"));
    API.get_param(first,"fpath_1_1_3",display,sizeof(display));assert(!strcmp(display,"G4"));
    /* Harmony can change while a note remains held without retrigger: report
       the harmony used at onset, not the new bus harmony. */
    hb_effective_write(chord(0,0,0));
    API.get_param(first,"fpath_1_1_2",display,sizeof(display));assert(!strcmp(display,"7th"));
    /* Two tracks may play the same pitch. Keep their independent paths. */
    first->published_follower[67]=1;first->mapped[67]=74;
    first->follower_path_harmony[67]=chord(7,0,0);
    API.get_param(first,"fpath_1_0_1",display,sizeof(display));assert(!strcmp(display,"5th"));
    API.get_param(first,"fpath_1_2_1",display,sizeof(display));assert(!strcmp(display,"5th"));
    API.get_param(first,"fpath_1_0_3",display,sizeof(display));assert(!strcmp(display,"D5"));
    API.get_param(first,"fpath_1_2_3",display,sizeof(display));assert(!strcmp(display,"G4"));
    API.get_param(first,"fpath_0_3_0",display,sizeof(display));assert(!strcmp(display,"--"));
    second->mapped[67]=-1;
    API.get_param(second,"fpath_0_0_0",display,sizeof(display));
    API.get_param(second,"fpath_0_0_3",display,sizeof(display));assert(!strcmp(display,"--"));
    API.destroy_instance(first);API.destroy_instance(second);
}
static void unchanged_pitch_updates_output_role(void){
    Inst *instance=fixture();
    API.set_param(instance,"travel_map","None");
    instance->published_follower[71]=1;
    instance->follower_sounding[71]=1;
    instance->follower_held[71]=1;
    instance->follower_velocity[71]=100;
    instance->mapped[71]=71;
    hb_harmony_t previous=chord(9,1,0);
    hb_effective_write(previous);
    instance->follower_path_harmony[71]=previous;
    instance->follower_bus_seq=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);
    char display[256];uint8_t output[16][3];int lengths[16];
    API.get_param(instance,"follower_snapshot",display,sizeof(display));
    assert(strstr(display,"|7th|2nd|B4|")!=NULL);
    hb_effective_write(chord(2,1,0));
    assert(hb_reharmonize_held_follower(instance,output,lengths,16)==0);
    assert(instance->mapped[71]==71);
    API.get_param(instance,"follower_snapshot",display,sizeof(display));
    assert(strstr(display,"|7th|6th|B4|")!=NULL);
    /* Reharmonization disabled preserves the original note-on context. */
    instance->retrigger_held=0;
    hb_effective_write(previous);
    assert(hb_reharmonize_held_follower(instance,output,lengths,16)==0);
    API.get_param(instance,"follower_snapshot",display,sizeof(display));
    assert(strstr(display,"|7th|6th|B4|")!=NULL);
    API.destroy_instance(instance);
}

static void direct_follower_input_owns_its_display(void){
    Inst *instance=fixture();
    API.set_param(instance,"source_channel","1");
    API.set_param(instance,"boundary_buffer_ms","0 ms");
    API.set_param(instance,"travel_map","None");
    hb_effective_write(chord(9,1,0));
    hb_monitor_shared_t monitor;memset(&monitor,0,sizeof(monitor));
    monitor.magic=HB_MONITOR_MAGIC;monitor.version=HB_MONITOR_VERSION;
    monitor.velocities[0][58]=100; /* stale A#3 from the auxiliary monitor */
    g_monitor=&monitor;
    uint8_t output[16][3];int lengths[16];
    API.tick(instance,64,48000,output,lengths,16);
    uint8_t message[3]={0x90,67,100};
    API.process_midi(instance,message,3,output,lengths,16);
    API.tick(instance,64,48000,output,lengths,16);
    char display[256];API.get_param(instance,"follower_snapshot",display,sizeof(display));
    assert(!strcmp(display,"fp1|G4|5th|7th|G4|--|--|--|--"));
    assert(!instance->follower_held[58]);
    assert(instance->follower_held[67]);
    /* A delayed monitor note-off must not resurrect a released direct note. */
    monitor.velocities[0][67]=100;
    message[0]=0x80;message[2]=0;
    API.process_midi(instance,message,3,output,lengths,16);
    API.tick(instance,64,48000,output,lengths,16);
    API.get_param(instance,"follower_snapshot",display,sizeof(display));
    assert(!strcmp(display,"fp1|--|--|--|--|--|--|--|--"));
    g_monitor=0;API.destroy_instance(instance);
}

int main(void){lookahead_disables_follower_buffer();direct_follower_input_owns_its_display();unchanged_pitch_updates_output_role();harmony_flow();coherent_display_and_none();transpose_last_at_limits();follower_rows();reference_invariance();transpose_equivariance();anti_buffer();puts("follower reference, transpose, anti-buffer and state regressions pass");}
