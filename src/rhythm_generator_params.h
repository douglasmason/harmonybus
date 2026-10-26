/* The library is global; selection/playback/editor cursors belong to a track. */
static int hb_rg_set(Inst *i,const char *key,const char *value){
    if(strncmp(key,"rhythm_gen_",11))return 0;hb_rg_init();hb_rg_clock *r=&i->player.rhythm;hb_rg_euclid_init(r);int n=parse_i(value,-1);
    if(!strcmp(key,"rhythm_gen_pattern")){int selected=enum_index(value,HB_RG_NAMES,10,r->pattern);if(selected!=r->pattern){hb_prepare_role_change_flush(i);hb_clear_instance_note_state(i);r->pattern=selected;}hb_rg_reset(r);}
    else if(!strcmp(key,"rhythm_gen_mode")){r->mode=enum_index(value,HB_RG_MODES,3,r->mode);hb_rg_reset(r);}
    else if(!strcmp(key,"rhythm_gen_span")){static const char *spans[]={"1 Beat","2 Beats","1 Bar","2 Bars","4 Bars"};r->span=enum_index(value,spans,5,r->span);hb_rg_reset(r);}
    else if(!strcmp(key,"rhythm_gen_source")){r->source=!strcmp(value,"Target");}
    else if(!strcmp(key,"rhythm_gen_edit")){if(n>=1&&n<=4)r->edit=n-1;}
    else if(!strcmp(key,"rhythm_gen_step")){if(n>=1&&n<=16)r->cell=n-1;}
    else if(!strcmp(key,"rhythm_gen_length")){if(n>=1&&n<=16)g_rg_users[r->edit].count=n;g_rg_restored=1;}
    else if(!strcmp(key,"rhythm_gen_kind")){static const char *k[]={"Rest","Attack","Tie"};g_rg_users[r->edit].steps[r->cell].kind=enum_index(value,k,3,1);g_rg_restored=1;}
    else if(!strcmp(key,"rhythm_gen_accent")){if(n>=1&&n<=127)g_rg_users[r->edit].steps[r->cell].accent=n;g_rg_restored=1;}
    else if(!strcmp(key,"rhythm_gen_gate")){if(n>=1&&n<=100)g_rg_users[r->edit].steps[r->cell].gate=n;g_rg_restored=1;}
    else if(!strcmp(key,"rhythm_gen_euclid_hits")){if(n>=0&&n<=HB_RG_STEPS)r->euclid_hits=n;}
    else if(!strcmp(key,"rhythm_gen_euclid_rotation")){if(n>=0&&n<HB_RG_STEPS)r->euclid_rotation=n;}
    else if(!strcmp(key,"rhythm_gen_euclid_apply")){
        if(!strcmp(value,"Apply")){
            hb_rg_euclidean_fill(&g_rg_users[r->edit],r->euclid_hits,r->euclid_rotation);
            g_rg_restored=1;
        }
    }
    else return 0;return 1;
}
static int hb_rg_param(Inst *i,const char *key,char *b,int length){
    if(strncmp(key,"rhythm_gen_",11))return -1;hb_rg_init();hb_rg_clock *r=&i->player.rhythm;hb_rg_euclid_init(r);
    if(!strcmp(key,"rhythm_gen_pattern"))return snprintf(b,(size_t)length,"%s",HB_RG_NAMES[r->pattern]);
    if(!strcmp(key,"rhythm_gen_mode"))return snprintf(b,(size_t)length,"%s",HB_RG_MODES[r->mode]);
    if(!strcmp(key,"rhythm_gen_source"))return snprintf(b,(size_t)length,"%s",r->source?"Target":"Motif");
    if(!strcmp(key,"rhythm_gen_span")){static const char *spans[]={"1 Beat","2 Beats","1 Bar","2 Bars","4 Bars"};return snprintf(b,(size_t)length,"%s",spans[r->span]);}
    if(!strcmp(key,"rhythm_gen_edit"))return snprintf(b,(size_t)length,"%d",r->edit+1);
    if(!strcmp(key,"rhythm_gen_step"))return snprintf(b,(size_t)length,"%d",r->cell+1);
    if(!strcmp(key,"rhythm_gen_length"))return snprintf(b,(size_t)length,"%d",g_rg_users[r->edit].count);
    hb_rg_step step=g_rg_users[r->edit].steps[r->cell];
    if(!strcmp(key,"rhythm_gen_kind")){static const char *k[]={"Rest","Attack","Tie"};return snprintf(b,(size_t)length,"%s",k[step.kind]);}
    if(!strcmp(key,"rhythm_gen_accent"))return snprintf(b,(size_t)length,"%d",step.accent);
    if(!strcmp(key,"rhythm_gen_gate"))return snprintf(b,(size_t)length,"%d",step.gate);
    if(!strcmp(key,"rhythm_gen_euclid_hits"))return snprintf(b,(size_t)length,"%d",r->euclid_hits);
    if(!strcmp(key,"rhythm_gen_euclid_rotation"))return snprintf(b,(size_t)length,"%d",r->euclid_rotation);
    if(!strcmp(key,"rhythm_gen_euclid_apply"))return snprintf(b,(size_t)length,"Ready");
    if(!strcmp(key,"rhythm_gen_euclid_preview")){
        hb_rg_pattern *pattern=&g_rg_users[r->edit];
        char preview[HB_RG_STEPS+1];
        for(int index=0;index<pattern->count;index++)
            preview[index]=hb_rg_euclidean_at(index,pattern->count,r->euclid_hits,r->euclid_rotation)?'X':'.';
        preview[pattern->count]=0;
        return snprintf(b,(size_t)length,"%s",preview);
    }
    return -1;
}
static int hb_rg_save(Inst *i,char *b,int length,int used){
    hb_rg_init();hb_rg_clock *r=&i->player.rhythm;hb_rg_euclid_init(r);
    if(used>=0&&used<length)used+=snprintf(b+used,(size_t)(length-used),";rg1,%d,%d,%d,%d",r->pattern,r->mode,r->span,r->source);
    for(int u=0;u<4;u++){
        if(used>=0&&used<length)used+=snprintf(b+used,(size_t)(length-used),",%d",g_rg_users[u].count);
        for(int k=0;k<16;k++){hb_rg_step s=g_rg_users[u].steps[k];if(used>=0&&used<length)used+=snprintf(b+used,(size_t)(length-used),",%d,%d,%d",s.kind,s.accent,s.gate);}
    }
    if(used>=0&&used<length)used+=snprintf(b+used,(size_t)(length-used),";eg1,%d,%d",r->euclid_hits,r->euclid_rotation);
    return used>=length?-1:used;
}
static void hb_rg_restore(Inst *i,const char *state){
    const char *p=strstr(state,";rg1,");if(!p)return;p+=5;
    int v[200];for(int k=0;k<200;k++){char *end;long n=strtol(p,&end,10);if(end==p||n<0||n>127)return;v[k]=(int)n;if(k<199){if(*end!=',')return;p=end+1;}else if(*end&&*end!=';')return;}
    if(v[0]>9||v[1]>2||v[2]>4||v[3]>1)return;
    hb_rg_pattern staged[4];int at=4;
    for(int u=0;u<4;u++){staged[u].count=v[at++];if(staged[u].count<1||staged[u].count>16)return;for(int k=0;k<16;k++){int kind=v[at++],accent=v[at++],gate=v[at++];if(kind>2||accent<1||gate<1||gate>100)return;staged[u].steps[k]=(hb_rg_step){kind,accent,gate};}}
    hb_rg_clock *r=&i->player.rhythm;r->pattern=v[0];r->mode=v[1];r->span=v[2];r->source=v[3];hb_rg_reset(r);hb_rg_init();
    if(!g_rg_restored){memcpy(g_rg_users,staged,sizeof(staged));g_rg_restored=1;}
    /* Draft generator settings use their own optional suffix; legacy rg1 saves load unchanged. */
    hb_rg_euclid_init(r);
    const char *euclid=strstr(state,";eg1,");
    if(euclid){
        int hits,rotation,used=0;
        if(sscanf(euclid,";eg1,%d,%d%n",&hits,&rotation,&used)==2&&used>0&&
           (euclid[used]==0||euclid[used]==';')&&
           hits>=0&&hits<=HB_RG_STEPS&&rotation>=0&&rotation<HB_RG_STEPS){
            r->euclid_hits=hits;r->euclid_rotation=rotation;
        }
    }
}
