#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PluginHost;

class Playlist : public juce::Component
{
public:
    Playlist(PluginHost& pluginHost);
    ~Playlist() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    PluginHost& pluginHost_;
    
    int numTracks_ = 13;
    int selectedTrack_ = 0;
    int scrollY_ = 0;
    float zoomX_ = 1.0f;   // Ctrl+scroll horizontal zoom
    
    static constexpr int HEADER_H = 28;
    static constexpr int RULER_H = 24;
    static constexpr int TRACK_H = 28;
    static constexpr int PATTERN_STRIP_W = 68;
    static constexpr int TRACK_LABEL_W = 76;
    static constexpr int BAR_W_BASE = 100;
    int barW() const { return juce::jmax(20, (int)(BAR_W_BASE * zoomX_)); }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Playlist)
};
