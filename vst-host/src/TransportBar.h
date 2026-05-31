#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PluginHost;

class TransportBar : public juce::Component, public juce::Timer
{
public:
    enum class PlaybackMode { Rack, Playlist };

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
    PlaybackMode getPlaybackMode() const { return playbackMode_; }
    
    std::function<void(bool)> onPlayStateChanged;
    std::function<void(PlaybackMode)> onPlaybackModeChanged;
    std::function<void(double)> onBPMChanged;
    std::function<void(double /*bpm*/, juce::Rectangle<int> /*anchorScreenArea*/)> onFindLoopsInBpmRange;
    std::function<void()>    onPianoToggle;
    std::function<void()>    onMixerToggle;
    std::function<void()>    onPlaylistToggle;
    std::function<void()>    onOrgChartToggle;
    std::function<void()>    onConsistencyToggle;
    std::function<void()>    onSettingsClicked;
    // Fired when the user scrolls the mouse wheel over the PIANO tab button.
    // delta = -1 (scroll up → previous channel) or +1 (scroll down → next channel).
    std::function<void(int)> onPianoTabScroll;
    std::function<void()> onSave;
    std::function<void()> onOpen;
    // Right-click "Render Video for this Beat" on the BEATS STUDIO button:
    // export the current beat and drive Beats Studio's Create tab to render.
    std::function<void()> onRenderVideoForBeat;

    // ── Create-Video button state (left of BEATS STUDIO) ───────────────
    // Idle    → "＋ Create Video" (click starts a render)
    // Rendering → progress bar 0-100%
    // Ready   → "▶ Open Video"   (click opens the rendered file)
    enum class VideoRenderState { Idle, Rendering, Ready };
    void startVideoRender();                              // begin (sets Rendering)
    void setVideoRenderProgress(int pct);                // update %
    void setVideoRenderDone(const juce::String& path);   // Ready
    void setVideoRenderIdle();                           // reset to Idle
    std::function<void()> onUploadToCloud;
    std::function<void()> onNewProject;
    // Pattern callbacks
    std::function<void(int)>           onPatternSelected;  // user picked a pattern
    std::function<void(juce::String)>  onPatternAdded;     // a new pattern was created (passed its name)

    // Pattern accessors (so other components stay in sync)
    juce::StringArray getPatterns()       const { return patterns_; }
    int               getCurrentPattern() const { return currentPattern_; }
    void              setCurrentPattern(int idx);
    int               addPattern(const juce::String& name = {});
    void              setPlaybackMode(PlaybackMode mode);

    // Project I/O
    juce::var toJson() const;
    void     fromJson(const juce::var& v);

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
    PlaybackMode playbackMode_ = PlaybackMode::Rack;
    double bpm_ = 130.0;

    // Elapsed-playback clock (drives the LCD time readout).
    double playElapsedSeconds_ = 0.0;   // accumulated time while playing
    double playStartMs_        = 0.0;   // wall-clock ms at the current play start
    juce::String formatElapsed() const;
    
    // View selection state (1 = Mixer, 2 = Playlist, 4 = Org Chart, 3 = Consistency)
    int selectedView_ = 2;
    
    // Animation state for button transitions
    float animationProgress_ = 1.0f;
    int previousView_ = 0;
    
    // Store button rects for hit detection
    juce::Rectangle<float> pianoBtnRect_;
    juce::Rectangle<float> mixerBtnRect_;
    juce::Rectangle<float> playlistBtnRect_;
    juce::Rectangle<float> orgChartBtnRect_;
    juce::Rectangle<float> consistencyBtnRect_;
    juce::Rectangle<float> settingsBtnRect_;
    juce::Rectangle<float> playbackModeRect_;
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
    juce::Rectangle<float> beatsStudioBtnRect_;
    juce::Rectangle<float> createVideoBtnRect_;
    VideoRenderState videoState_ = VideoRenderState::Idle;
    int         videoProgress_ = 0;
    juce::String videoOutputPath_;
    bool isBeatsAppRunning_ = false;
    
public:
    void togglePlay();
    void stop();
    void toggleRecord();
    void setBPM(double bpm);

    // Sync the LCD clock to the real playback position (in 16th-note steps),
    // so the readout always matches the playlist/piano-roll scrubber.
    void setPlaybackStep(int absoluteStep, bool playing);
private:
    void updateButtonRects();
    void drawCreateVideoButton(juce::Graphics& g, juce::Rectangle<float> r);
    void checkBeatsAppStatus();
    void sendBridgeCommand(const juce::String& jsonCommand);
    
    struct BeatsAppChecker : public juce::Timer
    {
        BeatsAppChecker(std::function<void()> callback) : cb(callback) { startTimer(2000); }
        void timerCallback() override { cb(); }
        std::function<void()> cb;
    };
    std::unique_ptr<BeatsAppChecker> beatsChecker_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
