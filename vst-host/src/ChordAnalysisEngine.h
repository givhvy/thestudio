#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <vector>

class Playlist;

class ChordAnalysisEngine
{
public:
    struct BassNote
    {
        int pitch = 36;
        int startStep = 0;
        int lengthSteps = 1;
        int velocity = 100;
    };

    struct Result
    {
        bool ok = false;
        juce::String error;
        double bpm = 0.0;
        juce::String key;
        std::vector<BassNote> notes;
    };

    using Completion = std::function<void(Result)>;

    static juce::File getAnalysisDirectory();
    static juce::File getPythonExecutable();
    static juce::File getAnalyzeScript();
    static bool isReady();

    void analyzeAsync(const juce::File& audioFile,
                      double bpmHint,
                      int maxSteps,
                      Completion onComplete);

    void cancelPending();

private:
    std::atomic<bool> cancelled_ { false };

    static Result parseJsonOutput(const juce::String& jsonText);
    static Result runAnalysis(const juce::File& audioFile, double bpmHint, int maxSteps);
};
