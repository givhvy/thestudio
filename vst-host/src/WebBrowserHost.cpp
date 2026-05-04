#include "WebBrowserHost.h"

static const char* kBridgeScript = R"JS(
(function() {
    if (window.__juceBridgeInjected) return;
    window.__juceBridgeInjected = true;
    window.__juceCallbacks = {};
    window.__juceListeners = {};

    // JUCE 8 native function invocation via emitEvent
    var _lastPromiseId = 0;
    var _pendingPromises = {};

    window.__JUCE__.backend.addEventListener('__juce__complete', function(e) {
        var pid = e.promiseId;
        if (_pendingPromises[pid]) {
            _pendingPromises[pid](e.result);
            delete _pendingPromises[pid];
        }
    });

    function _invoke(id, channel, argsJson) {
        var promiseId = _lastPromiseId++;
        _pendingPromises[promiseId] = function(result) {
            var cb = window.__juceCallbacks[id];
            if (cb) { delete window.__juceCallbacks[id]; cb(JSON.stringify(result)); }
        };
        window.__JUCE__.backend.emitEvent('__juce__invoke', {
            name: '__juceInvoke',
            params: [id, channel, argsJson],
            resultId: promiseId
        });
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
                _invoke(id, channel, JSON.stringify(args));
            });
        },
        on: function(channel, callback) {
            window.__juceListeners[channel] = window.__juceListeners[channel] || [];
            window.__juceListeners[channel].push(callback);
        },
        logToMain: function(msg) {},
        onMenuAction: function(cb) { window.__juceListeners['menu:action'] = [function(e,a){cb(a);}]; },
        onOpenAction: function(cb) { window.__juceListeners['menu:open'] = [function(e){cb();}]; },
        onSaveAction: function(cb) { window.__juceListeners['menu:save'] = [function(e){cb();}]; },
        onExportAction: function(cb) { window.__juceListeners['menu:export'] = [function(e){cb();}]; },
        onPlayAction: function(cb) { window.__juceListeners['menu:play'] = [function(e){cb();}]; },
        onStopAction: function(cb) { window.__juceListeners['menu:stop'] = [function(e){cb();}]; },
        onRecordAction: function(cb) { window.__juceListeners['menu:record'] = [function(e){cb();}]; },
        onUndoAction: function(cb) { window.__juceListeners['menu:undo'] = [function(e){cb();}]; },
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

    var origLog = console.log;
    console.log = function() {
        var msg = Array.from(arguments).map(function(a) {
            try { return JSON.stringify(a); } catch(e) { return String(a); }
        }).join(' ');
        window.electronAPI.invoke('log', msg);
        if (origLog) origLog.apply(console, arguments);
    };

    var origErr = console.error;
    console.error = function() {
        var msg = Array.from(arguments).map(function(a) {
            try { return JSON.stringify(a); } catch(e) { return String(a); }
        }).join(' ');
        window.electronAPI.invoke('log', '[ERROR] ' + msg);
        if (origErr) origErr.apply(console, arguments);
    };

    // Vite HMR hot-reload support
    try {
        var hmrWs = new WebSocket('ws://localhost:3001');
        hmrWs.onmessage = function(e) {
            try {
                var msg = JSON.parse(e.data);
                if (msg.type === 'full-reload' || msg.type === 'update') {
                    window.location.reload();
                }
            } catch(err) {}
        };
        hmrWs.onerror = function() {};
    } catch(e) {}
})();
)JS";

static std::optional<juce::WebBrowserComponent::Resource>
    serveFile (const juce::String& urlPath, const juce::File& frontendDir)
{
    // urlPath starts with "/"
    auto relativePath = urlPath == "/" ? "/index.html" : urlPath;
    auto file = frontendDir.getChildFile (relativePath.substring (1).replaceCharacter ('/', juce::File::getSeparatorChar()));

    if (! file.existsAsFile())
        return std::nullopt;

    auto ext = file.getFileExtension().toLowerCase();
    juce::String mime = "application/octet-stream";
    if      (ext == ".html") mime = "text/html";
    else if (ext == ".js")   mime = "application/javascript";
    else if (ext == ".css")  mime = "text/css";
    else if (ext == ".png")  mime = "image/png";
    else if (ext == ".svg")  mime = "image/svg+xml";
    else if (ext == ".woff2") mime = "font/woff2";
    else if (ext == ".json") mime = "application/json";

    juce::MemoryBlock mb;
    file.loadFileAsData (mb);
    std::vector<std::byte> data (mb.getSize());
    std::memcpy (data.data(), mb.getData(), mb.getSize());
    return juce::WebBrowserComponent::Resource { std::move (data), mime };
}

juce::WebBrowserComponent::Options WebBrowserHost::buildOptions (NativeBridge& bridge)
{
    // The frontend root directory — resolved at runtime
    // We store it in a shared_ptr so the lambda can capture it
    auto frontendDirPtr = std::make_shared<juce::File>();

    // Discover frontend/dist at configure time using the same walk-up logic as main.cpp
    auto execDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    {
        auto dir = execDir;
        for (int i = 0; i < 6; ++i)
        {
            auto candidate = dir.getChildFile ("frontend").getChildFile ("dist");
            if (candidate.getChildFile ("index.html").existsAsFile())
            {
                *frontendDirPtr = candidate;
                break;
            }
            dir = dir.getParentDirectory();
        }
    }

    return juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider ([frontendDirPtr] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            return serveFile (path, *frontendDirPtr);
        })
        .withNativeFunction ("__juceInvoke",
            [&bridge] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
            {
                if (args.size() < 3) { completion (juce::var()); return; }
                auto callbackId = args[0].toString();
                auto channel    = args[1].toString();
                juce::var parsedArgs;
                {
                    auto argsStr = args[2].toString();
                    if (argsStr.isNotEmpty())
                        parsedArgs = juce::JSON::parse (argsStr);
                }
                // Store completion so sendCallback can fire it instead of evaluateJavascript
                bridge.setPendingCompletion (callbackId, std::move (completion));
                bridge.handleJSInvoke (channel, parsedArgs, callbackId);
            })
        .withUserScript (kBridgeScript);
}

WebBrowserHost::WebBrowserHost (PluginHost& pluginHost, AudioEngine& audioEngine)
    : juce::WebBrowserComponent (buildOptions (bridge)),
      bridge (pluginHost, audioEngine)
{
    bridge.setBrowser (this);

    bridge.onMinimize = [this]()
    {
        if (auto* peer = getPeer())
            peer->setMinimised (true);
    };

    bridge.onMaximize = [this]()
    {
        if (auto* peer = getPeer())
            peer->setFullScreen (! peer->isFullScreen());
    };

    bridge.onClose = []()
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    };
}

void WebBrowserHost::loadFrontend (const juce::File&)
{
    goToURL (getResourceProviderRoot());
}

void WebBrowserHost::loadURL (const juce::String& url)
{
    goToURL (url);
}


void WebBrowserHost::pageFinishedLoading (const juce::String& url)
{
    juce::Logger::writeToLog ("[JUCE] Page loaded: " + url);
}
