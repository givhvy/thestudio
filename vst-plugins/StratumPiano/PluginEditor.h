#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class StratumPianoAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit StratumPianoAudioProcessorEditor(StratumPianoAudioProcessor&);
    ~StratumPianoAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    StratumPianoAudioProcessor& processor_;
    juce::Slider gain_;
    juce::Slider tone_;
    juce::Slider decay_;
    juce::Label title_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StratumPianoAudioProcessorEditor)
};
