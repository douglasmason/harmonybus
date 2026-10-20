from pathlib import Path
import json

MODULE = Path("modules/harmonybus/module.json")
DSP = Path("modules/harmonybus/dsp/harmonybus.c")

module = json.loads(MODULE.read_text())
ui = module["capabilities"]["ui_hierarchy"]
levels = ui["levels"]
root_params = levels["root"]["params"]
source = levels["follower_source"]

module["version"] = "0.2.65"
module["name"] = "Harmony Bus 0.2.65"
module["abbrev"] = "HB265"
module["description"] = (
    "Harmony Bus v0.2.65 — dedicated follower modifier performance page "
    "with K6 reset, K7 scale-above, and K8 chromatic-below knob touches"
)
levels["root"]["name"] = "Harmony Bus 0.2.65"

# Foll Map is only mapping/render behavior; modifier performance gets its own page.
modifier_keys = {"approach", "approach_mode"}
source["params"] = [
    item for item in source.get("params", [])
    if item.get("key") not in modifier_keys
]
source["knobs"] = [
    key for key in source.get("knobs", [])
    if key not in modifier_keys
]
source["name"] = "Follower Map"

levels["follower_mod"] = {
    "name": "Follower Mod",
    "params": [
        {
            "key": "approach",
            "name": "Follower Mod",
            "type": "canvas",
            "canvas_script": "canvas.js",
            "canvas_overlay": "follower_mod",
            "as_page": True,
            "show_value": False,
            "extra_keys": ["approach_mode"]
        }
    ],
    "knobs": ["approach"]
}

# Keep follower pages together: Foll Map, Foll Root, Foll Mod.
root_params[:] = [
    item for item in root_params
    if item.get("level") != "follower_mod"
]
root_index = next(
    index for index, item in enumerate(root_params)
    if item.get("level") == "follower_root"
)
root_params.insert(root_index + 1, {
    "level": "follower_mod",
    "label": "Foll Mod"
})

MODULE.write_text(json.dumps(module, indent=2) + "\n")

dsp = DSP.read_text()
dsp = dsp.replace("v0.2.64", "v0.2.65")
dsp = dsp.replace('HB_VERSION "0.2.64"', 'HB_VERSION "0.2.65"')
DSP.write_text(dsp)
