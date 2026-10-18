#ifndef HB_AVAILABLE_TENSIONS_H
#define HB_AVAILABLE_TENSIONS_H
/* Shared by follower content and generated chord extensions. Classification
   always uses the original protected chord, never a newly altered tone. */
static int hb_tension_is_avoid(unsigned chord_mask,int pitch_class){
    return !(chord_mask&(1u<<pitch_class)) &&
        (chord_mask&(1u<<((pitch_class+11)%12)))!=0;
}
#endif
