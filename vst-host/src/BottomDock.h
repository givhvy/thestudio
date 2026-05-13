#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class BottomDock : public juce::Component
{
public:
    BottomDock();
    ~BottomDock() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
    // Set which button is selected (0-5, or -1 for none)
    void setSelectedButton(int index);
    
    // Callbacks for quick tools buttons
    std::function<void()> onMixer;
    std::function<void()> onPianoRoll;
    std::function<void()> onChannelRack;
    std::function<void()> onPlugins;
    std::function<void()> onVideo;
    std::function<void()> onAI;

    // Live data providers for the MIXER PREVIEW panel.
    // If unset, the panel falls back to placeholder values.
    std::function<int()>                         getMixerTrackCount;
    std::function<juce::String(int)>             getMixerTrackName;
    std::function<float(int)>                    getMixerTrackVolume;
    std::function<bool(int)>                     getMixerTrackMuted;
    std::function<void(int /*track*/, float /*vol*/)> setMixerTrackVolume;

private:
    void storeButtonRects();
    juce::Rectangle<float> buttonRects_[6];
    int selectedButtonIndex_ = -1;

    // Cached MIXER PREVIEW geometry, set during paint, used by mouse handlers.
    juce::Rectangle<int> previewPanelRect_;
    int previewStripW_ = 0;
    int previewFaderTop_ = 0;
    int previewFaderBot_ = 0;
    int previewNumTracks_ = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomDock)
};
