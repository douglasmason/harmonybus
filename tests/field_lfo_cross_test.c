#include <assert.h>
#include <stdio.h>
#include "host/lfo_common.h"
int main(void){
    for(int source=0;source<2;source++)for(int bipolar=0;bipolar<2;bipolar++){
        lfo_state_t lfos[2]={0};int target=1-source;
        lfos[source].enabled=1;lfos[source].previous_valid=1;lfos[source].previous_signal=.5f;
        lfos[source].depth=.25f;lfos[source].bipolar=bipolar;
        strcpy(lfos[source].target,source?"lfo1":"lfo2");strcpy(lfos[source].param,"depth");
        lfos[target].depth=.2f;
        float base[2][3];
        for(int block=0;block<100;block++){
            lfo_cross_begin(lfos,base);
            float expected=.2f+(bipolar?.5f:.75f)*.25f*(bipolar?1:2);
            assert(fabsf(lfos[target].depth-expected)<1e-6f);
            lfo_cross_end(lfos,base);assert(lfos[target].depth==.2f);
        }
        lfos[source].enabled=0;lfo_cross_begin(lfos,base);assert(lfos[target].depth==.2f);lfo_cross_end(lfos,base);
        lfos[source].enabled=1;lfos[source].previous_valid=0;
        lfo_cross_begin(lfos,base);assert(lfos[target].depth==.2f);lfo_cross_end(lfos,base);
    }
    puts("cross-LFO bases, both directions, polarity, disable and stale-source clearing passed");
}
