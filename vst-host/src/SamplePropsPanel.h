#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "ChannelRack.h"

// FL Studio-style "Channel Sample Settings" panel. Edits a single channel's
// SampleProps in place and fires onChange so the host can re-audition / repaint.
class SamplePropsPanel : public juce::Component
{
public:
    SamplePropsPanel(const juce::File& sampleFile,
                     const juce::String& channelName,
                     ChannelRack::Channel::SampleProps* props);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    // Fired whenever a property changes (so the host can re-trigger the sample).
    std::function<void()> onChange;
    // Fired when the user clicks the preview button.
    std::function<void()> onAudition;

private:
    juce::File   file_;
    juce::String name_;
    ChannelRack::Channel::SampleProps* p_ = nullptr;

    juce::Slider pitchSlider_, fadeInSlider_, fadeOutSlider_;
    juce::Label  pitchLabel_, fadeInLabel_, fadeOutLabel_;

    std::vector<float> peaks_;   // mirrored waveform magnitudes (0..1)

    juce::Rectangle<int> waveRect_;
    juce::Rectangle<float> revBtn_, normBtn_, declickBtn_, pingBtn_, playBtn_;

    void configureKnob(juce::Slider& s, juce::Label& l, const juce::String& text);
    void computePeaks();
    void fireChange();
    bool hitToggle(juce::Point<float> pos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplePropsPanel)
};
