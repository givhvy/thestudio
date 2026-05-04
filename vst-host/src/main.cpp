#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginHost.h"
#include "JsonRpcServer.h"
#include "AudioEngine.h"
#include "AppWindow.h"
#include "NativeBridge.h"

// HTTP bridge server for dev mode (Vite on localhost:3001 → C++ NativeBridge)
// Listens on port 3002. Each request blocks until NativeBridge produces a result.
// POST /invoke  body: { "channel": "fs:listDirectory", "args": "[\"C:/path\"]" }
// Response:     { "result": [...] }
class HttpBridgeServer : public juce::Thread
{
public:
    HttpBridgeServer (NativeBridge& bridge, int port = 3002)
        : juce::Thread ("HttpBridge"), bridge_ (bridge), port_ (port) { startThread(); }
    ~HttpBridgeServer() override { stopThread (2000); }

    void run() override
    {
        juce::StreamingSocket server;
        if (! server.createListener (port_)) return;
        while (! threadShouldExit())
        {
            if (! server.isConnected()) break;
            auto* rawClient = server.waitForNextConnection();
            if (rawClient == nullptr) continue;
            std::unique_ptr<juce::StreamingSocket> client (rawClient);

            // Read HTTP request
            juce::MemoryOutputStream reqBuf;
            char buf[4096]; int n;
            while ((n = client->read (buf, sizeof(buf) - 1, false)) > 0)
            {
                reqBuf.write (buf, (size_t) n);
                auto s = reqBuf.toString();
                if (s.contains ("\r\n\r\n")) break;
            }
            auto req = reqBuf.toString();

            // Handle CORS preflight
            if (req.startsWith ("OPTIONS"))
            {
                sendHttpResponse (client.get(), 204, "", "");
                continue;
            }

            // Parse Content-Length and body
            juce::String body;
            auto headerEnd = req.indexOf ("\r\n\r\n");
            if (headerEnd >= 0)
            {
                auto headers = req.substring (0, headerEnd);
                int cl = headers.fromFirstOccurrenceOf ("Content-Length: ", false, true).getIntValue();
                body = req.substring (headerEnd + 4);
                while (body.getNumBytesAsUTF8() < (size_t) cl)
                {
                    n = client->read (buf, sizeof(buf)-1, false);
                    if (n <= 0) break;
                    buf[n] = 0; body += buf;
                }
            }

            auto parsed  = juce::JSON::parse (body);
            auto channel = parsed["channel"].toString();
            auto argsStr = parsed["args"].toString();
            juce::var args;
            if (argsStr.isNotEmpty()) args = juce::JSON::parse (argsStr);

            // Call bridge on message thread and wait for result
            juce::var result;
            juce::WaitableEvent done;
            bridge_.callSync (channel, args, [&] (const juce::var& r) {
                result = r;
                done.signal();
            });
            done.wait (5000);

            auto resultJson = juce::JSON::toString (result);
            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty ("result", result);
            sendHttpResponse (client.get(), 200, "application/json", juce::JSON::toString (juce::var (obj.get())));
        }
    }

private:
    static void sendHttpResponse (juce::StreamingSocket* c, int code,
                                  const juce::String& contentType, const juce::String& body)
    {
        juce::String status = (code == 200 ? "200 OK" : code == 204 ? "204 No Content" : "200 OK");
        juce::String http = "HTTP/1.1 " + status + "\r\n"
                            "Access-Control-Allow-Origin: *\r\n"
                            "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                            "Access-Control-Allow-Headers: Content-Type\r\n";
        if (body.isNotEmpty())
            http += "Content-Type: " + contentType + "\r\nContent-Length: "
                    + juce::String (body.getNumBytesAsUTF8()) + "\r\n\r\n" + body;
        else
            http += "Content-Length: 0\r\n\r\n";
        c->write (http.toRawUTF8(), (int) http.getNumBytesAsUTF8());
    }

    NativeBridge& bridge_;
    int port_;
};

// StratumDAW — Hybrid JUCE + WebBrowserComponent app
//  1. Hosts the React frontend inside a JUCE WebBrowserComponent
//  2. JS ↔ C++ bridge replaces Electron IPC for file I/O, VST, audio
//  3. Keeps JSON-RPC server for VST plugin hosting (port 9001)
//
//  Build: cmake -B build -S . && cmake --build build

class StratumDAWApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "StratumDAW"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise (const juce::String&) override
    {
        // Open audio device via AudioEngine
        engine_ = std::make_unique<AudioEngine>(host_);
        engine_->start (44100.0, 512);

        // Keep JSON-RPC server for VST plugins (frontend calls via window.electronAPI)
        int port = 9001;
        const char* envPort = std::getenv("STRATUM_VST_PORT");
        if (envPort) port = std::atoi(envPort);

        rpc_ = std::make_unique<JsonRpcServer>(port);
        registerVstMethods();
        juce::Logger::writeToLog("[VST] JSON-RPC port " + juce::String(port));

        // Create main window with WebBrowserComponent
        window_ = std::make_unique<AppWindow>(host_, *engine_);

        // Dev mode: STRATUM_DEV=1 loads from Vite HMR server (no rebuild needed for JS changes)
        const char* devMode = std::getenv ("STRATUM_DEV");
        const char* devPort = std::getenv ("STRATUM_DEV_PORT");
        int vitePort = (devPort && std::atoi (devPort) > 0) ? std::atoi (devPort) : 3001;

        if (devMode && juce::String (devMode) == "1")
        {
            auto devUrl = "http://localhost:" + juce::String (vitePort);
            window_->getWebHost()->loadURL (devUrl);
            // Start HTTP bridge so dev-mode JS can call NativeBridge via port 3002
            httpBridge_ = std::make_unique<HttpBridgeServer> (window_->getWebHost()->getBridge(), 3002);
            juce::Logger::writeToLog ("[JUCE] DEV MODE: loading from " + devUrl + " | HTTP bridge on :3002");
        }
        else
        {
            // Production: serve via ResourceProvider (already configured in WebBrowserHost)
            window_->getWebHost()->loadFrontend (juce::File());
            juce::Logger::writeToLog ("[JUCE] PROD MODE: serving via ResourceProvider");
        }
    }

    void shutdown() override
    {
        httpBridge_.reset();
        window_.reset();
        rpc_.reset();
        engine_.reset();
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    void registerVstMethods()
    {
        rpc_->registerMethod ("ping", [](const juce::var&) -> juce::var {
            return juce::var("pong");
        });
        rpc_->registerMethod ("scanPlugins", [this](const juce::var& p) -> juce::var {
            return host_.scanDirectory (p["path"].toString());
        });
        rpc_->registerMethod ("loadPlugin", [this](const juce::var& p) -> juce::var {
            juce::String err;
            int id = host_.loadPlugin (p["fileOrIdentifier"].toString(), err);
            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty ("slotId", id);
            obj->setProperty ("error", err);
            return juce::var (obj.get());
        });
        rpc_->registerMethod ("unloadPlugin", [this](const juce::var& p) -> juce::var {
            host_.unloadPlugin ((int) p["slotId"]);
            return juce::var (true);
        });
        rpc_->registerMethod ("noteOn", [this](const juce::var& p) -> juce::var {
            host_.sendMidiNote ((int) p["slotId"], (int) p["channel"],
                                (int) p["note"], (int) p["velocity"], true);
            return juce::var (true);
        });
        rpc_->registerMethod ("noteOff", [this](const juce::var& p) -> juce::var {
            host_.sendMidiNote ((int) p["slotId"], (int) p["channel"],
                                (int) p["note"], 0, false);
            return juce::var (true);
        });
        rpc_->registerMethod ("midiCC", [this](const juce::var& p) -> juce::var {
            host_.sendMidiCC ((int) p["slotId"], (int) p["channel"],
                              (int) p["cc"], (int) p["value"]);
            return juce::var (true);
        });
        rpc_->registerMethod ("setParameter", [this](const juce::var& p) -> juce::var {
            host_.setParameter ((int) p["slotId"], (int) p["paramIndex"],
                                (float) (double) p["value"]);
            return juce::var (true);
        });
        rpc_->registerMethod ("getParameters", [this](const juce::var& p) -> juce::var {
            return host_.getParameters ((int) p["slotId"]);
        });
        rpc_->registerMethod ("getLoadedPlugins", [this](const juce::var&) -> juce::var {
            return host_.getLoadedPlugins();
        });
        rpc_->registerMethod ("showEditor", [this](const juce::var& p) -> juce::var {
            host_.showEditor ((int) p["slotId"], (bool) p["show"]);
            return juce::var (true);
        });
        rpc_->registerMethod ("quit", [this](const juce::var&) -> juce::var {
            juce::MessageManager::callAsync ([this] { quit(); });
            return juce::var (true);
        });
    }

    PluginHost host_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<JsonRpcServer> rpc_;
    std::unique_ptr<AppWindow> window_;
    std::unique_ptr<HttpBridgeServer> httpBridge_;
};

START_JUCE_APPLICATION(StratumDAWApp)
