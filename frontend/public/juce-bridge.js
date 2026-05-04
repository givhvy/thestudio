// JUCE bridge — works in both dev (localhost:3001) and prod (resource provider)
// Uses two transport strategies:
//   1. window.__JUCE__.backend native functions (prod/resource provider)
//   2. Fetch POST to localhost:3002/bridge (dev mode fallback via C++ HTTP bridge)
// Strategy is auto-detected at runtime.

(function () {
  if (window.__juceBridgeInjected) return;
  window.__juceBridgeInjected = true;
  window.__juceCallbacks = {};
  window.__juceListeners = {};

  // Try native JUCE backend first, fall back to HTTP bridge on port 3002
  function getTransport() {
    if (window.__JUCE__ && window.__JUCE__.backend) {
      try {
        var fn = window.__JUCE__.backend.getNativeFunction('__juceInvoke');
        return function(id, channel, argsJson) { fn(id, channel, argsJson); };
      } catch(e) {}
    }
    // HTTP fallback: POST to C++ HTTP bridge server
    return function(id, channel, argsJson) {
      fetch('http://localhost:3002/invoke', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: id, channel: channel, args: argsJson })
      }).then(function(r) { return r.json(); }).then(function(res) {
        var cb = window.__juceCallbacks[id];
        if (cb) { delete window.__juceCallbacks[id]; cb(JSON.stringify(res.result)); }
      }).catch(function(e) {
        var cb = window.__juceCallbacks[id];
        if (cb) { delete window.__juceCallbacks[id]; cb(JSON.stringify({ error: String(e) })); }
      });
    };
  }

  var _transport = null;
  function invoke(id, channel, argsJson) {
    if (!_transport) _transport = getTransport();
    _transport(id, channel, argsJson);
  }

  window.electronAPI = {
    invoke: async function(channel) {
      var args = Array.prototype.slice.call(arguments, 1);
      var id = 'cb_' + Date.now() + '_' + Math.random().toString(36).slice(2);
      return new Promise(function(resolve) {
        window.__juceCallbacks[id] = function(resultJson) {
          var result;
          try { result = JSON.parse(resultJson); } catch(e) { result = resultJson; }
          if (channel === 'fs:readBinaryFile' && result && result.data) {
            var b = atob(result.data);
            var bytes = new Uint8Array(b.length);
            for (var i = 0; i < b.length; i++) bytes[i] = b.charCodeAt(i);
            result.data = bytes;
          }
          resolve(result);
        };
        invoke(id, channel, JSON.stringify(args));
      });
    },
    on: function(channel, callback) {
      window.__juceListeners[channel] = window.__juceListeners[channel] || [];
      window.__juceListeners[channel].push(callback);
    },
    logToMain: function(msg) {},
    onMenuAction: function(cb) { window.__juceListeners['menu:action'] = [function(e,a){cb(a);}]; },
    onOpenAction: function(cb) { window.__juceListeners['menu:open'] = [function(){cb();}]; },
    onSaveAction: function(cb) { window.__juceListeners['menu:save'] = [function(){cb();}]; },
    onExportAction: function(cb) { window.__juceListeners['menu:export'] = [function(){cb();}]; },
    onPlayAction: function(cb) { window.__juceListeners['menu:play'] = [function(){cb();}]; },
    onStopAction: function(cb) { window.__juceListeners['menu:stop'] = [function(){cb();}]; },
    onRecordAction: function(cb) { window.__juceListeners['menu:record'] = [function(){cb();}]; },
    onUndoAction: function(cb) { window.__juceListeners['menu:undo'] = [function(){cb();}]; },
    onPanelAction: function(cb) { window.__juceListeners['menu:panel'] = [function(e,p){cb(p);}]; },
    removeAllListeners: function() {},
    saveFile: async function(name) {
      var r = await window.electronAPI.invoke('dialog:saveFile', name);
      return r || null;
    },
    writeFile: async function(path, data) {
      return await window.electronAPI.invoke('fs:writeFile', path, data);
    },
    vstCall: async function(method, args) {
      return await window.electronAPI.invoke('vst:call', method, args);
    }
  };
})();
