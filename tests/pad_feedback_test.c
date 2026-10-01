/* Preview chord expansion must not make chromatic approaches look diatonic.
   Short MIDI onsets need a separate, bounded visual lifetime. */
#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static unsigned section(const char *view,const char *key){
    const char *start=strstr(view,key);unsigned mask=0;assert(start);
    assert(sscanf(start+strlen(key),"%u",&mask)==1);return mask;
}
int main(void){
    Inst *instance=fixture();instance->travel_map=0;instance->content_map=1;instance->chromatic_map=1;
    hb_set_shared_follower_scale(1);g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);
    instance->approach_layout=1;API.set_param(instance,"approach_bank_1","Connector Below");
    for(int mode=0;mode<3;mode++)for(int target=60;target<72;target++){
        instance->player.config.mode=mode;
        Inst preview=*instance;int alias=target<64?target+36:target-36;
        preview.movy_pad_shift[alias]=target-alias;preview.approach_rows.preview_row=3;
        unsigned long long low=0,high=0;
        unsigned rendered=hb_pad_render_mask(&preview,instance,hb_render_harmony(instance),alias,1,&low,&high);
        unsigned scale=hb_follower_scale_target(instance,hb_render_harmony(instance)).pitch_mask;
        assert(!(rendered&~preview.preview_gap_mask)==!(rendered&~scale));
        if(target==64){assert(rendered==(1u<<3));assert(!(preview.preview_gap_mask&(1u<<3)));}
    }
    API.destroy_instance(instance);
    instance=fixture();instance->role=3;
    char request[80]="pad_view@",view[4096];
    for(int slot=0;slot<32;slot++)snprintf(request+9+slot*2,3,"%02x",48+slot);
    uint8_t messages[2][3]={{0x90,60,100},{0x80,60,0}};int lengths[2]={3,3};
    hb_pad_observe_output(instance,messages,lengths,2);
    assert(!instance->pad_sounding[60]);
    API.get_param(instance,request,view,sizeof(view));
    assert(section(view,"|playpads1,")==0);
    assert(section(view,"|playflash1,")==1u<<12);
    uint8_t output[64][3];int sizes[64];
    API.tick(instance,2400,48000,output,sizes,64);
    API.get_param(instance,request,view,sizeof(view));assert(section(view,"|playflash1,")==1u<<12);
    API.tick(instance,1440,48000,output,sizes,64);
    API.get_param(instance,request,view,sizeof(view));assert(section(view,"|playflash1,")==0);
    hb_pad_observe_output(instance,messages,lengths,2);
    uint8_t stop[1][3]={{0xfc,0,0}};int one[1]={1};hb_pad_observe_output(instance,stop,one,1);
    API.get_param(instance,request,view,sizeof(view));assert(section(view,"|playflash1,")==0);
    API.destroy_instance(instance);
    puts("Approach membership stays chromatic across chord modes; short onsets flash without sustaining MIDI and clear on stop");
}
