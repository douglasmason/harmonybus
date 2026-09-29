/* Settings share the existing motif pattern as the global default. */
static const char *HB_RR_MODES[]={"Inherit","Off","Override"};
static const char *HB_RR_WINDOWS[]={"Beat","Bar"};
static int hb_rr_set(Inst *i,const char *key,const char *value){
    if(!strcmp(key,"render_rhythm_host")){i->rhythm_host=!strcmp(value,"movy-rhythm-v1");return 1;}
    if(!strcmp(key,"render_rhythm_mode")){i->rhythm_mode=enum_index(value,HB_RR_MODES,3,i->rhythm_mode);return 1;}
    if(!strcmp(key,"render_rhythm_pattern")){i->rhythm_pattern=enum_index(value,HB_MT_RHYTHMS,6,i->rhythm_pattern);return 1;}
    if(!strcmp(key,"render_rhythm_window")){g_render_window=enum_index(value,HB_RR_WINDOWS,2,g_render_window);g_render_restored=1;return 1;}
    if(!strcmp(key,"render_rhythm_track_window")){i->rhythm_window=enum_index(value,HB_RR_WINDOWS,2,i->rhythm_window);return 1;}
    return 0;
}
static int hb_rr_get(Inst *i,const char *key,char *buffer,int length){
    if(!strcmp(key,"render_rhythm_config"))return snprintf(buffer,(size_t)length,"rr1,%d,%d",hb_rr_pattern(i),i->rhythm_mode==2?i->rhythm_window:g_render_window);
    if(!strcmp(key,"render_rhythm_mode"))return snprintf(buffer,(size_t)length,"%s",HB_RR_MODES[i->rhythm_mode]);
    if(!strcmp(key,"render_rhythm_pattern"))return snprintf(buffer,(size_t)length,"%s",HB_MT_RHYTHMS[i->rhythm_pattern]);
    if(!strcmp(key,"render_rhythm_window"))return snprintf(buffer,(size_t)length,"%s",HB_RR_WINDOWS[g_render_window]);
    if(!strcmp(key,"render_rhythm_track_window"))return snprintf(buffer,(size_t)length,"%s",HB_RR_WINDOWS[i->rhythm_window]);
    return -1;
}
static void hb_rr_restore(Inst *i,const char *state){
    const char *p=strstr(state,";rr1,");int mode,pattern,window,global;
    if(!p||sscanf(p,";rr1,%d,%d,%d,%d",&mode,&pattern,&window,&global)!=4||mode<0||mode>2||pattern<0||pattern>5||window<0||window>1||global<0||global>1)return;
    i->rhythm_mode=mode;i->rhythm_pattern=pattern;i->rhythm_window=window;
    if(!g_render_restored){g_render_window=global;g_render_restored=1;}
}
