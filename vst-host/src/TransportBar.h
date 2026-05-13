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
    std::function<void()> onPlaylistToggle;
    std::function<void()> onSave;
    std::function<void()> onOpen;
    std::function<void()> onExport;
    std::function<void()> onLog;
    // Pattern callbacks
    std::function<void(int)>           onPatternSelected;  // user picked a pattern
    std::function<void(juce::String)>  onPatternAdded;     // a new pattern was created (passed its name)

    // Pattern accessors (so other components stay in sync)
    juce::StringArray getPatterns()       const { return patterns_; }
    int               getCurrentPattern() const { return currentPattern_; }
    void              setCurrentPattern(int idx);
    int               addPattern(const juce::String& name = {});

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
    
    // View selection state (0 = Piano, 1 = Mixer, 2 = Playlist)
    int selectedView_ = 0;
    
    // Animation state for button transitions
    float animationProgress_ = 1.0f;
    int previousView_ = 0;
    
    // Store button rects for hit detection
    juce::Rectangle<float> pianoBtnRect_;
    juce::Rectangle<float> mixerBtnRect_;
    juce::Rectangle<float> playlistBtnRect_;
    juce::Rectangle<float> patSelRect_;
    juce::Rectangle<float> patPlusRect_;

    // Patterns
    juce::StringArray patterns_      { "Pattern 1" };
    int               currentPattern_ = 0;

public:
    // Externally sync the active view pill (so clicking bottom-dock Channel Rack / Mixer etc.
    // reflects in the transport toggle group).
    void setSelectedView(int v) { selectedView_ = v; animationProgress_ = 1.0f; repaint(); }
private:
    
    // BPM dragging
    bool isDraggingBPM_ = false;
    int dragStartY_ = 0;
    double dragStartBPM_ = 0.0;
    juce::Rectangle<float> bpmBounds_;
    
public:
    void togglePlay();
    void stop();
    void toggleRecord();
    void setBPM(double bpm);
private:
    void updateButtonRects();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
