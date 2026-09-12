/* Real MIDI API sequence: raw key before boundary, conductor chord on boundary. */
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../modules/harmonybus/dsp/harmonybus.c"
static double test_beat;
static double beat_position(void){return test_beat;}
static float bpm(void){return 120.0f;}
static int clock_status(void){return 2;}
static hb_global_shared_t globals;
static host_api_v1_t host={.sample_rate=48000,.get_bpm=bpm,.get_beat_position=beat_position,.get_clock_status=clock_status};
static uint8_t output[16][3];
static int lengths[16];
static void midi(void *instance,int status,int note,int velocity){
    uint8_t message[3]={(uint8_t)status,(uint8_t)note,(uint8_t)velocity};
    API.process_midi(instance,message,3,output,lengths,16);
}
static int advance(void *instance){return API.tick(instance,64,48000,output,lengths,16);}
static void chord(void *instance,int root,int on){
    midi(instance,on?0x90:0x80,root,on?100:0);
    midi(instance,on?0x90:0x80,root+4,on?100:0);
    midi(instance,on?0x90:0x80,root+7,on?100:0);
}
static void scenario(int follower_first,int captured,int staged,int travel){
    int source_note=travel?61:60;
    g_init=0;g_global_shared=&globals;memset(&globals,0,sizeof(globals));
    memset(g_movy_clips,0,sizeof(g_movy_clips));g_movy_present=0;g_movy_blocked=0;
    g_movy_revision=14695981039346656037ULL;
    move_midi_fx_init(&host);
    globals.follower_root_policy=2; // Explicit C source key for Relative mapping.
    globals.follower_explicit_root=0;
    void *first=API.create_instance("",NULL),*second=API.create_instance("",NULL);
    Inst *follower=follower_first?first:second,*conductor=follower_first?second:first;
    API.set_param(conductor,"role","Conductor");API.set_param(follower,"role","Follower");
    API.set_param(conductor,"source_channel","1");API.set_param(follower,"source_channel","1");
    follower->travel_map=travel;follower->content_map=0;follower->quant_timing=3;
    test_beat=0.0;chord(conductor,60,1);advance(conductor);
    assert(bus_read().root_pc==0);
    // Outside the pre-boundary window, keep immediate mapping to C.
    test_beat=0.8;midi(follower,0x90,source_note,100);
    assert(advance(follower)==1&&output[0][1]==60);
    test_beat=0.85;midi(follower,0x80,source_note,0);
    assert(advance(follower)==1&&output[0][1]==60);
    test_beat=captured?0.98:1.0;
    midi(follower,0x90,source_note,100);
    if(captured){assert(advance(follower)==0);assert(follower->mapped[source_note]==-1);}
    test_beat=1.0;
    chord(conductor,60,0);chord(conductor,62,1);
    if(staged)API.set_param(conductor,"hb_movy_block","1,64,48000");
    if(!follower_first)advance(conductor);
    int count=advance(follower);
    assert(count==1);
    assert(output[0][0]==0x90&&output[0][1]==62); // D harmony at release, not C at keypress
    assert(bus_read().root_pc==2);
    if(follower_first)advance(conductor);
    if(staged){
        int elapsed=conductor->committed_frames;
        API.set_param(follower,"hb_movy_block","1,64,48000");
        assert(conductor->committed_frames==elapsed); // one shared phase, not one per track
    }
    // Changing harmony after ON must not change the pitch of its paired OFF.
    test_beat=1.01;chord(conductor,62,0);chord(conductor,64,1);
    if(staged)API.set_param(conductor,"hb_movy_block","2,64,48000");
    advance(conductor);
    assert(bus_read().root_pc==4);
    test_beat=1.02;midi(follower,0x80,source_note,0);
    if(captured){assert(advance(follower)==0);test_beat=1.04;}
    count=advance(follower);
    assert(count==1&&output[0][0]==0x80&&output[0][1]==62);
    API.destroy_instance(first);API.destroy_instance(second);
}
int main(void){
    for(int travel=0;travel<2;travel++)for(int staged=0;staged<2;staged++)for(int order=0;order<2;order++)for(int capture=0;capture<2;capture++)scenario(order,capture,staged,travel);
    puts("follower_boundary_test: current-boundary harmony and paired note-offs pass in both callback orders");
}
