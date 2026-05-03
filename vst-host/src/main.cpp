#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginHost.h"
#include "JsonRpcServer.h"
#include "AudioEngine.h"

// StratumVSTHost — JUCE console app that:
//  1. Opens the system audio device
//  2. Loads and hosts VST3/VST2 plugins
//  3. Exposes all functionality via JSON-RPC over TCP port 9001
//  4. Embeds plugin GUIs as child windows
//
// Compile: see BUILD.md in this directory.

class StratumVSTHostApp : public juce::JUCEApplicationBase
{
public:
    const juce::String getApplicationName() override    { return "StratumVSTHost"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise(const juce::String&) override
    {
        // Open audio device via AudioEngine
        engine_ = std::make_unique<AudioEngine>(host_);
        engine_->start(44100.0, 512);

        // Register JSON-RPC methods
        int port = 9001;
        const char* envPort = std::getenv("STRATUM_VST_PORT");
        if (envPort) port = std::atoi(envPort);

        rpc_ = std::make_unique<JsonRpcServer>(port);

        rpc_->registerMethod("ping", [](const juce::var&) -> juce::var {
            return juce::var("pong");
        });

        rpc_->registerMethod("scanPlugins", [this](const juce::var& p) -> juce::var {
            return host_.scanDirectory(p["path"].toString());
        });

        rpc_->registerMethod("loadPlugin", [this](const juce::var& p) -> juce::var {
            juce::String err;
            int id = host_.loadPlugin(p["fileOrIdentifier"].toString(), err);
            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty("slotId", id);
            obj->setProperty("error",  err);
            return juce::var(obj.get());
        });

        rpc_->registerMethod("unloadPlugin", [this](const juce::var& p) -> juce::var {
            host_.unloadPlugin((int)p["slotId"]);
            return juce::var(true);
        });

        rpc_->registerMethod("noteOn", [this](const juce::var& p) -> juce::var {
            host_.sendMidiNote((int)p["slotId"], (int)p["channel"],
                               (int)p["note"], (int)p["velocity"], true);
            return juce::var(true);
        });

        rpc_->registerMethod("noteOff", [this](const juce::var& p) -> juce::var {
            host_.sendMidiNote((int)p["slotId"], (int)p["channel"],
                               (int)p["note"], 0, false);
            return juce::var(true);
        });

        rpc_->registerMethod("midiCC", [this](const juce::var& p) -> juce::var {
            host_.sendMidiCC((int)p["slotId"], (int)p["channel"],
                             (int)p["cc"], (int)p["value"]);
            return juce::var(true);
        });

        rpc_->registerMethod("setParameter", [this](const juce::var& p) -> juce::var {
            host_.setParameter((int)p["slotId"], (int)p["paramIndex"],
                               (float)(double)p["value"]);
            return juce::var(true);
        });

        rpc_->registerMethod("getParameters", [this](const juce::var& p) -> juce::var {
            return host_.getParameters((int)p["slotId"]);
        });

        rpc_->registerMethod("getLoadedPlugins", [this](const juce::var&) -> juce::var {
            return host_.getLoadedPlugins();
        });

        rpc_->registerMethod("showEditor", [this](const juce::var& p) -> juce::var {
            host_.showEditor((int)p["slotId"], (bool)p["show"]);
            return juce::var(true);
        });

        rpc_->registerMethod("quit", [this](const juce::var&) -> juce::var {
            juce::MessageManager::callAsync([this] { quit(); });
            return juce::var(true);
        });

        juce::Logger::writeToLog("StratumVSTHost ready. JSON-RPC port " + juce::String(port));
    }

    void shutdown() override
    {
        rpc_.reset();
        engine_.reset();
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

    void suspended() override {}
    void resumed() override {}
    void unhandledException(const std::exception*, const juce::String&, int) override {}

private:
    PluginHost host_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<JsonRpcServer> rpc_;
};

START_JUCE_APPLICATION(StratumVSTHostApp)
