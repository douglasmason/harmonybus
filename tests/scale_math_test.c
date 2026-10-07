/* Exact arithmetic comparisons against the v0.2.268 implementations. */
#define main fixture_main
#include "follower_reference_test.c"
#undef main
static uint16_t reference_transpose_mask(uint16_t mask,int semitones){
    uint16_t shifted=0;
    for(int pitch=0;pitch<12;pitch++)if(mask&(1u<<pitch))shifted|=(uint16_t)(1u<<mod12(pitch+semitones));
    return shifted;
}
static uint16_t reference_accommodate_chord(uint16_t parent,hb_harmony_t harmony,int tonic){
    uint16_t chord=hb_harmony_chord_mask(harmony),result=parent;
    if(!parent)return chord;
    int root_degree=hb_source_degree_from_parent_scale(mod12(harmony.root_pc-tonic),tonic,parent);
    static const int interval_degree[12]={0,1,1,2,2,3,4,4,5,5,6,6};
    for(int interval=0;interval<12;interval++){
        int pitch=mod12(harmony.root_pc+interval);
        unsigned bit=1u<<pitch;
        if(!(chord&bit)||(parent&bit))continue;
        int role=interval_degree[interval];
        if(interval==8&&!(chord&(1u<<mod12(harmony.root_pc+7))))role=4; /* augmented fifth */
        if(interval==6&&(chord&(1u<<mod12(harmony.root_pc+7))))role=3; /* sharp eleventh */
        int degree=(root_degree+role)%7,seen=0,replaced=-1;
        /* Prefer the matching diatonic degree (e.g. D7 raises F, Fm lowers A).
           Altered fifths are fifths, not an inferred sharp fourth. */
        for(int offset=0;offset<12;offset++)if(parent&(1u<<mod12(tonic+offset))){
            if(seen++==degree){replaced=mod12(tonic+offset);break;}
        }
        if(replaced>=0&&!(chord&(1u<<replaced)))result&=(uint16_t)~(1u<<replaced);
        result|=bit;
    }
    return (uint16_t)(result|chord);
}

int main(void){
    unsigned transpose_cases=0,chord_cases=0;
    for(unsigned mask=0;mask<=65535;mask++)for(int shift=-24;shift<=24;shift++){
        assert(hb_transpose_mask((uint16_t)mask,shift)==reference_transpose_mask((uint16_t)mask,shift));
        transpose_cases++;
    }
    for(unsigned parent=0;parent<4096;parent++){
        unsigned subset=parent;
        do{
            hb_harmony_t harmony={.valid=1,.root_pc=parent%12,.chord_index=HB_HARMONY_EXPLICIT_TONES,.pitch_mask=subset};
            int tonic=subset%12;
            assert(hb_accommodate_chord(parent,harmony,tonic)==reference_accommodate_chord(parent,harmony,tonic));chord_cases++;
            if(!subset)break;
            subset=(subset-1)&parent;
        }while(1);
        /* Chromatic alterations still take the full degree-preserving path. */
        const unsigned chords[]={0x91,0x89,0x891,0x149};
        for(int root=0;root<12;root++)for(int form=0;form<4;form++){
            hb_harmony_t harmony={.valid=1,.root_pc=root,.chord_index=HB_HARMONY_EXPLICIT_TONES,
                .pitch_mask=reference_transpose_mask(chords[form],root)};
            int tonic=(root+parent)%12;
            assert(hb_accommodate_chord(parent,harmony,tonic)==reference_accommodate_chord(parent,harmony,tonic));chord_cases++;
        }
    }
    printf("Scale arithmetic: %u mask rotations and %u chord accommodations match v0.2.268\n",transpose_cases,chord_cases);
}
