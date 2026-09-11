#!/usr/bin/env python3
"""Make the Foll Mod panel use discrete enums/readouts instead of float knobs."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_JSON = ROOT / "modules/harmonybus/module.json"
DSP_C = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def spec_by_key(chain_params: list[dict[str, object]]) -> dict[str, dict[str, object]]:
    return {str(item.get("key")): item for item in chain_params if item.get("key")}


def main() -> None:
    module = json.loads(MODULE_JSON.read_text())
    module["name"] = "Harmony Bus 0.2.68"
    module["abbrev"] = "HB268"
    module["version"] = "0.2.68"
    module["description"] = (
        "Harmony Bus v0.2.68 — Foll Mod uses discrete enum/action/readout controls "
        "with K6 reset, K7 scale-above, and K8 chromatic-below"
    )

    capabilities = module["capabilities"]
    hierarchy = capabilities["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.68"

    chain_params: list[dict[str, object]] = capabilities["chain_params"]
    by_key = spec_by_key(chain_params)

    # The hierarchy must carry complete metadata here. Bare {key: ...} entries
    # are interpreted by Schwung as generic numeric knob cells before the
    # chain-param metadata is available, which produced float pots for enums.
    ordered_keys = [
        "approach_mode",
        "approach",
        "follower_mod_canvas",
        "approach_status",
        "touch_hint",
        "approach_reset",
        "mod_scale_above",
        "mod_chrom_below",
    ]
    missing = [key for key in ordered_keys if key not in by_key]
    if missing:
        raise RuntimeError(f"missing Foll Mod chain params: {missing}")

    follower_mod = hierarchy["levels"]["follower_mod"]
    follower_mod["name"] = "Foll Mod"
    follower_mod["params"] = [dict(by_key[key]) for key in ordered_keys]
    follower_mod["knobs"] = ordered_keys

    # Shorter labels fit the Move display better while retaining discrete types.
    labels = {
        "approach_mode": "Behavior",
        "approach": "Manual",
        "follower_mod_canvas": "Touch Perf",
        "approach_status": "Armed",
        "touch_hint": "Touch",
        "approach_reset": "Reset",
        "mod_scale_above": "Scale Above",
        "mod_chrom_below": "Chrom Below",
    }
    for key in ordered_keys:
        by_key[key]["name"] = labels[key]
    follower_mod["params"] = [dict(by_key[key]) for key in ordered_keys]

    MODULE_JSON.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP_C.read_text()
    for old_version in ("0.2.65", "0.2.66", "0.2.67"):
        source = source.replace(
            f"/* Harmony Bus v{old_version} — Schwung MIDI FX. */\n#define HB_VERSION \"{old_version}\"",
            "/* Harmony Bus v0.2.68 — Schwung MIDI FX. */\n#define HB_VERSION \"0.2.68\"",
        )
    DSP_C.write_text(source)


if __name__ == "__main__":
    main()
