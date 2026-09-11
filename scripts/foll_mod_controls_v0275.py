#!/usr/bin/env python3
"""Rework the standard Foll Mod panel and approach control semantics for v0.2.75."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def upsert(params: list[dict[str, object]], spec: dict[str, object]) -> None:
    key = spec["key"]
    for index, item in enumerate(params):
        if item.get("key") == key:
            params[index] = spec
            return
    params.append(spec)


def main() -> None:
    module = json.loads(MODULE.read_text())
    module["name"] = "Harmony Bus 0.2.75"
    module["abbrev"] = "HB275"
    module["version"] = "0.2.75"
    module["description"] = (
        "Harmony Bus v0.2.75 — standard Foll Mod uses manual selector, mutually-exclusive "
        "persistent toggles, and one-shot approach buttons"
    )

    caps = module["capabilities"]
    hierarchy = caps["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.75"
    chain_params: list[dict[str, object]] = caps["chain_params"]

    # The installed Schwung host is treating as_page canvas metadata as an ordinary
    # one-cell canvas param ("Touch Perf --") rather than a canvas page. Remove the
    # param entirely for this release so it cannot be orphan-swept into that bogus page.
    chain_params[:] = [param for param in chain_params if param.get("key") != "follower_mod_canvas"]

    upsert(chain_params, {"key": "foll_mod_blank2", "name": "", "type": "string", "access": "read"})
    upsert(chain_params, {"key": "foll_mod_blank5", "name": "", "type": "string", "access": "read"})
    upsert(chain_params, {
        "key": "approach", "name": "Manual", "type": "enum",
        "options": ["Chrom Below", "Off", "Scale Above"], "options_as_string": True,
        "default": "Off",
    })
    upsert(chain_params, {
        "key": "mod_scale_above", "name": "Scale Above", "type": "enum",
        "options": ["Off", "On"], "options_as_string": True, "default": "Off",
    })
    upsert(chain_params, {
        "key": "mod_chrom_below", "name": "Chrom Below", "type": "enum",
        "options": ["Off", "On"], "options_as_string": True, "default": "Off",
    })
    # These deliberately mirror the existing reset control's two-value action pattern:
    # get_param always returns Off, so any turn gesture fires the action and springs back.
    upsert(chain_params, {
        "key": "approach_reset", "name": "Reset", "type": "enum",
        "options": ["Off", "Reset"], "options_as_string": True, "default": "Off",
    })
    upsert(chain_params, {
        "key": "approach_scale_next", "name": "Scale Next", "type": "enum",
        "options": ["Off", "Scale +"], "options_as_string": True, "default": "Off",
    })
    upsert(chain_params, {
        "key": "approach_chrom_next", "name": "Chrom Next", "type": "enum",
        "options": ["Off", "Chrom -"], "options_as_string": True, "default": "Off",
    })

    params_by_key = {str(param.get("key")): param for param in chain_params if param.get("key")}
    panel = hierarchy["levels"]["follower_mod"]
    panel["name"] = "Foll Mod"
    standard_keys = [
        "approach",
        "foll_mod_blank2",
        "mod_scale_above",
        "mod_chrom_below",
        "foll_mod_blank5",
        "approach_reset",
        "approach_scale_next",
        "approach_chrom_next",
    ]
    panel["knobs"] = standard_keys
    panel["params"] = [dict(params_by_key[key]) for key in standard_keys]

    MODULE.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP.read_text()
    source = re.sub(
        r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"',
        '/* Harmony Bus v0.2.75 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.75"',
        source,
        count=1,
    )

    # A one-shot button must win regardless of the hidden Touch Perf behavior mode.
    source = re.sub(
        r'static int hb_active_approach\(const Inst \*instance\)\{.*?\n\}\nstatic int hb_apply_approach',
        '''static int hb_active_approach(const Inst *instance){
    if(!instance)return 1;
    if(instance->approach_pad_armed!=1)return instance->approach_pad_armed;
    if(g_bus.approach_mode==1){
        if(instance->approach_below_held)return 0;
        if(instance->approach_above_held)return 2;
    }
    return g_bus.approach_control;
}
static int hb_apply_approach''',
        source,
        count=1,
        flags=re.S,
    )
    source = source.replace(
        'static void hb_consume_next_approach(Inst *instance){\n    if(!instance||g_bus.approach_mode!=0)return;\n    instance->approach_pad_armed=1;\n    /* The knob is another way to arm exactly the same one-shot state. */\n    g_bus.approach_control=1;\n}',
        'static void hb_consume_next_approach(Inst *instance){\n    if(!instance)return;\n    instance->approach_pad_armed=1;\n}',
        1,
    )

    # Standard-panel toggles are persistent aliases of the Manual 3-state selector.
    old_handlers = '''if(!strcmp(key,"approach_reset")){if(parameter[0]=='1'||!strcmp(parameter,"Reset")){instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;g_bus.approach_control=1;}return;}if(!strcmp(key,"mod_scale_above")){int down=parameter[0]=='1'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;else if(instance->approach_pad_armed==2)instance->approach_pad_armed=1;}else instance->approach_above_held=(uint8_t)down;return;}if(!strcmp(key,"mod_chrom_below")){int down=parameter[0]=='1'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;else if(instance->approach_pad_armed==0)instance->approach_pad_armed=1;}else instance->approach_below_held=(uint8_t)down;return;}'''
    new_handlers = '''if(!strcmp(key,"approach_reset")){if(parameter[0]=='1'||!strcmp(parameter,"Reset")){instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;g_bus.approach_control=1;}return;}if(!strcmp(key,"mod_scale_above")){int on=parameter[0]=='1'||!strcmp(parameter,"On");if(on)g_bus.approach_control=2;else if(g_bus.approach_control==2)g_bus.approach_control=1;return;}if(!strcmp(key,"mod_chrom_below")){int on=parameter[0]=='1'||!strcmp(parameter,"On");if(on)g_bus.approach_control=0;else if(g_bus.approach_control==0)g_bus.approach_control=1;return;}if(!strcmp(key,"approach_scale_next")){if(parameter[0]=='1'||!strcmp(parameter,"Scale +")){instance->approach_pad_armed=2;}return;}if(!strcmp(key,"approach_chrom_next")){if(parameter[0]=='1'||!strcmp(parameter,"Chrom -")){instance->approach_pad_armed=0;}return;}'''
    if old_handlers not in source:
        raise RuntimeError("expected follower modifier handlers not found")
    source = source.replace(old_handlers, new_handlers, 1)

    old_get = 'if(!strcmp(key,"approach_reset"))return snprintf(buffer,(size_t)length,"Off");if(!strcmp(key,"mod_scale_above")){int on=g_bus.approach_mode==0?(instance->approach_pad_armed==2):(instance->approach_above_held!=0);return snprintf(buffer,(size_t)length,"%s",on?"On":"Off");}if(!strcmp(key,"mod_chrom_below")){int on=g_bus.approach_mode==0?(instance->approach_pad_armed==0):(instance->approach_below_held!=0);return snprintf(buffer,(size_t)length,"%s",on?"On":"Off");}'
    new_get = 'if(!strcmp(key,"approach_reset"))return snprintf(buffer,(size_t)length,"Off");if(!strcmp(key,"approach_scale_next"))return snprintf(buffer,(size_t)length,"Off");if(!strcmp(key,"approach_chrom_next"))return snprintf(buffer,(size_t)length,"Off");if(!strcmp(key,"mod_scale_above"))return snprintf(buffer,(size_t)length,"%s",g_bus.approach_control==2?"On":"Off");if(!strcmp(key,"mod_chrom_below"))return snprintf(buffer,(size_t)length,"%s",g_bus.approach_control==0?"On":"Off");}'
    if old_get not in source:
        raise RuntimeError("expected follower modifier getters not found")
    source = source.replace(old_get, new_get, 1)

    version_marker = 'if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);'
    if 'foll_mod_blank2' not in source:
        source = source.replace(
            version_marker,
            version_marker + 'if(!strcmp(key,"foll_mod_blank2"))return snprintf(buffer,(size_t)length,"");',
            1,
        )

    DSP.write_text(source)


if __name__ == "__main__":
    main()
