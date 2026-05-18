#include "PluginEditor.h"

StratumPianoAudioProcessorEditor::StratumPianoAudioProcessorEditor(StratumPianoAudioProcessor& processor)
    : AudioProcessorEditor(&processor), processor_(processor)
{
    title_.setText("Stratum Piano", juce::dontSendNotification);
    title_.setJustificationType(juce::Justification::centredLeft);
    title_.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    title_.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(title_);

    auto setupSlider = [this](juce::Slider& slider, const juce::String& suffix)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xfff97316));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffedd5));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff8fafc));
        slider.setTextValueSuffix(suffix);
        addAndMakeVisible(slider);
    };

    setupSlider(gain_, "");
    setupSlider(tone_, "");
    setupSlider(decay_, "s");

    gainAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.parameters, "gain", gain_);
    toneAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.parameters, "tone", tone_);
    decayAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.parameters, "decay", decay_);

    setSize(360, 190);
}

void StratumPianoAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff09090b));
    auto bounds = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("GAIN", 40, 150, 70, 18, juce::Justification::centred);
    g.drawText("TONE", 145, 150, 70, 18, juce::Justification::centred);
    g.drawText("DECAY", 250, 150, 70, 18, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f));
    g.setColour(processor_.getLoadedSampleCount() > 0 ? juce::Colour(0xff86efac) : juce::Colour(0xfffbbf24));
    const auto status = processor_.getLoadedSampleCount() > 0
        ? juce::String("Samples loaded: ") + juce::String(processor_.getLoadedSampleCount())
        : "No samples found - using synth fallback";
    g.drawText(status, 20, 34, getWidth() - 40, 16, juce::Justification::centredLeft);
}

void StratumPianoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(18);
    title_.setBounds(area.removeFromTop(32));
    auto controls = area.reduced(0, 6);
    gain_.setBounds(controls.removeFromLeft(104).reduced(10, 0));
    tone_.setBounds(controls.removeFromLeft(104).reduced(10, 0));
    decay_.setBounds(controls.removeFromLeft(104).reduced(10, 0));
}
