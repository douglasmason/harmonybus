/* Independent geometry must retain the exact production mapping and Move state. */
#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main

static void payload(char *buffer,int note,int target,int row,int spatial){
    for(int index=0;index<32;index++)snprintf(buffer+index*2,3,"%02x",(unsigned)(note<0?255:note)&255);
    buffer[64]=':';
    for(int index=0;index<32;index++)snprintf(buffer+65+index*2,3,"%02x",(unsigned)(target+1)&255);
    buffer[129]=':';
    for(int index=0;index<32;index++)buffer[130+index]=(char)('0'+row);
    snprintf(buffer+162,4,";%d",spatial);
}
int main(void){
    Inst *instance=fixture();hb_effective_write(chord(0,0,0));
    char move[166],lower[166],upper[166],before[8192],after[8192],external[8192],expected[8192];
    payload(move,60,-1,0,0);payload(lower,-1,64,1,1);payload(upper,76,-1,0,1);
    API.set_param(instance,"pad_preview_inputs",move);
    API.get_param(instance,"pad_view",before,sizeof(before));
    API.set_param(instance,"surface_enabled","1");
    API.set_param(instance,"surface_preview0",lower);API.set_param(instance,"surface_preview1",upper);
    assert(instance->approach_layout);assert(instance->preview_notes[0]==60);
    assert(API.get_param(instance,"surface_view0",external,sizeof(external))>0);
    assert(instance->preview_notes[0]==60&&instance->preview_targets[0]==-1&&instance->preview_count==32);
    /* The same geometry fed through Move gives byte-identical canonical output. */
    API.set_param(instance,"pad_preview_inputs",lower);
    API.get_param(instance,"pad_view",expected,sizeof(expected));assert(!strcmp(external,expected));
    API.set_param(instance,"pad_preview_inputs",move);
    assert(API.get_param(instance,"surface_view1",external,sizeof(external))>0);
    assert(instance->preview_notes[0]==60);
    API.set_param(instance,"pad_preview_inputs",upper);
    API.get_param(instance,"pad_view",expected,sizeof(expected));assert(!strcmp(external,expected));
    API.set_param(instance,"pad_preview_inputs",move);
    char small[2];API.get_param(instance,"surface_view0",small,sizeof(small));
    assert(instance->preview_notes[0]==60&&instance->preview_targets[0]==-1);
    API.set_param(instance,"surface_enabled","0");assert(!instance->approach_layout);
    API.get_param(instance,"pad_view",after,sizeof(after));assert(!strcmp(before,after));
    assert(API.get_param(instance,"surface_view0",external,sizeof(external))<0);
    /* Rejected payloads leave the last complete surface untouched. */
    API.set_param(instance,"surface_preview0","garbage");assert(instance->surface_targets[0][0]==64);
    API.destroy_instance(instance);
    puts("external surfaces: canonical parity, independent banks, bounded errors, Move restoration pass");
}
