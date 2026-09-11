#!/usr/bin/env python3
"""Seed or update a Movy UI-state with the HarmonyBus source bank on tracks 5-8.

Movy stores its hosted chain layout in the per-set ``ui-state.json``. This tool
merges four source-track chains into that document while preserving unrelated
tracks and UI state.

Movy track indices are zero-based internally, so user-facing tracks 5-8 are
stored as ``t`` values 4-7.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

MAP_USER_TRACK_TO_RENDER_CHANNEL: dict[int, int] = {
    5: 2,   # Conductor monitor -> user MIDI channel 3.
    6: 1,   # Follower render -> user MIDI channel 2.
    7: 2,   # Follower render -> user MIDI channel 3.
    8: 3,   # Follower render -> user MIDI channel 4.
}


def make_harmonybus_state(user_track: int) -> str:
    """Return fresh HarmonyBus hb15 state for one user-facing source track."""
    if user_track not in MAP_USER_TRACK_TO_RENDER_CHANNEL:
        raise ValueError(f"unsupported HarmonyBus source track: {user_track}")

    role: int = 0 if user_track == 5 else 1
    render_channel: int = MAP_USER_TRACK_TO_RENDER_CHANNEL[user_track]
    source_channel: int = 0  # Movy private chains emit internal MIDI on ch1.

    values: list[int] = [
        role,
        0,  # mode
        0,  # map_target
        25,  # inference_window_ms
        2,  # root_policy: Auto-Infer
        0,  # explicit_root
        0,  # input_root
        0,  # transpose
        0,  # split_map
        0,  # stability
        0,  # accidentals
        render_channel,
        source_channel,
        0,  # chord_timing
        0,  # context
        1,  # clip_context
        0,  # follow_lookahead_ms
        0,  # retrigger_held
        0,  # anticipation
        20,  # boundary_buffer_ms
        60,  # analysis_release_ms
        0,  # content_map
        0,  # travel_map
        0,  # follower_scale
        0,  # quant_timing
    ]
    return "hb15," + ",".join(str(value) for value in values)


def make_source_track_chain(user_track: int, synth_module: str | None) -> dict[str, Any]:
    """Build one Movy ChainTrackState for a HarmonyBus source track."""
    internal_track: int = user_track - 1
    components: list[dict[str, str]] = [
        {
            "c": "midi_fx1",
            "m": "harmonybus",
            "s": make_harmonybus_state(user_track),
        }
    ]
    if synth_module:
        components.append({"c": "synth", "m": synth_module})
    return {"t": internal_track, "comp": components}


def merge_source_bank(
    ui_state: dict[str, Any],
    synth_module: str | None = "plaits",
) -> dict[str, Any]:
    """Return ``ui_state`` with Movy tracks 5-8 replaced by the HB source bank."""
    existing_chains_raw: Any = ui_state.get("chains", [])
    existing_chains: list[Any] = existing_chains_raw if isinstance(existing_chains_raw, list) else []
    source_internal_tracks: set[int] = {4, 5, 6, 7}

    preserved_chains: list[Any] = [
        chain
        for chain in existing_chains
        if not (
            isinstance(chain, dict)
            and isinstance(chain.get("t"), int)
            and chain["t"] in source_internal_tracks
        )
    ]
    source_chains: list[dict[str, Any]] = [
        make_source_track_chain(user_track, synth_module)
        for user_track in range(5, 9)
    ]

    result: dict[str, Any] = dict(ui_state)
    result["chains"] = sorted(
        [*preserved_chains, *source_chains],
        key=lambda chain: chain.get("t", 1_000_000) if isinstance(chain, dict) else 1_000_000,
    )
    return result


def parse_args() -> argparse.Namespace:
    """Parse CLI arguments."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser()
    parser.add_argument(
        "input",
        nargs="?",
        type=Path,
        help="Existing Movy ui-state.json. Omit to start from an empty object.",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Write merged JSON here instead of stdout.",
    )
    parser.add_argument(
        "--synth",
        default="plaits",
        help="Movy/Schwung sound-generator module for source monitoring (default: plaits).",
    )
    parser.add_argument(
        "--no-synth",
        action="store_true",
        help="Create MIDI-FX-only source chains; useful for routing tests.",
    )
    return parser.parse_args()


def main() -> int:
    """Run the profile generator."""
    args: argparse.Namespace = parse_args()
    ui_state: dict[str, Any]
    if args.input is None:
        ui_state = {}
    else:
        loaded: Any = json.loads(args.input.read_text())
        if not isinstance(loaded, dict):
            raise ValueError("Movy ui-state root must be a JSON object")
        ui_state = loaded

    synth_module: str | None = None if args.no_synth else str(args.synth)
    merged: dict[str, Any] = merge_source_bank(ui_state, synth_module=synth_module)
    text: str = json.dumps(merged, separators=(",", ":")) + "\n"

    if args.output is None:
        print(text, end="")
    else:
        args.output.write_text(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
