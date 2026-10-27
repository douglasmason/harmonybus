/* Control-thread diagnostics use the same resolution and transformations as
   rendering. They do not mutate settings or cache a second harmonic state. */
static void hb_scale_label(int root,unsigned mask,char *buffer,int length){
    unsigned relative=hb_transpose_mask((uint16_t)mask,-root);
    const char *name=0;
    for(int scale=1;scale<HB_SCALE_COUNT;scale++)
        if(!HB_SCALE_DOMINANT[scale]&&HB_SCALE_MASKS[scale]==relative){name=HB_SCALE_NAMES[scale];break;}
    if(relative==0x6dbu)name="Half-Whole Dim";
    if(relative==0xb6du)name="Whole-Half Dim";
    if(name){snprintf(buffer,(size_t)length,"%s %s",hb_pc_name(root),name);return;}
    int used=snprintf(buffer,(size_t)length,"%s [",hb_pc_name(root));
    int first=1;
    for(int offset=0;offset<12&&used<length;offset++)if(mask&(1u<<mod12(root+offset))){
        used+=snprintf(buffer+used,(size_t)(length-used),"%s%s",first?"":" ",hb_pc_name(mod12(root+offset)));first=0;
    }
    if(used<length)snprintf(buffer+used,(size_t)(length-used),"]");
}
static int hb_status_pitch_changes(char *buffer,int length,int used,unsigned removed,unsigned result){
    for(int pitch=0;pitch<12&&used<length;pitch++)if(removed&(1u<<pitch)){
        int raised=mod12(pitch+1);
        if(result&(1u<<raised))used+=snprintf(buffer+used,(size_t)(length-used)," %s>%s",hb_pc_name(pitch),hb_pc_name(raised));
        else used+=snprintf(buffer+used,(size_t)(length-used)," -%s",hb_pc_name(pitch));
    }
    return used;
}
static int hb_effective_scale_status(Inst *instance,char *buffer,int length){
    static const char *origins[]={"Parent","Local","Context","Explicit","Recorded",
        "Tritone","Dom major","Dom minor","Dom Color","Borrowed","Simple Minor"};
    hb_harmony_t harmony=hb_key_harmony(instance,hb_render_harmony(instance));
    if(!harmony.valid)return snprintf(buffer,(size_t)length,"No harmony");
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))source_root=harmony.root_pc;
    unsigned baseline=hb_explicit_scale_mask(source_root,hb_parent_scale_index(instance,harmony));
    int origin=HB_SCALE_PARENT;
    unsigned collection=hb_output_chord_scale_resolved(instance,harmony,source_root,baseline,g_bus.global_transpose,&origin);
    unsigned chord=hb_harmony_chord_mask(harmony),effective=collection|chord;
    char label[128],parent_label[128];hb_scale_label(harmony.root_pc,effective,label,sizeof(label));
    hb_key_context context=hb_key_for(instance);
    int parent_root=context.active?context.target_root:mod12(source_root+g_bus.global_transpose);
    unsigned parent=context.active?context.target_mask:hb_transpose_mask(baseline,g_bus.global_transpose);
    hb_scale_label(parent_root,parent,parent_label,sizeof(parent_label));
    int used=snprintf(buffer,(size_t)length,"%s | %s | %s%s",label,origins[origin],parent_label,
        context.active?(g_parallel_on?" (Parallel)":" (Key)"):" (Parent)");
    if(instance->content_map==9&&used<length){
        /* Compare production content-degree resolution before and after raising.
           A raised pitch already present in the set must still be reported. */
        used+=snprintf(buffer+used,(size_t)(length-used)," | Foll raise:");
        unsigned seen=0;int changed=0;
        for(int degree=0;degree<(hb_ordinal_context(instance)&&hb_popcount12(collection)==8?8:7)&&used<length;degree++){
            int before=mod12(harmony.root_pc+hb_context_render_degree(instance,collection,harmony,degree));
            int after=mod12(harmony.root_pc+hb_content_degree_interval(instance,collection,harmony,degree,9));
            if(before!=after&&!(seen&(1u<<before))){
                used+=snprintf(buffer+used,(size_t)(length-used)," %s>%s",hb_pc_name(before),hb_pc_name(after));
                seen|=1u<<before;changed=1;
            }
        }
        if(!changed&&used<length)used+=snprintf(buffer+used,(size_t)(length-used)," none");
    }
    hb_cp_config config=*hb_cp_settings(&instance->player);
    if(config.extension_treatment&&config.mode&&used<length){
        /* This is a current-harmony preview, not a claim about held/onset voices. */
        int notes[12];unsigned before=0,after=0;
        config.extension_treatment=0;
        hb_cp_voice_semantic(config,60+harmony.root_pc,harmony.root_pc,chord,effective,notes,&before);
        config.extension_treatment=1;
        hb_cp_voice_semantic(config,60+harmony.root_pc,harmony.root_pc,chord,effective,notes,&after);
        used+=snprintf(buffer+used,(size_t)(length-used)," | Chord preview:");
        if(before==after&&used<length)used+=snprintf(buffer+used,(size_t)(length-used)," unchanged");
        else used=hb_status_pitch_changes(buffer,length,used,before&~after,after);
    }
    return used;
}
