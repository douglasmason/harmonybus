#!/usr/bin/env bash
set -euo pipefail

# Prepare a clean schwung-movy checkout for the HarmonyBus source bank.
# Usage:
#   scripts/prepare_movy_harmonybus.sh /path/to/schwung-movy

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/schwung-movy" >&2
  exit 2
fi

MOVY_DIR="$(cd "$1" && pwd)"
HB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ ! -d "$MOVY_DIR/.git" ]]; then
  echo "not a git checkout: $MOVY_DIR" >&2
  exit 2
fi

if [[ -n "$(git -C "$MOVY_DIR" status --porcelain)" ]]; then
  echo "refusing to modify dirty Movy checkout: $MOVY_DIR" >&2
  exit 1
fi

python3 "$HB_ROOT/scripts/apply_movy_harmonybus.py" "$MOVY_DIR"

if [[ -f "$MOVY_DIR/package-lock.json" ]]; then
  npm --prefix "$MOVY_DIR" ci
else
  npm --prefix "$MOVY_DIR" install
fi
npm --prefix "$MOVY_DIR" run typecheck
npm --prefix "$MOVY_DIR" run build:device

echo "Movy HarmonyBus source-bank build prepared successfully."
