#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginHost.h"

class NativeEffectEditor : public juce::Component
{
public:
    NativeEffectEditor(PluginHost& host, int effectId);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    PluginHost& host_;
    int effectId_ = 0;
    juce::String type_;
    juce::String name_;
    int selectedBand_ = 3;
    int draggingBand_ = -1;

    juce::Slider freqSlider_;
    juce::Slider gainSlider_;
    juce::Slider qSlider_;
    juce::Slider thresholdSlider_;
    juce::Slider preSlider_;
    juce::Slider postSlider_;

    juce::Label freqLabel_;
    juce::Label gainLabel_;
    juce::Label qLabel_;
    juce::Label thresholdLabel_;
    juce::Label preLabel_;
    juce::Label postLabel_;

    juce::Rectangle<int> graphRect_;
    juce::Rectangle<int> presetButtonRect_;
    std::array<juce::Rectangle<float>, 7> bandRects_ {};

    void configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void refreshFromHost();
    void pushEqBand();
    void pushClipper();
    void selectBand(int band);
    void showEqPresetMenu();
    void applyEqPreset(const juce::String& presetId);
    float freqToX(float freq) const;
    float xToFreq(float x) const;
    float gainToY(float gain) const;
    float yToGain(float y) const;
    juce::Colour bandColour(int band) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeEffectEditor)
};

class NativeEffectWindow : public juce::Component
{
public:
    NativeEffectWindow(const juce::String& title, std::unique_ptr<juce::Component> editor);
    ~NativeEffectWindow() override;

    std::function<void()> onClose;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    juce::String title_;
    std::unique_ptr<juce::Component> editor_;
    juce::TextButton closeBtn_;
    juce::ComponentDragger dragger_;
    bool draggingTitle_ = false;
    static constexpr int titleH_ = 25;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeEffectWindow)
};
