#ifndef HB_RHYTHM_PATTERN_H
#define HB_RHYTHM_PATTERN_H
/* Shared musical pattern vocabulary. Consumers supply their event count and
   timing window; target placement, ties and note ownership remain their own. */
static const char *HB_MT_RHYTHMS[]={"As Entered","Even","Long-Short","Short-Long","Accelerate","Decelerate"};
static int hb_rhythm_weight(int pattern,int index,int count){
    if(count<1||index<0||index>=count)return 0;
    if(pattern==2)return index%2?1:3;
    if(pattern==3)return index%2?3:1;
    if(pattern==4)return count-index;
    if(pattern==5)return index+1;
    return 1;
}
#endif
