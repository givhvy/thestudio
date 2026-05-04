#pragma once
#include <juce_core/juce_core.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <unordered_map>
#include <atomic>
#include "PluginHost.h"
#include "AudioEngine.h"
#include "JsonRpcServer.h"

class NativeBridge
{
public:
    NativeBridge (PluginHost& pluginHost, AudioEngine& audioEngine);

    void setBrowser (juce::WebBrowserComponent* browser);

    void handleJSInvoke (const juce::String& channel,
                         const juce::var& args,
                         const juce::String& callbackId);

    void sendEventToJS (const juce::String& channel, const juce::var& data);

    // Synchronous call for HTTP bridge (dev mode) — invokes handler and calls callback with result
    void callSync (const juce::String& channel, const juce::var& args,
                   std::function<void(const juce::var&)> resultCallback);

    // Store a JUCE 8 NativeFunctionCompletion for a given callback ID
    void setPendingCompletion (const juce::String& callbackId,
                               juce::WebBrowserComponent::NativeFunctionCompletion completion);

    std::function<void()> onMinimize;
    std::function<void()> onMaximize;
    std::function<void()> onClose;

private:
    void handleDialogOpenFile (const juce::var& args, const juce::String& callbackId);
    void handleDialogSaveFile (const juce::var& args, const juce::String& callbackId);
    void handleDialogOpenDirectory (const juce::var& args, const juce::String& callbackId);

    void handleFsReadFile (const juce::var& args, const juce::String& callbackId);
    void handleFsWriteFile (const juce::var& args, const juce::String& callbackId);
    void handleFsReadBinaryFile (const juce::var& args, const juce::String& callbackId);
    void handleFsDeleteFile (const juce::var& args, const juce::String& callbackId);
    void handleFsGetProjectsDir (const juce::var& args, const juce::String& callbackId);
    void handleFsListProjects (const juce::var& args, const juce::String& callbackId);
    void handleFsListDirectory (const juce::var& args, const juce::String& callbackId);

    void handleLog (const juce::var& args);
    void handleAppMinimize (const juce::var& args);
    void handleAppMaximize (const juce::var& args);
    void handleAppClose (const juce::var& args);
    void handleOpenDevTools (const juce::var& args);
    void handleWindowMove (const juce::var& args);
    void handleWindowStartDrag (const juce::var& args);
    void handleWindowDrag (const juce::var& args);
    void handleWindowMoveBy (const juce::var& args);

    void handleVstConnect (const juce::var& args, const juce::String& callbackId);
    void handleVstCall (const juce::var& args, const juce::String& callbackId);
    void handleVstScanFolder (const juce::var& args, const juce::String& callbackId);

    void sendCallback (const juce::String& callbackId, const juce::var& result);

    PluginHost& pluginHost;
    AudioEngine& audioEngine;
    juce::WebBrowserComponent* browser = nullptr;
    juce::FileChooser* currentFileChooser = nullptr;

    // Window drag state
    bool isDraggingWindow = false;
    juce::Point<int> dragOffset;

    juce::CriticalSection pendingSyncLock_;
    std::unordered_map<std::string, std::function<void(const juce::var&)>> pendingSyncCallbacks_;
    std::unordered_map<std::string, juce::WebBrowserComponent::NativeFunctionCompletion> pendingCompletions_;
};
