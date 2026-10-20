#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static int input(Inst *i,int source,int on){uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];int lengths[64],note=-1;int count=API.process_midi(i,message,3,output,lengths,64);for(int n=0;n<count;n++)if(lengths[n]==3&&(output[n][0]&0xf0)==0x90&&output[n][2])note=output[n][1];count=API.tick(i,64,48000,output,lengths,64);for(int n=0;n<count;n++)if(lengths[n]==3&&(output[n][0]&0xf0)==0x90&&output[n][2])note=output[n][1];return note;}
static Inst *setup(void){Inst *i=fixture();i->travel_map=0;i->content_map=1;i->chromatic_map=1;i->boundary_buffer_ms=0;i->next_anti_buffer_ms=0;hb_set_shared_follower_scale(1);g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);return i;}
static void touch(Inst *i,const char *key){API.set_param(i,key,"Down");API.set_param(i,key,"Up,50");}

static unsigned lights(Inst *i){char v[4096];API.get_param(i,"pad_view",v,sizeof(v));unsigned m=0;const char *p=strstr(v,"|playpads1,");assert(p&&sscanf(p,"|playpads1,%u",&m)==1);return m;}
static void settle(Inst *i){uint8_t out[64][3];int sizes[64];position+=4;API.tick(i,96000,48000,out,sizes,64);}
int main(void){
 /* Every ordinary key must own its sounding light through every travel mode,
    including chromatic travel and non-C conductor harmony. */
 for(int travel=0;travel<8;travel++)for(int chrom=0;chrom<2;chrom++)for(int root=0;root<12;root++){
 Inst *i=setup();i->travel_map=travel;i->chromatic_map=chrom;i->approach_rows.enabled=1;g_bus.observed_harmony=chord(root,0,0);hb_effective_write(g_bus.observed_harmony);
 char payload[65];for(int n=0;n<32;n++)sprintf(payload+2*n,"%02x",48+n);API.set_param(i,"pad_preview_inputs",payload);
 for(int n=0;n<32;n++){int pitch=input(i,48+n,1);assert(pitch>=0);assert(lights(i)&(1u<<n));input(i,48+n,0);assert(!lights(i));}
 API.destroy_instance(i);
 }
 for(int row=0;row<4;row++){
 Inst *i=setup();i->approach_rows.enabled=1;
 char payload[166],before[4096],after[4096],param[40];
 for(int n=0;n<32;n++)sprintf(payload+2*n,"%02x",n<8?60+n:255);
 payload[64]=':';for(int n=0;n<32;n++)sprintf(payload+65+2*n,"%02x",n==8?61:0);
 payload[129]=':';for(int n=0;n<32;n++)payload[130+n]=n==8&&row<3?'1'+row:'0';strcpy(payload+162,";1");
 API.set_param(i,"pad_preview_inputs",payload);
 touch(i,"approach_touch_1");assert(!i->approach_rows.performance);
 API.get_param(i,"pad_render",before,sizeof(before));
 int alias=row<3?(60+32*(row+1))%128:96;snprintf(param,sizeof(param),"%d,%d,%d",alias,60-alias,row);
 int first=-1;
 for(int n=0;n<3;n++){
 API.set_param(i,"hb_movy_input_approach",param);int pitch=input(i,alias,1);assert(pitch>=0);if(n)assert(pitch==first);else first=pitch;
 assert(lights(i)&(1u<<8));input(i,alias,0);settle(i);assert(!lights(i));
 API.get_param(i,"pad_render",after,sizeof(after));assert(!strcmp(before,after));
 assert(input(i,60,1)==60);assert(lights(i)&1);input(i,60,0);settle(i);
 }
 API.destroy_instance(i);
 }
 puts("Stable spatial approaches: repeated pitch, independent target, unchanged colors, matching play light; ordinary keys across travel/chromatic/harmony pass");
 return 0;
}
