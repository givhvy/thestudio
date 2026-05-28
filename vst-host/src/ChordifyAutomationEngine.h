#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <vector>

class ChordifyAutomationEngine
{
public:
    struct BassNote
    {
        int pitch = 60;
        int startStep = 0;
        int lengthSteps = 1;
        int velocity = 100;
    };

    struct Result
    {
        bool ok = false;
        juce::String error;
        juce::File midiFile;
        double bpm = 0.0;
        std::vector<BassNote> notes;
    };

    using Completion = std::function<void(Result)>;

    static juce::File getProfileDirectory();
    static juce::File getAutomationScript();
    static juce::File getPythonExecutable();
    static bool isReady();
    static bool isFullyReady();
    static bool isCdpAvailable();
    static void refreshCdpStatus();
    static bool isRunning();
    static juce::File getLockFile();
    static void forceRestart();
    static bool launchChrome();

    void fetchBassAsync(const juce::File& audioFile,
                        double bpmHint,
                        int maxSteps,
                        Completion onComplete);

    void cancelPending();

private:
    std::atomic<bool> cancelled_ { false };
    static std::atomic<bool> running_;

    static Result parseJsonOutput(const juce::String& jsonText);
    static Result runAutomation(const juce::File& audioFile, double bpmHint, int maxSteps);
};
