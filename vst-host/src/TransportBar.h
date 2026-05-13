#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PluginHost;

class TransportBar : public juce::Component, public juce::Timer
{
public:
    TransportBar(PluginHost& pluginHost);
    ~TransportBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void timerCallback() override;

    bool isPlaying() const { return isPlaying_; }
    double getBPM() const { return bpm_; }
    
    std::function<void(bool)> onPlayStateChanged;
    std::function<void(double)> onBPMChanged;
    std::function<void()> onPianoToggle;
    std::function<void()> onMixerToggle;
    std::function<void()> onSave;
    std::function<void()> onOpen;
    std::function<void()> onExport;
    std::function<void()> onLog;

private:
    PluginHost& pluginHost_;
    
    std::unique_ptr<juce::Component> playBtn_;
    std::unique_ptr<juce::Component> stopBtn_;
    std::unique_ptr<juce::Component> recordBtn_;
    juce::Slider bpmSlider_;
    juce::Label bpmLabel_;
    juce::Label timeLabel_;
    
    bool isPlaying_ = false;
    bool isRecording_ = false;
    double bpm_ = 130.0;
    
    // View selection state (0 = Piano, 1 = Mixer)
    int selectedView_ = 0;
    
    // Animation state for button transitions
    float animationProgress_ = 1.0f;
    int previousView_ = 0;
    
    // Store button rects for hit detection
    juce::Rectangle<float> pianoBtnRect_;
    juce::Rectangle<float> mixerBtnRect_;
    
    // BPM dragging
    bool isDraggingBPM_ = false;
    int dragStartY_ = 0;
    double dragStartBPM_ = 0.0;
    juce::Rectangle<float> bpmBounds_;
    
public:
    void togglePlay();
    void stop();
    void toggleRecord();
private:
    void updateButtonRects();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
