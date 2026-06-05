#pragma once

#include "AgentRegistry.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

class OrgChartPanel : public juce::Component,
                      private juce::Timer
{
public:
    OrgChartPanel();
    ~OrgChartPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    std::function<void(const juce::String& /*agentId*/)> onRunAgent;
    std::function<void()> onRunAllEnabled;

private:
    enum class LayoutMode { Wide, Medium, Narrow };

    struct AgentHitRects
    {
        juce::Rectangle<int> card;
        juce::Rectangle<int> toggle;
        juce::Rectangle<int> run;
    };

    struct LayoutSnapshot
    {
        LayoutMode mode = LayoutMode::Wide;
        int pad = 24;
        int headerH = 62;
        juce::Rectangle<int> runAllRect;
        juce::Rectangle<float> conductorRect;
        std::map<juce::String, juce::Rectangle<float>> deptRects;
        std::map<juce::String, AgentHitRects> agentHits;
        int contentHeight = 0;
    };

    void timerCallback() override;
    LayoutSnapshot buildLayout() const;
    void rebuildLayout();

    static LayoutMode modeForWidth(int w);
    static AgentHitRects makeAgentHits(juce::Rectangle<float> card);
    static void placeAgentCardsForDepartment(const std::vector<AgentDefinition>& defs,
                                             const juce::String& department,
                                             juce::Rectangle<float> deptRect,
                                             float cardH, float cardGap, int cardsPerRow, float yStart,
                                             LayoutSnapshot& layout);
    static float departmentBlockHeight(int agentCount, int cardsPerRow, float cardH, float cardGap, float deptHeaderH);

    LayoutSnapshot layout_;
    int scrollY_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrgChartPanel)
};
