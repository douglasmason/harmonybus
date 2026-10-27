#ifndef HB_RELATIVE_FEEDBACK_H
#define HB_RELATIVE_FEEDBACK_H
#include "relative_operations.h"

/* A rendered result, never an instruction to resolve again. Low 32 bits are
   one event; the atomic snapshot adds a monotonically changing serial. */
static unsigned hb_rf_make(const hb_ro_definition *operation,int target,int root,unsigned mask,int chord,int origin){
    unsigned token=operation?(unsigned)operation->token:0;
    if(token>63)token=0; /* Smart selectors must already have become concrete. */
    return 0x80000000u|token|((unsigned)((target%12+12)%12)<<6)|
        ((unsigned)((root%12+12)%12)<<10)|((mask&4095u)<<14)|
        ((unsigned)(chord!=0)<<26)|((unsigned)(origin&3)<<27);
}
static unsigned hb_rf_note(unsigned event,int pitch){
    unsigned root=(unsigned)((pitch%12+12)%12);
    return (event&~((15u<<10)|(4095u<<14)))|(root<<10)|((1u<<root)<<14);
}
static void hb_rf_publish(unsigned long long *snapshot,unsigned event){
    unsigned long long previous=__atomic_load_n(snapshot,__ATOMIC_RELAXED);
    unsigned serial=(unsigned)(previous>>32)+1u;
    __atomic_store_n(snapshot,((unsigned long long)serial<<32)|event,__ATOMIC_RELEASE);
}
#endif
