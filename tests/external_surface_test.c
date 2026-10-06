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
    /* Session view may leave Move's last spatial preview cached. */
    API.set_param(instance,"pad_preview_inputs",lower);
    API.set_param(instance,"surface_enabled","1");
    instance->approach_rows.latch_slots=1;
    API.set_param(instance,"surface_preview0",move);
    assert(!instance->approach_layout);
    API.set_param(instance,"surface_preview0",lower);
    assert(instance->approach_layout&&!instance->approach_rows.latch_slots);
    API.set_param(instance,"surface_preview0",move);
    API.set_param(instance,"approach_touch_1","Down");
    assert(instance->approach_rows.down&1);
    API.set_param(instance,"approach_mode_active","0");
    assert(instance->approach_rows.down&1); /* A Copy/clip view change cannot steal the external hold. */
    API.set_param(instance,"approach_touch_1","Up,500");
    assert(!(instance->approach_rows.down&1));
    API.set_param(instance,"approach_control_1","LatchOn");
    API.set_param(instance,"approach_mode_active","1");
    assert(instance->approach_rows.latch&&instance->approach_rows.performance);
    /* Combined frames are byte-identical to independent canonical previews,
       including approach intent, octave duplicates, trails and voice travel. */
    char frame[32768],reference[3][8192];
    const char *keys[]={"pad_view","surface_view0","surface_view1"};
    for(int role=0;role<2;role++)for(int travel=0;travel<8;travel++)for(int root=0;root<12;root++){
        instance->role=role;instance->travel_map=travel;instance->trail_enabled=root%2;
        instance->content_map=root%9;hb_effective_write(chord(root,root%2,root%3==0));
        for(int index=0;index<32;index++){
            instance->preview_notes[index]=48+index;instance->preview_targets[index]=-1;instance->preview_rows[index]=0;
            for(int bank=0;bank<2;bank++){
                instance->surface_notes[bank][index]=36+bank*32+index;
                instance->surface_targets[bank][index]=-1;instance->surface_rows[bank][index]=0;
                if(index%8==3&&root%2){instance->surface_notes[bank][index]=-1;instance->surface_targets[bank][index]=48+index;instance->surface_rows[bank][index]=1+bank;}
            }
        }
        instance->preview_count=32;
        for(int index=0;index<3;index++)assert(API.get_param(instance,keys[index],reference[index],sizeof(reference[index]))>0);
        assert(API.get_param(instance,"surface_frame",frame,sizeof(frame))>0);
        char *part=frame;assert(!strncmp(part,"sf1\n",4));part+=4;
        for(int index=0;index<3;index++){
            char *end=strchr(part,'\n');if(end)*end=0;
            if(strcmp(part,reference[index])){fprintf(stderr,"frame mismatch role=%d travel=%d root=%d part=%d\n",role,travel,root,index);assert(0);}
            if(index<2){assert(end);part=end+1;}else assert(!end);
        }
        assert(!instance->surface_frame_cache);
        char tiny[12];assert(API.get_param(instance,"surface_frame",tiny,sizeof(tiny))<0);
        assert(!instance->surface_frame_cache&&instance->preview_notes[0]==48);
    }
    API.destroy_instance(instance);
    puts("external surfaces: canonical parity, independent banks, bounded errors, Move restoration pass");
}
