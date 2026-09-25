#!/usr/bin/env python3
from __future__ import annotations
import importlib.util
from pathlib import Path
from types import ModuleType
from typing import Any


def load_profile_module() -> ModuleType:
    script_path = Path(__file__).resolve().parents[1] / "scripts" / "make_movy_harmonybus_profile.py"
    spec = importlib.util.spec_from_file_location("make_movy_harmonybus_profile", script_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    profile = load_profile_module()
    original: dict[str, Any] = {"rootPc":7,"chains":[{"t":1,"comp":[{"c":"synth","m":"obxd"}]},{"t":9,"comp":[{"c":"synth","m":"nusaw"}]}]}
    merged = profile.merge_source_bank(original, synth_module="plaits")
    by_track = {chain["t"]:chain for chain in merged["chains"] if isinstance(chain,dict) and isinstance(chain.get("t"),int)}
    assert merged["rootPc"] == 7
    assert by_track[1] == original["chains"][0]
    assert by_track[9] == original["chains"][1]
    expected = {4:2,5:1,6:2,7:3}
    for track, render_channel in expected.items():
        components = by_track[track]["comp"]
        assert components[0]["c"] == "midi_fx1"
        assert components[0]["m"] == "harmonybus"
        fields = components[0]["s"].split(",")
        assert int(fields[12]) == render_channel
        assert int(fields[13]) == 0
        assert components[1] == {"c":"synth","m":"plaits"}
    print("test_movy_harmonybus_profile: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
