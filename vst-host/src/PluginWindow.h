#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

// Floating, draggable child component that hosts a plugin's editor INSIDE
// the main app window (FL Studio-style), instead of opening a separate OS
// window. The editor pointer is non-owning; the PluginHost retains
// ownership and will delete it when the slot closes.
class PluginWindow : public juce::Component
{
public:
    PluginWindow(const juce::String& title, juce::AudioProcessorEditor* editor);
    ~PluginWindow() override;

    // Fires when the user clicks the close button on the title bar.
    std::function<void()> onClose;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    juce::String                   title_;
    juce::AudioProcessorEditor*    editor_ = nullptr; // non-owning
    juce::TextButton               closeBtn_;
    juce::ComponentDragger         dragger_;
    bool                           isDraggingTitle_ = false;

    static constexpr int TITLE_H = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
