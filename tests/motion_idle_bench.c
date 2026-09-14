/* Informational desktop benchmark; not a device timing gate. */
#define main fixture_main
#include "chord_player_test.c"
#undef main
#include <time.h>
static Inst bank[16];
static double seconds(void){struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);return now.tv_sec+now.tv_nsec*1e-9;}
int main(void){
    fixture();transport=MOVE_CLOCK_STATUS_STOPPED;
    for(int track=0;track<16;track++)hb_mo_defaults(&bank[track].motion);
    double start=seconds();
    for(int block=0;block<20000;block++)for(int track=0;track<16;track++)hb_motion_tick_routes(&bank[track]);
    printf("16-instance idle motion routing: %.6f seconds / 20000 blocks\n",seconds()-start);
    return 0;
}
