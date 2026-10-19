#ifndef HB_RHYTHM_GENERATOR_MATERIAL_H
#define HB_RHYTHM_GENERATOR_MATERIAL_H
/* Source indices retain each note/chord's operations. Rests and ties belong
   to the selected rhythm, so only playable source events consume attacks. */
typedef struct {int count,indices[HB_MT_STEPS+2];} hb_rg_material;
static void hb_rg_material_add(hb_rg_material *material,const hb_mt_event *event,int index){
    if(event->kind==0&&event->count>0&&material->count<HB_MT_STEPS+2)
        material->indices[material->count++]=index;
}
static int hb_rg_material_next(const hb_rg_material *material,int attack,int fallback){
    return material->count?material->indices[attack%material->count]:fallback;
}
#endif
