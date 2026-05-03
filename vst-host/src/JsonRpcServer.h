#pragma once
#include <juce_core/juce_core.h>
#include <functional>

// Simple line-delimited JSON-RPC server over a TCP socket.
// Electron connects to port 9001 (or STRATUM_VST_PORT env var).
// Each line in = one JSON request.
// Each line out = one JSON response.
//
// Request:  { "id": 1, "method": "scanPlugins", "params": { "path": "C:/VST3" } }
// Response: { "id": 1, "result": [...] }  OR  { "id": 1, "error": "..." }

class JsonRpcServer : public juce::Thread
{
public:
    using Handler = std::function<juce::var(const juce::var& params)>;

    explicit JsonRpcServer(int port = 9001);
    ~JsonRpcServer() override;

    void registerMethod(const juce::String& name, Handler h);
    void broadcastEvent(const juce::String& event, const juce::var& data);

    void run() override;

private:
    void handleClient(juce::StreamingSocket* client);

    int port_;
    std::unordered_map<std::string, Handler> handlers_;
    juce::OwnedArray<juce::StreamingSocket> clients_;
    juce::CriticalSection clientLock_;
};
