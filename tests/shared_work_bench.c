/* Serial desktop benchmark; does not model synth DSP or device deadlines. */
#define main chord_test_main
#include "chord_player_test.c"
#undef main
#include <time.h>
#include <stdlib.h>
static double clock_seconds(void){struct timespec stamp;clock_gettime(CLOCK_MONOTONIC,&stamp);return stamp.tv_sec+stamp.tv_nsec*1e-9;}
int main(int argc,char **argv){
 int tracks=argc>1?atoi(argv[1]):16,blocks=argc>2?atoi(argv[2]):10000;
 if(tracks<1||tracks>16||blocks<1){fprintf(stderr,"usage: %s [tracks 1..16] [positive blocks]\n",argv[0]);return 2;}
 Inst *instances[16];instances[0]=fixture();
 for(int track=1;track<tracks;track++)instances[track]=API.create_instance("",0);
 for(int track=0;track<tracks;track++){
  Inst *instance=instances[track];instance->movy_track=track;
  API.set_param(instance,"role",track<4?"Conductor":"Follower");
  API.set_param(instance,"chord_mode","Scale Degree");
  API.set_param(instance,"travel_map","Closest Split");
  g_movy_clips[track]=(hb_movy_clip_t){.present=1,.active=1,.running=1,.ppqn=96,.period=384,.slot=0,.revision=100+track};
 }
 g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=9,.source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(9,2)};
 hb_movy_refresh();
 double start=clock_seconds();
 for(int block=0;block<blocks;block++){
  position=block*128.0/44100*2;
  for(int track=0;track<tracks;track++)g_movy_clips[track].tick=(hb_tick_t)(position*96);
  char message[80];snprintf(message,sizeof(message),"%d,128,44100",block+1);
  API.set_param(instances[0],"hb_movy_block",message);
  for(int track=0;track<tracks;track++)API.tick(instances[track],128,44100,output,lengths,64);
 }
 double elapsed=clock_seconds()-start;
 for(int track=0;track<tracks;track++)API.destroy_instance(instances[track]);
 printf("%d tracks %d blocks: %.2f us/block\n",tracks,blocks,elapsed*1e6/blocks);
}
