#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginHost.h"
#include "JsonRpcServer.h"
#include "AudioEngine.h"
#include "AppWindow.h"
#include "NativeBridge.h"
#include "StratumLookAndFeel.h"

#if JUCE_WINDOWS
 // Forward-declare to avoid pulling in <windows.h> macro pollution that breaks JUCE DSP.
 extern "C" __declspec(dllimport) long __stdcall SetCurrentProcessExplicitAppUserModelID (const wchar_t* AppID);
 // winmm: raise the system timer resolution to 1ms so the message-thread
 // sequencer Timer fires with tight, low-jitter scheduling.
 extern "C" __declspec(dllimport) unsigned int __stdcall timeBeginPeriod (unsigned int uPeriod);
 extern "C" __declspec(dllimport) unsigned int __stdcall timeEndPeriod   (unsigned int uPeriod);
#endif

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
        // Startup log → %APPDATA%/Stratum DAW/startup.log (overwritten each run)
        auto logFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("Stratum DAW").getChildFile ("startup.log");
        logFile.getParentDirectory().createDirectory();
        fileLogger_ = std::make_unique<juce::FileLogger> (logFile, "Stratum DAW startup", 0);
        juce::Logger::setCurrentLogger (fileLogger_.get());

        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto logStep = [&t0](const char* what)
        {
            juce::Logger::writeToLog("[startup] " + juce::String(what) + " +"
                + juce::String(juce::Time::getMillisecondCounterHiRes() - t0, 1) + "ms");
        };

       #if JUCE_WINDOWS
        // Set a unique AppUserModelID so Windows treats this as a distinct app
        // identity for the taskbar (prevents stale icon cache from old builds).
        SetCurrentProcessExplicitAppUserModelID (L"Stratum.DAW.App.1");
        timeBeginPeriod (1);   // 1ms system timer resolution → low-jitter sequencer
       #endif

        // Scale all UI elements 25% bigger (so it doesn't look zoomed-out)
        juce::Desktop::getInstance().setGlobalScaleFactor(1.25f);

        // Apply our themed LookAndFeel so popup menus, tooltips, alerts etc.
        // match the dark / orange skeuomorphic UI.
        lookAndFeel_ = std::make_unique<StratumLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel(lookAndFeel_.get());
        logStep("look-and-feel ready");

        // Construct the engine but DON'T open the audio device yet — opening
        // the device takes ~0.5-1s on some Windows drivers and would block
        // the window from appearing. We open it right after the window shows.
        engine_ = std::make_unique<AudioEngine>(host_);
        logStep("PluginHost + AudioEngine constructed");

        // Keep JSON-RPC server for VST plugins (frontend calls via window.electronAPI)
        int port = 9001;
        const char* envPort = std::getenv("STRATUM_VST_PORT");
        if (envPort) port = std::atoi(envPort);

        rpc_ = std::make_unique<JsonRpcServer>(port);
        registerVstMethods();
        juce::Logger::writeToLog("[VST] JSON-RPC port " + juce::String(port));
        logStep("JSON-RPC server up");

        // Create main window with native JUCE GUI
        window_ = std::make_unique<AppWindow>(host_, *engine_);
        juce::Logger::writeToLog("[JUCE] Native UI mode");
        logStep("AppWindow + MainComponent constructed");

        // Open the audio device AFTER the window is up so the UI appears
        // immediately. Runs on the message thread (AudioDeviceManager
        // requirement) via callAsync — i.e. right after this initialise()
        // returns and the first frame is painted.
        juce::MessageManager::callAsync ([this, t0]
        {
            engine_->start (44100.0, 512);
            juce::Logger::writeToLog ("[startup] audio device opened (deferred) +"
                + juce::String (juce::Time::getMillisecondCounterHiRes() - t0, 1) + "ms");
        });
    }

    void shutdown() override
    {
       #if JUCE_WINDOWS
        timeEndPeriod (1);
       #endif
        httpBridge_.reset();
        window_.reset();
        rpc_.reset();
        engine_.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        lookAndFeel_.reset();
        juce::Logger::setCurrentLogger (nullptr);
        fileLogger_.reset();
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
        // Perf harness: build a busy beat and play (mode 1) / stop (mode 0).
        // Touches UI, so marshal to the message thread.
        rpc_->registerMethod ("perfStress", [this](const juce::var& p) -> juce::var {
            const int mode = (int) p["mode"];
            juce::MessageManager::callAsync ([this, mode] {
                if (window_)
                    if (auto* mc = window_->getMainComponent())
                        mc->runPerfStress (mode);
            });
            return juce::var (true);
        });
    }

    PluginHost host_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<JsonRpcServer> rpc_;
    std::unique_ptr<AppWindow> window_;
    std::unique_ptr<HttpBridgeServer> httpBridge_;
    std::unique_ptr<StratumLookAndFeel> lookAndFeel_;
    std::unique_ptr<juce::FileLogger> fileLogger_;
};

START_JUCE_APPLICATION(StratumDAWApp)
