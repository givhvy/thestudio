#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <map>
#include <vector>

namespace AgentIds
{
    inline constexpr const char* chordifyLoop    = "chordify_loop";
    inline constexpr const char* bassAnalysis    = "bass_analysis";
    inline constexpr const char* pinterest       = "pinterest";
    inline constexpr const char* createVideo     = "create_video";
    inline constexpr const char* beatstars       = "beatstars";
    inline constexpr const char* cloudUpload     = "cloud_upload";
}

struct AgentDefinition
{
    juce::String id;
    juce::String name;
    juce::String department;
    juce::String description;
};

enum class AgentJobState { Idle, Running, Success, Failed };

struct AgentRuntime
{
    AgentJobState state = AgentJobState::Idle;
    int progress = 0;
    juce::String statusMessage;
    juce::int64 lastUpdateMs = 0;
};

class AgentRegistry
{
public:
    static AgentRegistry& get();

    const std::vector<AgentDefinition>& definitions() const { return definitions_; }
    AgentRuntime runtimeFor(const juce::String& agentId) const;

    void setJobRunning(const juce::String& agentId, const juce::String& message = {});
    void setJobProgress(const juce::String& agentId, int progressPct, const juce::String& message = {});
    void setJobDone(const juce::String& agentId, const juce::String& message = {});
    void setJobFailed(const juce::String& agentId, const juce::String& message = {});
    void setJobIdle(const juce::String& agentId);

    bool isAgentEnabled(const juce::String& agentId) const;
    void setAgentEnabled(const juce::String& agentId, bool enabled);
    std::vector<juce::String> enabledAgentIds() const;
    void loadSettings();
    void saveSettings() const;

    std::function<void()> onChanged;

private:
    AgentRegistry();

    std::vector<AgentDefinition> definitions_;
    mutable juce::CriticalSection lock_;
    std::map<juce::String, AgentRuntime> runtime_;
    std::map<juce::String, bool> enabled_;

    void notifyChanged();
    void touchRuntime(const juce::String& agentId);
};
