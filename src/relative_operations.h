#ifndef HB_RELATIVE_OPERATIONS_H
#define HB_RELATIVE_OPERATIONS_H
#ifndef HB_FREESTANDING
#include <string.h>
#endif
#include "../core/include/operation_catalog.h"

/* Indexed immutable lookups: no allocation, parsing or registry scan per note. */
static const hb_ro_definition *hb_ro_choice(int choice){
    return choice>=0&&choice<HB_AR_CHOICE_COUNT&&HB_RELATIVE_OPERATIONS[choice].name?
        &HB_RELATIVE_OPERATIONS[choice]:0;
}
static const hb_ro_definition *hb_ro_motion(int operation){
    return operation>=0&&operation<HB_MO_OPERATION_COUNT&&HB_RO_BY_MOTION[operation]?
        hb_ro_choice(HB_RO_BY_MOTION[operation]-1):0;
}
static const hb_ro_definition *hb_ro_role(int role){
    return role>0&&role<(int)sizeof(HB_RO_BY_ROLE)&&HB_RO_BY_ROLE[role]?
        hb_ro_choice(HB_RO_BY_ROLE[role]-1):0;
}
static const hb_ro_definition *hb_ro_token(unsigned token){
    if(token<sizeof(HB_RO_BY_TOKEN))return HB_RO_BY_TOKEN[token]?hb_ro_choice(HB_RO_BY_TOKEN[token]-1):0;
    /* These are live bank selector IDs, never six-bit recorded operation codes. */
    unsigned index=token-HB_RO_SELECTOR_BASE;
    return index<sizeof(HB_RO_BY_SELECTOR)?hb_ro_choice(HB_RO_BY_SELECTOR[index]-1):0;
}
static int hb_ro_trigger_index(unsigned trigger){
    if(!trigger||(trigger&(trigger-1))||trigger>=(1u<<sizeof(HB_RO_BY_TRIGGER)))return -1;
    int index=0;while(trigger>1){trigger>>=1;index++;}return index;
}
static const hb_ro_definition *hb_ro_trigger(unsigned trigger){
    int index=hb_ro_trigger_index(trigger);
    return index>=0?hb_ro_choice(HB_RO_BY_TRIGGER[index]-1):0;
}
/* The already resolved musical fields share one presentation identity. */
static const hb_ro_definition *hb_ro_resolved(int role,int modifier,int connector_above){
    if(role)return hb_ro_role(role);
    return modifier?hb_ro_motion(modifier<0?HB_MO_BELOW:modifier==1?HB_MO_ABOVE:
        modifier==2&&connector_above?HB_MO_CHROM_ABOVE:HB_MO_TRITONE_V):0;
}
static const char *hb_ro_canonical_name(const char *name){
    for(unsigned index=0;index<sizeof(HB_RO_ALIASES)/sizeof(HB_RO_ALIASES[0]);index++)
        if(!strcmp(name,HB_RO_ALIASES[index].alias))return HB_RO_ALIASES[index].name;
    return name;
}
/* Direction is a preference. Switch only when it exposes an otherwise missing
   adjacent pitch, and select the entire LT/Upper Dim recipe, not an offset. */
static unsigned hb_ro_resolve_selector(unsigned token,int target,unsigned scale){
    const hb_ro_definition *selector=hb_ro_token(token);
    if(!selector||!selector->preferred_token)return token;
    const hb_ro_definition *preferred=hb_ro_token(selector->preferred_token);
    const hb_ro_definition *alternate=hb_ro_token(selector->alternate_token);
    int preferred_pitch=target+preferred->semitones,alternate_pitch=target+alternate->semitones;
    if(preferred_pitch<0||preferred_pitch>127)return selector->alternate_token;
    if(alternate_pitch<0||alternate_pitch>127)return selector->preferred_token;
    int preferred_present=!!(scale&(1u<<(preferred_pitch%12)));
    int alternate_present=!!(scale&(1u<<(alternate_pitch%12)));
    return preferred_present&&!alternate_present?selector->alternate_token:selector->preferred_token;
}
#endif
