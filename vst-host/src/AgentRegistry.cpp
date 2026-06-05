#include "AgentRegistry.h"
#include <juce_events/juce_events.h>

namespace
{
juce::File settingsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Stratum DAW");
    dir.createDirectory();
    return dir.getChildFile("agent-org-chart-settings.json");
}
} // namespace

AgentRegistry::AgentRegistry()
{
    definitions_ = {
        { AgentIds::chordifyLoop, "Chordify Loop", "Music",
          "Drag a loop to Chordify, download 808 bass MIDI" },
        { AgentIds::bassAnalysis, "Bass Analysis", "Music",
          "ML / BTC chord analysis when Chordify is offline" },
        { AgentIds::pinterest, "Pinterest Images", "Media",
          "Download reference images from Pinterest" },
        { AgentIds::createVideo, "Create Video", "Media",
          "Render a beat video in Beats Studio" },
        { AgentIds::beatstars, "BeatStars Publish", "Distribution",
          "Upload the beat to BeatStars" },
        { AgentIds::cloudUpload, "Cloud Upload", "Distribution",
          "Export WAV and publish to cloud" },
    };

    for (const auto& def : definitions_)
    {
        runtime_[def.id] = {};
        enabled_[def.id] = true;
    }
    loadSettings();
}

AgentRegistry& AgentRegistry::get()
{
    static AgentRegistry instance;
    return instance;
}

AgentRuntime AgentRegistry::runtimeFor(const juce::String& agentId) const
{
    const juce::ScopedLock sl(lock_);
    auto it = runtime_.find(agentId);
    return it != runtime_.end() ? it->second : AgentRuntime{};
}

void AgentRegistry::touchRuntime(const juce::String& agentId)
{
    runtime_[agentId].lastUpdateMs = juce::Time::getMillisecondCounterHiRes();
}

void AgentRegistry::notifyChanged()
{
    if (onChanged)
        juce::MessageManager::callAsync([cb = onChanged]() { if (cb) cb(); });
}

void AgentRegistry::setJobRunning(const juce::String& agentId, const juce::String& message)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& rt = runtime_[agentId];
        rt.state = AgentJobState::Running;
        rt.progress = juce::jmax(rt.progress, 5);
        if (message.isNotEmpty())
            rt.statusMessage = message;
        touchRuntime(agentId);
    }
    notifyChanged();
}

void AgentRegistry::setJobProgress(const juce::String& agentId, int progressPct, const juce::String& message)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& rt = runtime_[agentId];
        rt.state = AgentJobState::Running;
        rt.progress = juce::jlimit(0, 100, progressPct);
        if (message.isNotEmpty())
            rt.statusMessage = message;
        touchRuntime(agentId);
    }
    notifyChanged();
}

void AgentRegistry::setJobDone(const juce::String& agentId, const juce::String& message)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& rt = runtime_[agentId];
        rt.state = AgentJobState::Success;
        rt.progress = 100;
        rt.statusMessage = message.isNotEmpty() ? message : "Completed";
        touchRuntime(agentId);
    }
    notifyChanged();
}

void AgentRegistry::setJobFailed(const juce::String& agentId, const juce::String& message)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& rt = runtime_[agentId];
        rt.state = AgentJobState::Failed;
        rt.statusMessage = message.isNotEmpty() ? message : "Failed";
        touchRuntime(agentId);
    }
    notifyChanged();
}

void AgentRegistry::setJobIdle(const juce::String& agentId)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& rt = runtime_[agentId];
        rt.state = AgentJobState::Idle;
        rt.progress = 0;
        rt.statusMessage = {};
        touchRuntime(agentId);
    }
    notifyChanged();
}

bool AgentRegistry::isAgentEnabled(const juce::String& agentId) const
{
    const juce::ScopedLock sl(lock_);
    auto it = enabled_.find(agentId);
    return it == enabled_.end() || it->second;
}

void AgentRegistry::setAgentEnabled(const juce::String& agentId, bool enabled)
{
    {
        const juce::ScopedLock sl(lock_);
        enabled_[agentId] = enabled;
    }
    saveSettings();
    notifyChanged();
}

std::vector<juce::String> AgentRegistry::enabledAgentIds() const
{
    std::vector<juce::String> ids;
    const juce::ScopedLock sl(lock_);
    for (const auto& def : definitions_)
        if (enabled_.count(def.id) == 0 || enabled_.at(def.id))
            ids.push_back(def.id);
    return ids;
}

void AgentRegistry::loadSettings()
{
    const auto file = settingsFile();
    if (! file.existsAsFile())
        return;

    const auto json = juce::JSON::parse(file);
    if (! json.isObject())
        return;

    const juce::ScopedLock sl(lock_);
    if (auto* obj = json.getDynamicObject())
    {
        for (const auto& def : definitions_)
        {
            if (obj->hasProperty(def.id))
                enabled_[def.id] = (bool) obj->getProperty(def.id);
        }
    }
}

void AgentRegistry::saveSettings() const
{
    auto* obj = new juce::DynamicObject();
    {
        const juce::ScopedLock sl(lock_);
        for (const auto& [id, on] : enabled_)
            obj->setProperty(id, on);
    }
    settingsFile().replaceWithText(juce::JSON::toString(juce::var(obj), true));
}
