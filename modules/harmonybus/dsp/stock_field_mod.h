/* Stock chain callback routes, serviced by Movy's pre-render block message.
 * No worker, host ABI extension, or direct parameter writes. */
static void hb_stock_field_clear(Inst *instance){
    for(int i=0;i<2;i++){hb_stock_field_route *r=&instance->field_mod[i];
        if(r->clear&&r->ctx)r->clear(r->ctx,r->source);
    }
}
static int hb_stock_field_key(const char *key,const char **sub){
    if(strncmp(key,"field_lfo",9)||key[9]<'1'||key[9]>'2'||key[10]!=':')return -1;
    *sub=key+11;return key[9]-'1';
}
static int hb_stock_field_target(const char *target){
    return !strcmp(target,"synth")||(target[0]=='f'&&target[1]=='x'&&target[2]>='1'&&target[2]<='8'&&!target[3]);
}
static int hb_stock_field_name(const char *s,int cap){
    if((int)strlen(s)>=cap)return 0;
    for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_'||*s=='-'||*s=='.'||*s==':'))return 0;
    return 1;
}
static int hb_stock_field_set(Inst *instance,const char *key,const char *value){
    const char *sub=0;int i=hb_stock_field_key(key,&sub);if(i<0)return 0;
    hb_stock_field_route *r=&instance->field_mod[i];
    if(r->clear&&r->ctx)r->clear(r->ctx,r->source);
    if(!strcmp(sub,"shape")){int v=parse_i(value,0);r->shape=v>=6&&v<22?v:0;}
    else if(!strcmp(sub,"enabled"))r->enabled=parse_i(value,0)!=0;
    else if(!strcmp(sub,"polarity"))r->bipolar=parse_i(value,0)!=0;
    else if(!strcmp(sub,"depth")){double v=strtod(value,0);if(v>=-1&&v<=1)r->depth=(int)(v*10000+(v<0?-.5:.5));}
    else if(!strcmp(sub,"target")){if(!*value||hb_stock_field_target(value))snprintf(r->target,sizeof(r->target),"%s",value);else r->target[0]=0;}
    else if(!strcmp(sub,"target_param")&&hb_stock_field_name(value,sizeof(r->param)))snprintf(r->param,sizeof(r->param),"%s",value);
    return 1;
}
static int hb_stock_field_get(Inst *instance,const char *key,char *buffer,int length){
    if(!strcmp(key,"field_mod_version"))return snprintf(buffer,length,"%d",instance->field_managed&&instance->field_mod[0].emit&&instance->field_mod[0].clear?1:0);
    const char *sub=0;int i=hb_stock_field_key(key,&sub);if(i<0)return -1;
    hb_stock_field_route *r=&instance->field_mod[i];
    if(!strcmp(sub,"shape"))return snprintf(buffer,length,"%d",r->shape);
    if(!strcmp(sub,"enabled"))return snprintf(buffer,length,"%d",r->enabled);
    if(!strcmp(sub,"polarity"))return snprintf(buffer,length,"%d",r->bipolar);
    if(!strcmp(sub,"depth"))return snprintf(buffer,length,"%.4f",r->depth*.0001);
    if(!strcmp(sub,"target"))return snprintf(buffer,length,"%s",r->target);
    if(!strcmp(sub,"target_param"))return snprintf(buffer,length,"%s",r->param);
    if(!strcmp(sub,"active"))return snprintf(buffer,length,"%d",r->shape&&r->enabled&&r->target[0]&&r->param[0]);
    return -1;
}
static void hb_stock_field_service(Inst *instance,int bypassed){
    for(int i=0;i<2;i++){
        hb_stock_field_route *r=&instance->field_mod[i];
        if(!r->shape||!r->enabled||bypassed||!r->target[0]||!r->param[0]){
            if(r->status&&r->clear&&r->ctx)r->clear(r->ctx,r->source);
            r->status=0;continue;
        }
        if(!r->emit||!r->clear||!r->ctx)continue;
        int channel=r->shape-6;const hb_motion_lane *lane=&instance->motion.lanes[channel];
        double raw=hb_nf_lane_sample(channel,lane->field_x*.01,lane->field_y*.01,lane->field_z*.01);
        float signal=(float)(2*hb_nf_output(channel,raw)-1);
        int result=r->emit(r->ctx,r->source,r->target,r->param,signal,r->depth*.0001f,0,r->bipolar,1);
        r->status=result>=0?1:0;
        if(result<0)r->clear(r->ctx,r->source);
    }
}
static int hb_stock_field_save(Inst *instance,char *buffer,int length,int used){
    for(int i=0;i<2&&used>=0&&used<length;i++){
        hb_stock_field_route *r=&instance->field_mod[i];if(!r->shape)continue;
        used+=snprintf(buffer+used,length-used,";fl1,%d,%d,%d,%d,%d,%s,%s",i,r->shape,r->enabled,r->depth,r->bipolar,r->target[0]?r->target:"-",r->param[0]?r->param:"-");
    }return used;
}
static void hb_stock_field_restore(Inst *instance,const char *state){
    hb_stock_field_clear(instance);
    for(int i=0;i<2;i++){hb_stock_field_route *r=&instance->field_mod[i];r->shape=r->enabled=r->status=0;r->target[0]=r->param[0]=0;}
    const char *p=state;
    while((p=strstr(p,";fl1,"))){int i,shape,enabled,depth,polarity;char target[32],param[64];
        if(sscanf(p,";fl1,%d,%d,%d,%d,%d,%31[^,;],%63[^,;]",&i,&shape,&enabled,&depth,&polarity,target,param)==7&&i>=0&&i<2&&shape>=6&&shape<22&&(enabled==0||enabled==1)&&depth>=-10000&&depth<=10000&&(polarity==0||polarity==1)&&(hb_stock_field_target(target)||!strcmp(target,"-"))&&hb_stock_field_name(param,sizeof(param))){
            hb_stock_field_route *r=&instance->field_mod[i];r->shape=shape;r->enabled=enabled;r->depth=depth;r->bipolar=polarity;
            snprintf(r->target,sizeof(r->target),"%s",!strcmp(target,"-")?"":target);snprintf(r->param,sizeof(r->param),"%s",!strcmp(param,"-")?"":param);
        }p+=5;
    }
}
