#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

// Floating "AI" panel:
//  - chat-style transcript at top
//  - quick preset buttons that drop drum patterns into the channel rack
//  - close button (X) in the header
//
// The host (MainComponent) wires `onPreset` to ChannelRack::applyDrumPreset
// and toggles visibility from the BottomDock "AI" quick-tools button.
class AIPanel : public juce::Component
{
public:
    AIPanel();
    ~AIPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // Fired when a preset button is clicked. The host should apply it
    // to the channel rack and (optionally) call addAssistantMessage().
    std::function<void(const juce::String& presetId, const juce::String& presetLabel)> onPreset;

    // Fired when the close button is clicked.
    std::function<void()> onClose;

    void addUserMessage(const juce::String& text);
    void addAssistantMessage(const juce::String& text);

private:
    struct PresetBtn { juce::String id; juce::String label; juce::Rectangle<int> rect; };
    struct ChatLine  { bool fromUser; juce::String text; };

    static constexpr int HEADER_H   = 32;
    static constexpr int CHAT_PAD   = 10;
    static constexpr int FOOTER_H   = 22;
    static constexpr int BTN_H      = 30;
    static constexpr int BTN_GAP    = 8;
    static constexpr int BTN_COLS   = 3;

    juce::Rectangle<int> closeBtnRect_;
    std::vector<PresetBtn> buttons_;
    std::vector<ChatLine>  chat_;

    juce::ComponentDragger dragger_;
    bool isDraggingPanel_ = false;

    void layoutButtons(juce::Rectangle<int> area);
    void drawChat(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIPanel)
};
