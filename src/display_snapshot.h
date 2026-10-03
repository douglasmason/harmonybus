/* One host read publishes a complete live display page. */
static int get_param(void *value,const char *key,char *buffer,int length);
static int hb_display_snapshot(Inst *instance,const char *key,char *buffer,int length){
    static const char *const next_keys[]={"next_lookahead","next_anti_buffer_ms","boundary_buffer_ms","next_model","next_shift","next_loop_length","next_position","next_harmony"};
    static const char *const timing_keys[]={"chord_timing","anticipation","chord_grid_status","timing_position","timing_last_at","timing_next_at","timing_last_chord","timing_next_chord"};
    static const char *const root_keys[]={"follower_root_policy","follower_explicit_root","follower_scale","inferred_root","used_root","used_scale"};
    const char *const *keys=!strcmp(key,"next_harm_snapshot")?next_keys:!strcmp(key,"grid_timing_snapshot")?timing_keys:!strcmp(key,"follower_root_snapshot")?root_keys:0;
    if(!keys)return -1;
    int used=snprintf(buffer,(size_t)length,"dp1");
    for(int index=0;index<(!strcmp(key,"follower_root_snapshot")?6:8);index++){
        char field[192];int count=get_param(instance,keys[index],field,sizeof(field));
        if(count<0||count>=(int)sizeof(field)||used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),"|%s",count?field:"--");
    }
    return used<length?used:-1;
}
