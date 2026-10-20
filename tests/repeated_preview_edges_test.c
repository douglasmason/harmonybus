/* Repeated physical strikes must not become a hold when previews intervene. */
#define main chord_player_fixture_main
#include "chord_player_test.c"
#undef main
int main(void){
 const int release_gaps_ms[]={0,1,2,3,6,12};
 for(int gap=0;gap<6;gap++)for(int role=0;role<2;role++)for(int mode=0;mode<3;mode++)for(int preview=0;preview<2;preview++){
  Inst *instance=fixture();API.set_param(instance,"role",role?"Follower":"Conductor");
  instance->player.config.mode=mode;instance->player.config.playback=0;instance->player.config.latch=0;
  instance->boundary_buffer_ms=0;instance->follow_lookahead_ms=0;
  instance->surface_enabled=preview;instance->surface_count[0]=instance->surface_count[1]=32;
  for(int bank=0;bank<2;bank++)for(int slot=0;slot<32;slot++){
   instance->surface_notes[bank][slot]=36+bank*16+slot;
   instance->surface_targets[bank][slot]=-1;instance->surface_rows[bank][slot]=0;
  }
  char snapshot[8192];midi(instance,1,60);advance(instance,3,64);
  int voices=render_count;assert(voices>0);
  for(int strike=0;strike<40;strike++){
   render_count=0;midi(instance,0,60);
   if(preview)assert(API.get_param(instance,"surface_view0",snapshot,sizeof(snapshot))>0);
   if(release_gaps_ms[gap])advance(instance,release_gaps_ms[gap],64);
   midi(instance,1,60);
   if(preview)assert(API.get_param(instance,"surface_view1",snapshot,sizeof(snapshot))>0);
   advance(instance,3,64);
   assert(render_count==2*voices);
   int offs=0,ons=0;
   for(int event=0;event<render_count;event++){
    if((rendered[event][1]&0xf0)==0x80){assert(!ons);offs++;}
    else if((rendered[event][1]&0xf0)==0x90&&rendered[event][3])ons++;
   }
   assert(offs==voices&&ons==voices);
  }
  midi(instance,0,60);advance(instance,3,64);API.destroy_instance(instance);
 }
 puts("Repeated pad edges: 2880 OFF/ON pairs remain ordered at 0/1/2/3/6/12 ms release gaps across native surface previews, conductor/follower and raw/auto-chord modes");
}
