/* Differential contract: the extracted core must match the pinned implementation. */
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "../core/include/harmony_core.h"
typedef hb_harmony_t (*infer_fn)(const uint8_t *, int);
typedef int (*map_fn)(int,int,hb_harmony_t,hb_map_mode_t);
int main(int argc,char **argv){
    assert(argc==2);
    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(library);
    infer_fn original_infer=(infer_fn)dlsym(library,"hb_infer_harmony");
    map_fn original_map=(map_fn)dlsym(library,"hb_map_note");assert(original_infer&&original_map);
    unsigned cases=0;
    for(unsigned mask=1;mask<4096;mask++){
        uint8_t notes[12];int count=0;
        for(int pc=0;pc<12;pc++)if(mask&(1u<<pc))notes[count++]=(uint8_t)(48+pc);
        hb_harmony_t expected=original_infer(notes,count),actual=hb_infer_harmony(notes,count);
        assert(!memcmp(&expected,&actual,sizeof(actual)));
        for(int note=0;note<128;note++)for(int mode=0;mode<3;mode++){
            assert(hb_map_note(note,0,actual,(hb_map_mode_t)mode)==original_map(note,0,expected,(hb_map_mode_t)mode));cases++;
        }
    }
    dlclose(library);printf("Core parity: 4095 pitch-class sets, %u mapping cases pass\n",cases);return 0;
}
