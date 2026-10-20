#!/usr/bin/env python3
import json
from pathlib import Path

MODULE_PATH = Path("modules/harmonybus/module.json")
TARGET_VERSION = "0.2.66"
TARGET_ABBREV = "HB266"

module = json.loads(MODULE_PATH.read_text())
current_version = module.get("version")
if current_version == TARGET_VERSION:
    raise SystemExit(0)

module["version"] = TARGET_VERSION
module["name"] = f"Harmony Bus {TARGET_VERSION}"
module["abbrev"] = TARGET_ABBREV
module["description"] = (
    f"Harmony Bus v{TARGET_VERSION} — dedicated follower modifier performance page "
    "with K6 reset, K7 scale-above, and K8 chromatic-below knob touches"
)

levels = module.get("capabilities", {}).get("ui_hierarchy", {}).get("levels", {})
root = levels.get("root")
if isinstance(root, dict):
    root["name"] = f"Harmony Bus {TARGET_VERSION}"

MODULE_PATH.write_text(json.dumps(module, indent=2) + "\n")
