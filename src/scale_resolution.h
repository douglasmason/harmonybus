/* Provenance travels with the collection through the production resolver.
   UI reads request it; ordinary rendering passes a null pointer. */
enum {HB_SCALE_PARENT,HB_SCALE_LOCAL,HB_SCALE_CONTEXT,HB_SCALE_EXPLICIT,
      HB_SCALE_RECORDED,HB_SCALE_TRITONE,HB_SCALE_DOM_MAJOR,HB_SCALE_DOM_MINOR,
      HB_SCALE_DOM_COLOR,HB_SCALE_BORROWED,HB_SCALE_MINOR_TONIC};
static unsigned hb_scale_resolved(unsigned mask,int origin,int *source){
    if(source)*source=origin;
    return mask;
}
static int hb_dominant_origin(Inst *instance,int minor){
    return instance->dominant_color_held||instance->dominant_color_latched?
        HB_SCALE_DOM_COLOR:minor?HB_SCALE_DOM_MINOR:HB_SCALE_DOM_MAJOR;
}
