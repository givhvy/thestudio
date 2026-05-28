#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

class ConsistencyPanel : public juce::Component
{
public:
    ConsistencyPanel();
    ~ConsistencyPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Call on app launch to record session start.
    static void recordSessionStart();

    // Call on app close to record session end.
    static void recordSessionEnd();

    static juce::File sessionsFile();

private:
    static juce::var  loadJson();
    static void       saveJson(const juce::var& json);

    // Returns map of "YYYY-MM-DD" → total minutes for that day.
    std::map<juce::String, int> buildDayMinutesMap() const;

    int    computeStreak(const std::map<juce::String, int>& dayMap) const;
    int    countTotalSessions() const;
    double computeAvgTimePerDay(const std::map<juce::String, int>& dayMap) const;

    static juce::Colour heatColour(int minutes);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConsistencyPanel)
};
