from pathlib import Path
import json

DSP = Path('modules/harmonybus/dsp/harmonybus.c')
text = DSP.read_text()

# Rename the existing inferred-harmony split and add a fourth split mode for
# the literal active conductor pitch classes. Preserve numeric meanings of
# existing explicit splits: 0=Harm/Out, 1=135/2467, 2=1357/246, 3=Act/Out.
old = '''    if(split==1){
        unsigned on_bits=(1u<<0)|(1u<<2)|(1u<<4);             /* 135 / 2467 */
        on_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits);
        off_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits);
        source_is_on=(on_bits&(1u<<source_degree))!=0;
    }else if(split==2){
        unsigned on_bits=(1u<<0)|(1u<<2)|(1u<<4)|(1u<<6);     /* 1357 / 246 */
        on_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits);
        off_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits);
        source_is_on=(on_bits&(1u<<source_degree))!=0;
    }else{
        on_mask=inferred_chord_mask;
        off_mask=(uint16_t)(chord_scale&(uint16_t)(~inferred_chord_mask)&0x0FFFu);
        int degree_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,source_degree);
        int degree_pc=mod12(detected.root_pc+degree_interval);
        source_is_on=(inferred_chord_mask&(1u<<degree_pc))!=0;
    }'''
new = '''    if(split==1){
        unsigned on_bits=(1u<<0)|(1u<<2)|(1u<<4);             /* 135 / 2467 */
        on_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits);
        off_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits);
        source_is_on=(on_bits&(1u<<source_degree))!=0;
    }else if(split==2){
        unsigned on_bits=(1u<<0)|(1u<<2)|(1u<<4)|(1u<<6);     /* 1357 / 246 */
        on_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits);
        off_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits);
        source_is_on=(on_bits&(1u<<source_degree))!=0;
    }else if(split==3){
        /* Act. / Out: the ON side is the literal currently active conductor
           pitch classes, before diads are expanded or complex voicings are
           simplified by harmony inference. OUT is the remainder of the
           detected chord-scale. */
        uint8_t active_notes[64];
        int active_count=hb_observed_notes(0,active_notes,64);
        uint16_t active_mask=0;
        for(int index=0;index<active_count;index++)
            active_mask|=(uint16_t)(1u<<mod12(active_notes[index]));
        on_mask=active_mask;
        off_mask=(uint16_t)(chord_scale&(uint16_t)(~active_mask)&0x0FFFu);
        int degree_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,source_degree);
        int degree_pc=mod12(detected.root_pc+degree_interval);
        source_is_on=(active_mask&(1u<<degree_pc))!=0;
    }else{
        /* Harm. / Out: use the inferred harmony mask. This intentionally uses
           the normalized harmony, so a diad may be expanded to its inferred
           triad and a complex voicing may be simplified before the split. */
        on_mask=inferred_chord_mask;
        off_mask=(uint16_t)(chord_scale&(uint16_t)(~inferred_chord_mask)&0x0FFFu);
        int degree_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,source_degree);
        int degree_pc=mod12(detected.root_pc+degree_interval);
        source_is_on=(inferred_chord_mask&(1u<<degree_pc))!=0;
    }'''
if old not in text:
    raise SystemExit('split logic block not found')
text = text.replace(old, new)
text = text.replace('static const char *opts[]={"Auto","135 / 2467","1357 / 246"};g_bus.follower_split_map=enum_index(parameter,opts,3,g_bus.follower_split_map);',
                    'static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};g_bus.follower_split_map=enum_index(parameter,opts,4,g_bus.follower_split_map);')
text = text.replace('if(values[8]>=0&&values[8]<3)g_bus.follower_split_map=values[8];',
                    'if(values[8]>=0&&values[8]<4)g_bus.follower_split_map=values[8];')
text = text.replace('static const char *opts[]={"Auto","135 / 2467","1357 / 246"};int split=g_bus.follower_split_map;if(split<0||split>2)split=0;',
                    'static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};int split=g_bus.follower_split_map;if(split<0||split>3)split=0;')
text = text.replace('0.2.62', '0.2.63').replace('HB262', 'HB263')
DSP.write_text(text)

module_path = Path('modules/harmonybus/module.json')
module = json.loads(module_path.read_text())

def walk(value):
    if isinstance(value, list):
        for item in value:
            walk(item)
    elif isinstance(value, dict):
        if value.get('key') == 'split_map':
            value['options'] = ['Harm. / Out', '135 / 2467', '1357 / 246', 'Act. / Out']
            value['default'] = 'Harm. / Out'
        for child in value.values():
            walk(child)

walk(module)
module['name'] = 'Harmony Bus 0.2.63'
module['version'] = '0.2.63'
module['abbrev'] = 'HB263'
module['description'] = 'Harmony Bus v0.2.63 — split mapping distinguishes inferred harmony tones from literal active conductor tones'
try:
    module['capabilities']['ui_hierarchy']['levels']['root']['name'] = 'Harmony Bus 0.2.63'
except Exception:
    pass
module_path.write_text(json.dumps(module, indent=2) + '\n')

for name in ['scripts/build_harmonybus_move.sh', 'release.json']:
    path = Path(name)
    value = path.read_text().replace('0.2.62', '0.2.63').replace('HB262', 'HB263')
    path.write_text(value)
