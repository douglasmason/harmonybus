#!/usr/bin/env python3
"""Seed or update a Movy UI-state with the HarmonyBus source bank on tracks 5-8."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from typing import Any

MAP_USER_TRACK_TO_RENDER_CHANNEL: dict[int, int] = {5: 2, 6: 1, 7: 2, 8: 3}


def make_harmonybus_state(user_track: int) -> str:
    if user_track not in MAP_USER_TRACK_TO_RENDER_CHANNEL:
        raise ValueError(f"unsupported HarmonyBus source track: {user_track}")
    role: int = 0 if user_track == 5 else 1
    render_channel: int = MAP_USER_TRACK_TO_RENDER_CHANNEL[user_track]
    values: list[int] = [role,0,0,25,2,0,0,0,0,0,0,render_channel,0,0,0,1,0,0,0,20,60,0,0,0,0]
    return "hb15," + ",".join(str(value) for value in values)


def make_source_track_chain(user_track: int, synth_module: str | None) -> dict[str, Any]:
    components: list[dict[str, str]] = [{"c":"midi_fx1","m":"harmonybus","s":make_harmonybus_state(user_track)}]
    if synth_module:
        components.append({"c":"synth","m":synth_module})
    return {"t":user_track - 1,"comp":components}


def merge_source_bank(ui_state: dict[str, Any], synth_module: str | None = "plaits") -> dict[str, Any]:
    existing_raw: Any = ui_state.get("chains", [])
    existing: list[Any] = existing_raw if isinstance(existing_raw, list) else []
    source_tracks: set[int] = {4,5,6,7}
    preserved: list[Any] = [chain for chain in existing if not (isinstance(chain, dict) and isinstance(chain.get("t"), int) and chain["t"] in source_tracks)]
    source_chains: list[dict[str, Any]] = [make_source_track_chain(track, synth_module) for track in range(5,9)]
    result: dict[str, Any] = dict(ui_state)
    result["chains"] = sorted([*preserved,*source_chains], key=lambda chain: chain.get("t",1_000_000) if isinstance(chain,dict) else 1_000_000)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", nargs="?", type=Path)
    parser.add_argument("-o","--output", type=Path)
    parser.add_argument("--synth", default="plaits")
    parser.add_argument("--no-synth", action="store_true")
    args = parser.parse_args()
    ui_state: dict[str, Any] = {}
    if args.input is not None:
        loaded: Any = json.loads(args.input.read_text())
        if not isinstance(loaded, dict):
            raise ValueError("Movy ui-state root must be a JSON object")
        ui_state = loaded
    merged = merge_source_bank(ui_state, None if args.no_synth else str(args.synth))
    text = json.dumps(merged, separators=(",", ":")) + "\n"
    if args.output is None:
        print(text, end="")
    else:
        args.output.write_text(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
