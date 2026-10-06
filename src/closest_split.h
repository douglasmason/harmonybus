#ifndef HARMONYBUS_CLOSEST_SPLIT_H
#define HARMONYBUS_CLOSEST_SPLIT_H
#define HB_CLOSEST_SPLIT_DEGREES 7
#define HB_CLOSEST_SPLIT_SEARCH_RADIUS 6
#define HB_CLOSEST_MAX_INPUTS 12
static inline int hb_cs_mod12(int value){value%=12;return value<0?value+12:value;}
static inline int hb_cs_abs(int value){return value<0?-value:value;}
/* Nearest legal octave of one pitch class, with the same lower-note tie as
   the former exhaustive MIDI scan. Nominals may sit outside MIDI bounds. */
static inline int hb_cs_nearest_pc(int nominal,int pc){
    int below=nominal-hb_cs_mod12(nominal-pc),above=below+12;
    if(below<0)return pc;
    if(above>127)return pc+12*((127-pc)/12);
    return nominal-below<=above-nominal?below:above;
}
static inline int hb_cs_nearest(int nominal,unsigned pitch_mask){
    int best=-1,distance=1000000;
    for(int pc=0;pc<12;pc++)if(pitch_mask&(1u<<pc)){
        int pitch=hb_cs_nearest_pc(nominal,pc),next=hb_cs_abs(pitch-nominal);
        if(next<distance||(next==distance&&(best<0||pitch<best))){best=pitch;distance=next;}
    }
    return best;
}
/* A bounded joint assignment balances motion, register and collisions.
   A new pitch class earns two semitones of total-motion credit; spilling past
   the source range adds half a semitone of cost per semitone outside. Thus
   diversity cannot force large jumps. Allowed pools and the six-semitone
   travel bound remain hard constraints. Octave copies earn no diversity credit.
   Deterministic ties keep the mapping independent of performance order. */
static inline int hb_build_closest_assignment(int count,const int *nominal,
        const unsigned *allowed,int *outputs){
    if(count<1||count>HB_CLOSEST_MAX_INPUTS)return 0;
    int low=nominal[0],high=nominal[0],candidate[12][12],cost[12][12];
    for(int row=0;row<count;row++){if(nominal[row]<low)low=nominal[row];if(nominal[row]>high)high=nominal[row];}
    for(int row=0;row<count;row++){
        if(!(allowed[row]&4095u))return 0;
        for(int pc=0;pc<12;pc++){
            int pitch=hb_cs_nearest_pc(nominal[row],pc);
            candidate[row][pc]=pitch;
            int distance=hb_cs_abs(pitch-nominal[row]);
            int spill=pitch<low?low-pitch:pitch>high?pitch-high:0;
            cost[row][pc]=!(allowed[row]&(1u<<pc))||distance>6?10000000:spill*5+distance*10;
        }
    }
    int columns=count*12,owner[145]={0},way[145]={0},potential_row[13]={0},potential_column[145]={0};
    for(int row=1;row<=count;row++){
        owner[0]=row;int column=0,minimum[145];unsigned char used[145]={0};
        for(int index=1;index<=columns;index++)minimum[index]=100000000;
        do{
            used[column]=1;int active=owner[column],delta=100000000,next=0;
            for(int index=1;index<=columns;index++)if(!used[index]){
                int pc=(index-1)/count,slot=(index-1)%count;
                int value=cost[active-1][pc]-(slot==0?20:0)-potential_row[active]-potential_column[index];
                if(value<minimum[index]){minimum[index]=value;way[index]=column;}
                if(minimum[index]<delta){delta=minimum[index];next=index;}
            }
            for(int index=0;index<=columns;index++)if(used[index]){potential_row[owner[index]]+=delta;potential_column[index]-=delta;}else minimum[index]-=delta;
            column=next;
        }while(owner[column]);
        do{int previous=way[column];owner[column]=owner[previous];column=previous;}while(column);
    }
    for(int column=1;column<=columns;column++)if(owner[column]){
        int row=owner[column]-1,pc=(column-1)/count;
        if(cost[row][pc]>=10000000)return 0;
        outputs[row]=candidate[row][pc];
    }
    return 1;
}
static inline int hb_build_closest_split_assignment(const int nominal[7],const unsigned allowed[7],int outputs[7]){
    return hb_build_closest_assignment(7,nominal,allowed,outputs);
}
/* Alternate current/next harmony previews and octave rows must not evict one
   another's assignments. Cache exact solver inputs; never approximate a pitch. */
#define HB_CLOSEST_CACHE_SLOTS 16
typedef struct {int count,nominal[12],output[12];unsigned allowed[12];} hb_closest_cache_entry;
typedef struct {hb_closest_cache_entry entries[HB_CLOSEST_CACHE_SLOTS];unsigned cursor;} hb_closest_cache;
static inline int hb_cached_closest_assignment(hb_closest_cache *cache,int count,
        const int *nominal,const unsigned *allowed,int *outputs){
    if(count<1||count>HB_CLOSEST_MAX_INPUTS)return 0;
    for(int slot=0;slot<HB_CLOSEST_CACHE_SLOTS;slot++){
        hb_closest_cache_entry *entry=&cache->entries[slot];
        if(entry->count!=count)continue;
        int matches=1;
        for(int index=0;index<count&&matches;index++)
            matches=entry->nominal[index]==nominal[index]&&entry->allowed[index]==allowed[index];
        if(matches){for(int index=0;index<count;index++)outputs[index]=entry->output[index];return 1;}
    }
    if(!hb_build_closest_assignment(count,nominal,allowed,outputs))return 0;
    hb_closest_cache_entry *entry=&cache->entries[cache->cursor++%HB_CLOSEST_CACHE_SLOTS];
    entry->count=count;
    for(int index=0;index<count;index++){
        entry->nominal[index]=nominal[index];entry->allowed[index]=allowed[index];entry->output[index]=outputs[index];
    }
    return 1;
}
#endif
