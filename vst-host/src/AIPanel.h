#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "PatternsPanel.h"

// Floating "AI" panel:
//  - chat-style transcript at top
//  - Presets tab: genre quick buttons
//  - Artist tab: artist list + pattern variants from PatternsPanel library
class AIPanel : public juce::Component
{
public:
    AIPanel();
    ~AIPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    std::function<void(const juce::String& presetId, const juce::String& presetLabel)> onPreset;
    std::function<void(const PatternsPanel::PatternDefinition& pattern)> onPatternVariant;
    std::function<bool(const juce::String& presetId, const juce::String& presetLabel)> onRerollSounds;
    std::function<void()> onClose;

    void addUserMessage(const juce::String& text);
    void addAssistantMessage(const juce::String& text);

private:
    struct PresetBtn { juce::String id; juce::String label; juce::Rectangle<int> rect; };
    struct ChatLine  { bool fromUser; juce::String text; };
    struct ArtistPatternRow { PatternsPanel::PatternDefinition def; juce::Rectangle<int> rect; };

    static constexpr int HEADER_H   = 32;
    static constexpr int CHAT_PAD   = 10;
    static constexpr int FOOTER_H   = 22;
    static constexpr int TAB_BAR_H  = 30;
    static constexpr int BTN_H      = 30;
    static constexpr int BTN_GAP    = 8;
    static constexpr int BTN_COLS   = 4;
    static constexpr int ARTIST_LIST_W = 112;
    static constexpr int ARTIST_PATTERN_ROW_H = 46;

    juce::Rectangle<int> closeBtnRect_;
    juce::Rectangle<int> presetsTabRect_;
    juce::Rectangle<int> artistTabRect_;
    juce::Rectangle<int> browserAreaRect_;

    int activeTab_ = 0;
    std::vector<PresetBtn> buttons_;
    std::vector<ChatLine>  chat_;

    juce::StringArray artists_;
    juce::String selectedArtist_;
    std::vector<PatternsPanel::PatternDefinition> artistLibrary_;
    std::vector<ArtistPatternRow> artistPatternRows_;
    juce::Rectangle<int> artistRects_[24];
    int artistPatternScrollY_ = 0;

    juce::ComponentDragger dragger_;
    bool isDraggingPanel_ = false;

    void rebuildArtistPatternRows();
    std::vector<int> visibleArtistPatternIndices() const;
    void layoutPresetButtons(juce::Rectangle<int> area);
    void drawChat(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTabs(juce::Graphics& g, juce::Rectangle<int> tabsArea);
    void drawPresetBrowser(juce::Graphics& g, juce::Rectangle<int> area);
    void drawArtistBrowser(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIPanel)
};
