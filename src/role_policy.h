#ifndef HB_ROLE_POLICY_H
#define HB_ROLE_POLICY_H
/* Role defaults are shared; overrides and voice ownership belong to a track. */
#define HB_POLICY_FIELDS 13
enum {HB_P_FORM,HB_P_QUALITY,HB_P_INVERSION,HB_P_VOICING,HB_P_SPREAD,HB_P_CHROMATIC,
      HB_P_GAP,HB_P_CONTEXT,HB_P_MAJOR,HB_P_MINOR,HB_P_HALFDIM,HB_P_DOMINANT,HB_P_BORROWED};
static const int HB_POLICY_DEFAULTS[HB_POLICY_FIELDS]={0,0,0,0,0,3,0,2,0,0,0,0,0};
static const int HB_POLICY_MAX[HB_POLICY_FIELDS]={20,12,8,3,1000,6,2,2,1,1,1,3,3};
static const char *HB_POLICY_KEYS[HB_POLICY_FIELDS]={"chord_form","chord_quality","chord_inversion","chord_voicing","strum_spread","chromatic_quality","gap_scale","scale_context","local_major","local_minor","local_halfdim","dominant_scale","borrowed_scale"};
#endif
