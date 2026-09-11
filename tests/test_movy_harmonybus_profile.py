#!/usr/bin/env python3
"""Tests for the Movy HarmonyBus source-bank profile generator."""

from __future__ import annotations

import importlib.util
from pathlib import Path
from types import ModuleType
from typing import Any


def load_profile_module() -> ModuleType:
    """Load the generator script as a module without requiring package setup."""
    script_path: Path = Path(__file__).resolve().parents[1] / "scripts" / "make_movy_harmonybus_profile.py"
    spec = importlib.util.spec_from_file_location("make_movy_harmonybus_profile", script_path)
    assert spec is not None and spec.loader is not None
    module: ModuleType = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_preserves_unrelated_chains_and_ui_state(profile: ModuleType) -> None:
    """Tracks outside 5-8 and unrelated UI keys survive source-bank seeding."""
    original: dict[str, Any] = {
        "rootPc": 7,
        "chains": [
            {"t": 1, "comp": [{"c": "synth", "m": "obxd"}]},
            {"t": 9, "comp": [{"c": "synth", "m": "nusaw"}]},
        ],
    }
    merged: dict[str, Any] = profile.merge_source_bank(original, synth_module="plaits")
    map_track_to_chain: dict[int, dict[str, Any]] = {
        chain["t"]: chain for chain in merged["chains"] if isinstance(chain, dict) and isinstance(chain.get("t"), int)
    }

    assert merged["rootPc"] == 7
    assert map_track_to_chain[1] == original["chains"][0]
    assert map_track_to_chain[9] == original["chains"][1]
    assert set(range(4, 8)).issubset(map_track_to_chain)


def test_harmonybus_defaults(profile: ModuleType) -> None:
    """Tracks 5-8 are seeded Conductor/Follower with render channels 3/off and 2-4."""
    merged: dict[str, Any] = profile.merge_source_bank({}, synth_module="plaits")
    map_track_to_chain: dict[int, dict[str, Any]] = {chain["t"]: chain for chain in merged["chains"]}

    expected_state: dict[int, str] = {
        4: "hb15,0,0,0,25,2,0,0,0,0,0,0,-1,0,0,0,1,0,0,0,20,60,0,0,0,0",
        5: "hb15,1,0,0,25,2,0,0,0,0,0,0,1,0,0,0,1,0,0,0,20,60,0,0,0,0",
        6: "hb15,1,0,0,25,2,0,0,0,0,0,0,2,0,0,0,1,0,0,0,20,60,0,0,0,0",
        7: "hb15,1,0,0,25,2,0,0,0,0,0,0,3,0,0,0,1,0,0,0,20,60,0,0,0,0",
    }
    for internal_track, state in expected_state.items():
        components: list[dict[str, str]] = map_track_to_chain[internal_track]["comp"]
        assert components[0] == {"c": "midi_fx1", "m": "harmonybus", "s": state}
        assert components[1] == {"c": "synth", "m": "plaits"}


def test_no_synth_mode(profile: ModuleType) -> None:
    """Routing tests can seed MIDI-FX-only source tracks."""
    merged: dict[str, Any] = profile.merge_source_bank({}, synth_module=None)
    for chain in merged["chains"]:
        assert [component["c"] for component in chain["comp"]] == ["midi_fx1"]


def main() -> int:
    """Run tests without pytest so branch CI needs only stock Python."""
    profile: ModuleType = load_profile_module()
    test_preserves_unrelated_chains_and_ui_state(profile)
    test_harmonybus_defaults(profile)
    test_no_synth_mode(profile)
    print("test_movy_harmonybus_profile: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
