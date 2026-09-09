#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLANG="${CLANG:-clang}"
OUT="$ROOT/build/move"
DIST_HB="$ROOT/dist/harmonybus"
DIST_MON="$ROOT/dist/harmonybus-monitor"

rm -rf "$OUT" "$DIST_HB" "$DIST_MON" \
  "$ROOT/dist/harmonybus-v0.2.30-module.tar.gz" \
  "$ROOT/dist/harmonybus-monitor-v0.2.30-tool.tar.gz"

mkdir -p "$OUT" "$DIST_HB" "$DIST_MON"

"$CLANG" --target=aarch64-linux-gnu -fuse-ld=lld -std=c11 -O2 -fPIC \
  -fno-stack-protector -DHB_FREESTANDING -nostdlibinc -nostdlib -shared \
  -Wl,--allow-shlib-undefined \
  "$ROOT/modules/harmonybus/dsp/harmonybus.c" "$ROOT/src/harmony_core.c" \
  -o "$OUT/harmonybus-dsp.so"

"$CLANG" --target=aarch64-linux-gnu -fuse-ld=lld -std=c11 -O2 -fPIC \
  -fno-stack-protector -nostdlibinc -nostdlib -shared \
  -Wl,--allow-shlib-undefined \
  "$ROOT/modules/harmonybus-monitor/dsp/monitor.c" \
  -o "$OUT/harmonybus-monitor-dsp.so"

file "$OUT/harmonybus-dsp.so" | grep -q 'ARM aarch64'
file "$OUT/harmonybus-monitor-dsp.so" | grep -q 'ARM aarch64'

cp "$ROOT/modules/harmonybus/module.json" "$DIST_HB/module.json"
cp "$OUT/harmonybus-dsp.so" "$DIST_HB/dsp.so"
chmod +x "$DIST_HB/dsp.so"

cp "$ROOT/modules/harmonybus-monitor/module.json" "$DIST_MON/module.json"
cp "$ROOT/modules/harmonybus-monitor/ui.js" "$DIST_MON/ui.js"
cp "$OUT/harmonybus-monitor-dsp.so" "$DIST_MON/dsp.so"
chmod +x "$DIST_MON/dsp.so"

(cd "$ROOT/dist" && tar -czvf harmonybus-v0.2.30-module.tar.gz harmonybus/)
# Schwung's install-module installer extracts the archive *inside*
# modules/tools/<module-id>, so the tool archive must contain files at its root.
tar -C "$DIST_MON" -czvf "$ROOT/dist/harmonybus-monitor-v0.2.30-tool.tar.gz" module.json ui.js dsp.so

echo "$ROOT/dist/harmonybus-v0.2.30-module.tar.gz"
echo "$ROOT/dist/harmonybus-monitor-v0.2.30-tool.tar.gz"
