#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.79"
module["name"] = "Harmony Bus 0.2.79"
module["abbrev"] = "HB279"
module["description"] = "Harmony Bus v0.2.79 — reliable Scale Above toggle/next button and shortened Foll panel names"

levels = module["capabilities"]["ui_hierarchy"]["levels"]
# Keep navigation labels compact and make level titles consistent.
for level_key, short_name in {
    "follower_source": "Foll Map",
    "follower_root": "Foll Root",
    "follower_mod": "Foll Mod",
    "follower_this": "Foll Trk",
    "follower_all": "Foll All",
}.items():
    if level_key in levels:
        levels[level_key]["name"] = short_name

# Root nav labels were already mostly short, enforce them so migrations cannot regress.
root_params = levels["root"]["params"]
for entry in root_params:
    if isinstance(entry, dict) and "level" in entry:
        entry["label"] = {
            "follower_source": "Foll Map",
            "follower_root": "Foll Root",
            "follower_mod": "Foll Mod",
            "follower_this": "Foll Trk",
            "follower_all": "Foll All",
        }.get(entry["level"], entry.get("label", entry["level"]))

MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = source.replace('/* Harmony Bus v0.2.78 — Schwung MIDI FX. */', '/* Harmony Bus v0.2.79 — Schwung MIDI FX. */')
source = source.replace('#define HB_VERSION "0.2.78"', '#define HB_VERSION "0.2.79"')

old = '''if(!strcmp(key,"mod_scale_above")){int on=parameter[0]=='1'||!strcmp(parameter,"On");if(on)g_bus.approach_control=2;else if(g_bus.approach_control==2)g_bus.approach_control=1;return;}if(!strcmp(key,"mod_chrom_below")){int on=parameter[0]=='1'||!strcmp(parameter,"On");if(on)g_bus.approach_control=0;else if(g_bus.approach_control==0)g_bus.approach_control=1;return;}if(!strcmp(key,"approach_scale_next")){if(parameter[0]=='1'||!strcmp(parameter,"Scale +")){instance->approach_pad_armed=2;}return;}if(!strcmp(key,"approach_chrom_next")){if(parameter[0]=='1'||!strcmp(parameter,"Chrom -")){instance->approach_pad_armed=0;}return;}'''
new = '''if(!strcmp(key,"mod_scale_above")){int on=parameter[0]=='1'||!strcmp(parameter,"On");if(on){g_bus.approach_control=2;instance->approach_pad_armed=1;}else if(g_bus.approach_control==2)g_bus.approach_control=1;return;}if(!strcmp(key,"mod_chrom_below")){int on=parameter[0]=='1'||!strcmp(parameter,"On");if(on){g_bus.approach_control=0;instance->approach_pad_armed=1;}else if(g_bus.approach_control==0)g_bus.approach_control=1;return;}if(!strcmp(key,"approach_scale_next")){if(parameter[0]=='1'||!strcmp(parameter,"Scale +")){instance->approach_pad_armed=2;g_bus.approach_mode=0;}return;}if(!strcmp(key,"approach_chrom_next")){if(parameter[0]=='1'||!strcmp(parameter,"Chrom -")){instance->approach_pad_armed=0;g_bus.approach_mode=0;}return;}'''
if old not in source:
    raise SystemExit("expected Foll Mod control block not found")
source = source.replace(old, new, 1)
DSP.write_text(source)
