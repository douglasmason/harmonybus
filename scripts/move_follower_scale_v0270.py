#!/usr/bin/env python3
"""Move Follower Scale from Foll Map to Foll Root and bump HarmonyBus to v0.2.70."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_JSON = ROOT / "modules/harmonybus/module.json"
DSP_C = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def main() -> None:
    module = json.loads(MODULE_JSON.read_text())
    module["name"] = "Harmony Bus 0.2.70"
    module["abbrev"] = "HB270"
    module["version"] = "0.2.70"
    module["description"] = "Harmony Bus v0.2.70 — Follower Scale moved from Foll Map to Foll Root"

    capabilities = module["capabilities"]
    hierarchy = capabilities["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.70"
    levels = hierarchy["levels"]

    follower_map = levels["follower_source"]
    follower_root = levels["follower_root"]

    scale_spec = None
    new_map_params = []
    for param in follower_map.get("params", []):
        if param.get("key") == "follower_scale":
            scale_spec = dict(param)
        else:
            new_map_params.append(param)
    if scale_spec is None:
        # Fall back to the canonical chain param if a prior migration already moved it.
        for param in capabilities.get("chain_params", []):
            if param.get("key") == "follower_scale":
                scale_spec = dict(param)
                break
    if scale_spec is None:
        raise RuntimeError("follower_scale metadata not found")

    follower_map["params"] = new_map_params
    follower_map["knobs"] = [key for key in follower_map.get("knobs", []) if key != "follower_scale"]

    root_params = [param for param in follower_root.get("params", []) if param.get("key") != "follower_scale"]
    # Keep editable root-related controls together before readouts.
    insert_index = 2 if len(root_params) >= 2 else len(root_params)
    root_params.insert(insert_index, scale_spec)
    follower_root["params"] = root_params

    root_knobs = [key for key in follower_root.get("knobs", []) if key != "follower_scale"]
    insert_knob_index = 2 if len(root_knobs) >= 2 else len(root_knobs)
    root_knobs.insert(insert_knob_index, "follower_scale")
    follower_root["knobs"] = root_knobs

    MODULE_JSON.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP_C.read_text()
    for old_version in ("0.2.68", "0.2.69"):
        source = source.replace(
            f'/* Harmony Bus v{old_version} — Schwung MIDI FX. */\n#define HB_VERSION "{old_version}"',
            '/* Harmony Bus v0.2.70 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.70"',
        )
    DSP_C.write_text(source)


if __name__ == "__main__":
    main()
