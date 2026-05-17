#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <vector>

class PatternsPanel : public juce::Component
{
public:
    using PatternGrid = std::array<std::array<int, 16>, 4>;

    struct PatternDefinition
    {
        juce::String id;
        juce::String presetId;
        juce::String genre;
        juce::String title;
        juce::String feel;
        int bpm = 90;
        PatternGrid rows {};
        bool useFullPresetRows = false;
    };

    PatternsPanel();
    ~PatternsPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;

    std::function<void(const PatternDefinition& pattern)> onApplyPattern;
    std::function<void()> onClose;

    static std::vector<PatternDefinition> getPatternLibrary();
    static std::vector<PatternDefinition> getPatternsForPreset(const juce::String& presetId);

private:
    struct PatternEntry
    {
        PatternDefinition def;
        juce::Rectangle<int> rect;
    };

    static constexpr int HEADER_H = 32;
    static constexpr int FOOTER_H = 46;
    static constexpr int GENRE_W = 150;
    static constexpr int ROW_H = 56;

    std::vector<PatternEntry> patterns_;
    juce::StringArray genres_;
    juce::String selectedGenre_ = "Boom Bap";
    int selectedPattern_ = 0;
    int listScrollY_ = 0;

    juce::Rectangle<int> closeBtnRect_;
    juce::Rectangle<int> applyBtnRect_;
    juce::Rectangle<int> genreRects_[32];

    juce::ComponentDragger dragger_;
    bool isDraggingPanel_ = false;

    std::vector<int> visiblePatternIndices() const;
    const PatternEntry* selectedPattern() const;
    void drawPreview(juce::Graphics& g, juce::Rectangle<int> area, const PatternEntry& p);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternsPanel)
};
