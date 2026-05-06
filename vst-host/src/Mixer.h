#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class PluginHost;

class Mixer : public juce::Component
{
public:
    struct Track
    {
        juce::String name;
        float volume = 0.8f;
        float pan = 0.0f;
        float reverbSend = 0.0f;
        bool muted = false;
        bool solo = false;
    };
    
    Mixer(PluginHost& pluginHost);
    ~Mixer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    PluginHost& pluginHost_;
    std::vector<Track> tracks_;
    int selectedStrip_ = 0;
    
    int draggingTrackIdx_ = -1;
    enum class DragTarget { None, Volume, ReverbSend, Pan };
    DragTarget dragTarget_ = DragTarget::None;
    int dragStartY_ = 0;
    float dragStartValue_ = 0;
    
    // React design constants
    static constexpr int HEADER_HEIGHT = 26;
    static constexpr int STRIP_WIDTH = 52;
    static constexpr int FADER_HEIGHT = 120;
    static constexpr int DETAIL_PANEL_WIDTH = 180;
    
    juce::Rectangle<int> getStripRect(int idx) const;
    juce::Rectangle<int> getPanKnobRect(int idx) const;
    juce::Rectangle<int> getFaderRect(int idx) const;
    juce::Rectangle<int> getReverbRect(int idx) const;
    juce::Rectangle<int> getMuteRect(int idx) const;
    juce::Rectangle<int> getSoloRect(int idx) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mixer)
};
