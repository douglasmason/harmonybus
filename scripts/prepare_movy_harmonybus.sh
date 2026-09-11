#!/usr/bin/env bash
set -euo pipefail

# Apply the HarmonyBus source-bank overlays to a schwung-movy checkout.
# Usage:
#   scripts/prepare_movy_harmonybus.sh /path/to/schwung-movy
#
# The script is intentionally non-destructive: it refuses a dirty checkout and
# applies each overlay only if it has not already been applied.

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
  echo "refusing to patch dirty Movy checkout: $MOVY_DIR" >&2
  exit 1
fi

apply_overlay() {
  local patch_path="$1"
  if git -C "$MOVY_DIR" apply --reverse --check "$patch_path" >/dev/null 2>&1; then
    echo "already applied: $(basename "$patch_path")"
    return 0
  fi
  git -C "$MOVY_DIR" apply --check "$patch_path"
  git -C "$MOVY_DIR" apply "$patch_path"
  echo "applied: $(basename "$patch_path")"
}

apply_overlay "$HB_ROOT/patches/schwung-movy-hb-source-audio-mute.patch"
apply_overlay "$HB_ROOT/patches/schwung-movy-hb-two-bank-navigation.patch"
apply_overlay "$HB_ROOT/patches/schwung-movy-hb-auto-source-bank.patch"

if [[ -f "$MOVY_DIR/package-lock.json" ]]; then
  npm --prefix "$MOVY_DIR" ci
else
  npm --prefix "$MOVY_DIR" install
fi
npm --prefix "$MOVY_DIR" run typecheck

echo "Movy HarmonyBus overlay prepared successfully."
