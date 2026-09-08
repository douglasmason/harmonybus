#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLANG="${CLANG:-clang}"
OUT="$ROOT/build"
DIST="$ROOT/dist"
PKG="$DIST/harmonybus-monitor"

rm -rf "$OUT" "$DIST"
mkdir -p "$OUT" "$PKG"

"$CLANG" --target=aarch64-linux-gnu -fuse-ld=lld -std=c11 -O2 -fPIC \
  -fno-stack-protector -nostdlibinc -nostdlib -shared \
  -Wl,--allow-shlib-undefined \
  "$ROOT/dsp/monitor.c" -o "$OUT/dsp.so"

file "$OUT/dsp.so" | grep -q 'ARM aarch64'
cp "$ROOT/module.json" "$PKG/module.json"
cp "$ROOT/ui.js" "$PKG/ui.js"
cp "$OUT/dsp.so" "$PKG/dsp.so"
chmod +x "$PKG/dsp.so"

(cd "$DIST" && tar -czf harmonybus-monitor-v0.1.91-tool.tar.gz harmonybus-monitor)
echo "$DIST/harmonybus-monitor-v0.1.91-tool.tar.gz"
