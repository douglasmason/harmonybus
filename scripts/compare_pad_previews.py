"""Compare complete pad snapshots against a baseline HarmonyBus checkout."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT: Path = Path(__file__).resolve().parents[1]


def snapshot(checkout: Path, executable: Path) -> str:
    """Compile the same probe against one checkout and capture its snapshots."""
    compiler: str = os.environ.get("HB_TEST_CC", "cc")
    command: list[str] = [compiler, "-w", "-std=c11", "-D_POSIX_C_SOURCE=200809L",
        "-O0", "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-I", str(checkout / "tests"), str(ROOT / "tests/pad_snapshot_probe.c"),
        str(checkout / "src/harmony_core.c"), "-lm", "-o", str(executable)]
    subprocess.run(command, check=True)
    return subprocess.check_output([str(executable)], text=True)


def main() -> None:
    """Fail if any current rendering differs from the baseline renderer."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    arguments: argparse.Namespace = parser.parse_args()
    directory: str
    with tempfile.TemporaryDirectory(prefix="hb-pad-parity-") as directory:
        temporary: Path = Path(directory)
        baseline: str = snapshot(arguments.baseline.resolve(), temporary / "baseline")
        current: str = snapshot(ROOT, temporary / "current")
        assert baseline == current, "Pad snapshots differ; inspect both probe outputs"
        print(f"{len(current.splitlines())} complete pad snapshots match the baseline")


if __name__ == "__main__":
    main()
