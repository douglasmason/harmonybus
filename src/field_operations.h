/* Resolve shared performance lanes once at the Field clock boundary. These
   contributions never overwrite saved settings or leased surface inputs. */
static void hb_field_operations_sync(void){
    double rate=1,zoom=1;unsigned outputs=0;
    memset(g_hb_field_operation_values,0,sizeof(g_hb_field_operation_values));
    for(int owner=0;owner<HB_MAX_INSTANCES;owner++){
        Inst *instance=&g_pool[owner];if(!instance->used||instance->role>=2||(instance->field_managed&&instance->field_mod[0].bypassed))continue;
        hb_motion_config *config=&instance->motion;
        for(int lane=0;lane<HB_MOTION_USER_LANES;lane++){
            int operation=config->lanes[lane].operation;
            if(!hb_mo_field_operation(operation))continue;
            unsigned long long bit=1ULL<<lane;
            if((config->gesture_once&bit)&&!(config->gesture_down&bit)&&
               !(config->gesture_input_started&bit))continue;
            double value;
            if(!hb_mo_value_at(config,lane,hb_motion_position(instance),hb_motion_condition_position(),0,&value))continue;
            /* Field is global: a joint gesture broadcast to several tracks
               must not apply the same multiplier several times. */
            if(operation==HB_MO_FIELD_RATE){value=hb_nf_bound(value*.01,-4,4);if(fabs(value-1)>fabs(rate-1))rate=value;}
            else if(operation==HB_MO_FIELD_ZOOM){value=hb_nf_bound(value*.01,.25,4);if(fabs(value-1)>fabs(zoom-1))zoom=value;}
            else{
                value=hb_nf_bound(value*.01,0,1);
                if(!(outputs&(1u<<lane))||value>g_hb_field_operation_values[lane])g_hb_field_operation_values[lane]=value;
                outputs|=1u<<lane;
            }
        }
    }
    g_hb_field_operation_rate=hb_nf_bound(rate,-4,4);
    g_hb_field_operation_zoom=hb_nf_bound(zoom,.25,4);
    g_hb_field_operation_outputs=outputs;
}
