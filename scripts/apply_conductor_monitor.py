#!/usr/bin/env python3
"""Apply the HarmonyBus conductor-monitor hook to a DSP source file.

The transform is intentionally exact and idempotent. It fails loudly if the
expected conductor block has drifted instead of guessing at a nearby location.
"""

from __future__ import annotations

import argparse
from pathlib import Path

MARKER: str = "/* HB conductor monitor injection: Render To Ch is monitor output. */"

BEFORE: str = """    hb_publish_instance_notes(instance);\n    if(max_output<1)return 0;\n    output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;\n    return 1;\n}\n"""

AFTER: str = """    hb_publish_instance_notes(instance);\n    /* HB conductor monitor injection: Render To Ch is monitor output. */\n    if(instance->render_channel>=0)\n        hb_inject_follower_note(instance,mapped,length>=3?input[2]:0,is_on);\n    if(max_output<1)return 0;\n    output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;\n    return 1;\n}\n"""


def apply_transform(source: str) -> str:
    """Return source with the conductor monitor hook applied exactly once."""
    if MARKER in source:
        return source
    occurrences: int = source.count(BEFORE)
    if occurrences != 1:
        raise RuntimeError(
            f"expected exactly one conductor block, found {occurrences}; DSP source drifted"
        )
    return source.replace(BEFORE, AFTER, 1)


def main() -> int:
    """Apply the transform in place."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args: argparse.Namespace = parser.parse_args()

    source_path: Path = args.source
    original: str = source_path.read_text()
    updated: str = apply_transform(original)
    if updated != original:
        source_path.write_text(updated)
        print(f"applied conductor monitor hook: {source_path}")
    else:
        print(f"conductor monitor hook already present: {source_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
