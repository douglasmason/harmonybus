#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLANG="${CLANG:-clang}"
OUT="$ROOT/build/move"
DIST="$ROOT/dist/harmonybus"
rm -rf "$OUT" "$ROOT/dist/harmonybus" "$ROOT/dist/harmonybus-v0.1.82-module.tar.gz"
mkdir -p "$OUT" "$DIST"
"$CLANG" --target=aarch64-linux-gnu -fuse-ld=lld -std=c11 -O2 -fPIC -fno-stack-protector -DHB_FREESTANDING -nostdlibinc -nostdlib -shared -Wl,--allow-shlib-undefined "$ROOT/modules/harmonybus/dsp/harmonybus.c" "$ROOT/src/harmony_core.c" -o "$OUT/dsp.so"
file "$OUT/dsp.so" | grep -q 'ARM aarch64' || { echo 'Not an AArch64 .so' >&2; exit 1; }
cp "$ROOT/modules/harmonybus/module.json" "$DIST/module.json"
cp "$OUT/dsp.so" "$DIST/dsp.so"
chmod +x "$DIST/dsp.so"
(cd "$ROOT/dist" && tar -czvf harmonybus-v0.1.82-module.tar.gz harmonybus/)
echo "$ROOT/dist/harmonybus-v0.1.82-module.tar.gz"
