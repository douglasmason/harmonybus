#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.86"
module["name"] = "Harmony Bus 0.2.86"
module["abbrev"] = "HB286"
module["description"] = "Harmony Bus v0.2.86 — follower timing fix plus Foll Mod travel shortcut"
module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.86"

panel = module["capabilities"]["ui_hierarchy"]["levels"]["follower_mod"]
# K2 mirrors the existing global Follower Travel parameter from Foll Map.
panel["params"] = [
    ({"key": "travel_map"} if param.get("key") == "foll_mod_blank2" else param)
    for param in panel["params"]
]
panel["knobs"] = ["travel_map" if key == "foll_mod_blank2" else key for key in panel["knobs"]]

MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.86 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.86"', source, count=1)
DSP.write_text(source)
