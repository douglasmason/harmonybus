#define main existing_motif_tests
#include "chord_player_test.c"
#undef main
static void legacy_library_migrates_once(void){
    Inst *instance=fixture();char legacy[4096]=";mf1,0,2,0,2,0,0,0:";
    int used=(int)strlen(legacy);
    used=hb_mt_write_hex(legacy,sizeof(legacy),used,1,2);
    for(int index=0;index<4;index++)used=hb_mt_write_hex(legacy,sizeof(legacy),used,0,8);
    used=hb_mt_write_hex(legacy,sizeof(legacy),used,5,2);
    used=hb_mt_write_hex(legacy,sizeof(legacy),used,4,2);
    const int pitches[]={60,62,60,67,60};
    for(int step=0;step<5;step++){
        unsigned fields[]={0,24,1,0,1,0,0,3,0xAB5,0};
        const int digits[]={1,3,1,1,1,2,2,1,3,4};
        for(int field=0;field<10;field++)used=hb_mt_write_hex(legacy,sizeof(legacy),used,fields[field],digits[field]);
        used=hb_mt_write_hex(legacy,sizeof(legacy),used,0,2);
        used=hb_mt_write_hex(legacy,sizeof(legacy),used,pitches[step],2);
        used=hb_mt_write_hex(legacy,sizeof(legacy),used,100,2);
    }
    for(int slot=1;slot<16;slot++)used=hb_mt_write_hex(legacy,sizeof(legacy),used,0,4);
    hb_mt_restore(instance,legacy);
    assert(g_motifs[0].body.count==3&&g_motifs[0].placement==3);
    assert(g_motifs[0].body.events[1].notes[0].pitch==60); /* Internal target survives. */
    hb_mt_definition saved=g_motifs[0];
    char current[8192];assert(hb_mt_save(instance,current,sizeof(current),0)>0);
    assert(!strncmp(current,";mf2,",5));
    memset(g_motifs,0,sizeof(g_motifs));g_motifs_restored=0;
    hb_mt_restore(instance,current);
    assert(!memcmp(&saved,&g_motifs[0],sizeof(saved)));
    /* Truncation anywhere in the library cannot replace any saved slot. */
    char *end=strstr(current,";mp1,");assert(end);
    for(int cut=0;cut<(int)(end-current);cut++){
        char truncated[8192];memcpy(truncated,current,(size_t)cut);truncated[cut]=0;
        g_motifs_restored=0;hb_mt_restore(instance,truncated);
        assert(!memcmp(&saved,&g_motifs[0],sizeof(saved)));
    }
    API.destroy_instance(instance);
}
static void boundary_ties_keep_recorded_indices(void){
    Inst *instance=fixture();
    hb_mt_phrase original={.count=7,.anchor=5};
    const int pitches[]={60,0,62,60,67,60,0};
    for(int index=0;index<7;index++){
        original.events[index]=(hb_mt_event){.kind=pitches[index]?0:2,.duration=6+index,
            .count=pitches[index]?1:0,.chord_mode=3,.scale=0xAB5};
        if(pitches[index])original.events[index].notes[0]=(hb_mt_note){pitches[index],100};
    }
    assert(hb_mt_normalize(&original,&g_motifs[0]));
    assert(g_motifs[0].body.count==3);
    char state[8192];assert(hb_mt_save(instance,state,sizeof(state),0)>0);
    memset(g_motifs,0,sizeof(g_motifs));g_motifs_restored=0;hb_mt_restore(instance,state);
    hb_mt_phrase restored;hb_mt_library_view(0,&restored);
    assert(restored.count==original.count&&restored.anchor==original.anchor);
    for(int index=0;index<7;index++){
        assert(restored.events[index].kind==original.events[index].kind);
        assert(restored.events[index].duration==original.events[index].duration);
        assert(restored.events[index].notes[0].pitch==original.events[index].notes[0].pitch);
    }
    /* The V still lives at token step 4, despite removing the opening I
       and its tie from canonical body storage. */
    assert(hb_ar_schedule(instance,60,0,35|(4<<6),100,0,0));
    int voices=0;
    for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used){
        assert(instance->motif.events[index].pitch==67);voices++;
    }
    assert(voices==1);API.destroy_instance(instance);
}
int main(void){legacy_library_migrates_once();boundary_ties_keep_recorded_indices();puts("Legacy motif migration, recorded step indices, normalized persistence and atomic truncation regressions pass");}
