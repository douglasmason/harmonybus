#!/usr/bin/env bash
set -euo pipefail
# Arguments are source checkouts; the instrument fixture is the only substitute.
INTEGRATION_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HB_SOURCE="$INTEGRATION_ROOT"
SCHWUNG_SOURCE="$(realpath "$1")"
FIXTURE_ROOT="$(mktemp -d)"
trap 'rm -rf "$FIXTURE_ROOT"' EXIT

mkdir -p "$FIXTURE_ROOT/chain" "$FIXTURE_ROOT/movy" "$FIXTURE_ROOT/sound_generators/probe" "$FIXTURE_ROOT/midi_fx/harmonybus" "$FIXTURE_ROOT/audio_fx/probe"
CHAIN_SOURCE="$SCHWUNG_SOURCE/src/modules/chain/dsp"
# Match upstream's private lane implementation without exporting host symbols.
for component in lane_store lane_serial lane_edit; do
  ${CC:-cc} ${TEST_CFLAGS:-} -O1 -fPIC -fvisibility=hidden -c "$SCHWUNG_SOURCE/src/host/$component.c" \
    -I"$SCHWUNG_SOURCE/src" -I"$SCHWUNG_SOURCE/src/host" -o "$FIXTURE_ROOT/$component.o"
done
${CC:-cc} ${TEST_CFLAGS:-} -O1 -shared -fPIC "$CHAIN_SOURCE/chain_host.c" "$CHAIN_SOURCE/chain_json.c" \
  "$CHAIN_SOURCE/chain_synth_load.c" "$CHAIN_SOURCE/chain_fx_load.c" \
  "$CHAIN_SOURCE/chain_params.c" "$CHAIN_SOURCE/chain_mod.c" "$CHAIN_SOURCE/chain_midi.c" \
  "$CHAIN_SOURCE/chain_patch.c" "$CHAIN_SOURCE/chain_reorder.c" "$CHAIN_SOURCE/chain_bus.c" \
  "$CHAIN_SOURCE/chain_scene.c" "$CHAIN_SOURCE/chain_chance.c" \
  "$CHAIN_SOURCE/chain_lanes.c" "$FIXTURE_ROOT/lane_store.o" "$FIXTURE_ROOT/lane_serial.o" "$FIXTURE_ROOT/lane_edit.o" \
  "$SCHWUNG_SOURCE/src/host/unified_log.c" -I"$SCHWUNG_SOURCE/src" -lm -ldl -lpthread -o "$FIXTURE_ROOT/chain/dsp.so"
${CC:-cc} ${TEST_CFLAGS:-} -O1 -shared -fPIC -I"$SCHWUNG_SOURCE/src" "$HB_SOURCE/tests/stock_field_probe.c" -o "$FIXTURE_ROOT/sound_generators/probe/dsp.so"
printf '%s\n' '{"id":"probe","name":"Probe","api_version":2,"capabilities":{"component_type":"sound_generator"}}' > "$FIXTURE_ROOT/sound_generators/probe/module.json"
cp "$FIXTURE_ROOT/sound_generators/probe/dsp.so" "$FIXTURE_ROOT/audio_fx/probe/probe.so"
printf '%s\n' '{"id":"probe","name":"Probe","api_version":2,"capabilities":{"component_type":"audio_fx"}}' > "$FIXTURE_ROOT/audio_fx/probe/module.json"
${CC:-cc} ${TEST_CFLAGS:-} -std=c11 -D_POSIX_C_SOURCE=200809L -DHB_STOCK_FIELD_MOD -include sys/mman.h -include unistd.h -O1 -shared -fPIC "$HB_SOURCE/modules/harmonybus/dsp/harmonybus.c" "$HB_SOURCE/src/harmony_core.c" -lm -o "$FIXTURE_ROOT/midi_fx/harmonybus/dsp.so"
cp "$HB_SOURCE/modules/harmonybus/module.json" "$FIXTURE_ROOT/midi_fx/harmonybus/module.json"
${CC:-cc} ${TEST_CFLAGS:-} -std=c11 -D_POSIX_C_SOURCE=200809L -I"$SCHWUNG_SOURCE/src" "$HB_SOURCE/tests/stock_field_route_test.c" -ldl -lm -o "$FIXTURE_ROOT/route-test"
"$FIXTURE_ROOT/route-test" "$FIXTURE_ROOT/chain/dsp.so" "$FIXTURE_ROOT/chain"
