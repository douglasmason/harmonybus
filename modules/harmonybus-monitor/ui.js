// HarmonyBus Monitor loader, modeled on ChordDex's proven DSP bridge.
var DSP_PATH = '/data/UserData/schwung/modules/tools/harmonybus-monitor/dsp.so';
var loaded = false;
var attempts = 0;

function setParam(key, value) {
  if (typeof host_module_set_param_blocking === 'function') {
    host_module_set_param_blocking(key, value, 50);
  } else if (typeof host_module_set_param === 'function') {
    host_module_set_param(key, value);
  }
}

globalThis.init = function() {
  clear_screen();
  print(2, 4, 'HarmonyBus Monitor', 2);
  print(2, 20, 'Loading cable-2 sensor...', 1);
  setParam('load', DSP_PATH);
};

globalThis.tick = function() {
  var ping = (typeof host_module_get_param === 'function')
      ? host_module_get_param('ping') : null;
  if (ping === 'pong hbmon1') {
    loaded = true;
  } else if (++attempts % 30 === 0) {
    setParam('load', DSP_PATH);
  }
  clear_screen();
  print(2, 4, 'HarmonyBus Monitor', 2);
  print(2, 20, loaded ? 'ACTIVE' : 'loading...', 1);
  if (loaded) {
    var status = host_module_get_param('status');
    if (status) print(2, 34, status, 1);
    print(2, 50, 'Back: suspend monitor', 1);
  }
};
