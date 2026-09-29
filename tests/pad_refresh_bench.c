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
}
