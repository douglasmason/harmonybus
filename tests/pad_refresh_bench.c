/* Informational desktop benchmark for synchronous full-pad reads.
 * Includes actual Auto Chord rendering; device timing must be measured separately. */
#define main ignored_main
#include "follower_reference_test.c"
#undef main
#include <time.h>
static double seconds(void){struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);return now.tv_sec+now.tv_nsec*1e-9;}
int main(void){
 Inst *instance=fixture();char request[80]="pad_view@",view[4096];
 hb_commit_observed_harmony(chord(0,0,0));
 for(int slot=0;slot<32;slot++)snprintf(request+9+slot*2,3,"%02x",48+slot);
 printf("Inst bytes %zu\n",sizeof(Inst));
 for(int mode=0;mode<=2;mode++)for(int travel=0;travel<8;travel++){
  instance->player.config.mode=mode;instance->travel_map=travel;
  double start=seconds();for(int i=0;i<50;i++)API.get_param(instance,request,view,sizeof(view));
  printf("mode %d travel %d us %.0f\n",mode,travel,(seconds()-start)*1e6/50);fflush(stdout);
 }
 API.destroy_instance(instance);
 /* Reproduce the reported expensive combinations, including alternating
    current/next previews over several registers with an active key change. */
 for(int key_active=0;key_active<2;key_active++)for(int mode=0;mode<2;mode++)for(int travel=0;travel<4;travel++){
  instance=fixture();instance->player.config.mode=mode;instance->travel_map=travel;
  instance->chromatic_map=1;instance->follower_split_map=1;instance->trail_enabled=0;
  g_pad_settings[0]=6;g_pad_next_pulse=0;
  g_key_context=(hb_key_context){.active=key_active,.source_root=0,.target_root=9,
   .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(9,2)};
  g_bus.next_model_locked=1;g_bus.next_model_count=2;g_bus.clip_loop_end=8;
  g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(2,1,0)};
  g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=4,.harmony=chord(7,0,1)};
  position=1;hb_commit_observed_harmony(g_bus.next_model[0].harmony);
  double cold_start=seconds();API.get_param(instance,request,view,sizeof(view));
  double cold_us=(seconds()-cold_start)*1e6;
#ifdef HB_CLOSEST_CACHE_SLOTS
  unsigned initial_solves=instance->closest_assignments.cursor;
#endif
  double start=seconds(),maximum=0;
  for(int iteration=0;iteration<50;iteration++){
   double before=seconds();API.get_param(instance,request,view,sizeof(view));
   double elapsed=seconds()-before;if(elapsed>maximum)maximum=elapsed;
  }
  printf("full-next key%d mode%d travel%d cold_us %.0f mean_us %.0f max_us %.0f",key_active,mode,travel,cold_us,(seconds()-start)*1e6/50,maximum*1e6);
#ifdef HB_CLOSEST_CACHE_SLOTS
  printf(" warm_solves %u",instance->closest_assignments.cursor-initial_solves);
  assert(instance->closest_assignments.cursor==initial_solves);
#endif
  puts("");fflush(stdout);API.destroy_instance(instance);
 }
}
