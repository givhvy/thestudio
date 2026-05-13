#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

class PluginHost;

class ChannelRack : public juce::Component, public juce::Timer, public juce::DragAndDropTarget
{
public:
    enum class InstrumentType { Kick, Snare, Hihat, Clap, Bass, Lead, Pad };
    
    struct Channel
    {
        juce::String name;
        InstrumentType type;
        std::vector<bool> steps;
        bool muted = false;
        bool solo = false;
        float volume = 0.8f;
        float pan = 0.0f;
        juce::File sampleFile; // optional drag-dropped audio file
        
        // Piano roll notes for this channel
        struct Note { int pitch; int startStep; int lengthSteps; int velocity = 100; };
        std::vector<Note> pianoRollNotes;
    };

    ChannelRack(PluginHost& pluginHost);
    ~ChannelRack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
    // Callback when a channel name is clicked
    std::function<void(int channelIndex)> onChannelClicked;
    
    // Callback when a channel's pattern/notes change (e.g. step toggled)
    std::function<void(int channelIndex)> onChannelDataChanged;

    // Fires on every step advance; payload is (currentStep, isPlaying).
    std::function<void(int /*currentStep*/, bool /*playing*/)> onPlayheadTick;

    int getCurrentStep() const { return currentStep_; }
    bool getIsPlaying() const { return isPlaying_; }
    
    // Default pitch for drum-style step hits (C5)
    static constexpr int DEFAULT_DRUM_PITCH = 60;
    
    // Callbacks for header buttons
    std::function<void()> onAddChannel;
    std::function<void()> onToggle16_32;
    std::function<void()> onStepGraph;
    std::function<void()> onAddPattern;
    std::function<void()> onAddInstrument;
    
    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;
    
    void setPlaying(bool playing);
    void setBPM(double bpm);
    void timerCallback() override;
    
    // Access channels for Piano Roll sync
    std::vector<Channel>& getChannels() { return channels_; }
    int getSelectedChannel() const { return selectedChannel_; }

    // Apply a named drum preset (e.g. "boom_bap", "trap", "hiphop", "house",
    // "drill", "rnb", "lofi"). Returns true if the preset was found and applied.
    // If the preset has a configured sample folder, auto-assigns a matching audio
    // file to each row's channel. Rows for which no matching file was found are
    // appended to `outMissing` (if non-null) as the row's display name.
    bool applyDrumPreset(const juce::String& presetId, juce::StringArray* outMissing = nullptr);
    static juce::StringArray getAvailableDrumPresets();
    // Returns the natural BPM for a preset, or 0 if not found / no specific tempo (e.g. "empty").
    static double getPresetBPM(const juce::String& presetId);

private:
    PluginHost& pluginHost_;
    
    std::vector<Channel> channels_;
    int currentStep_ = 0;
    int totalSteps_ = 16;
    int selectedChannel_ = -1;
    bool isPlaying_ = false;
    double bpm_ = 130.0;
    int dropHighlightRow_ = -1;
    
    juce::ComponentDragger dragger_;
    bool isDraggingPanel_ = false;

    // Bottom-edge resize handle so user can drag to grow/shrink the rack.
    std::unique_ptr<juce::ResizableEdgeComponent> bottomResizer_;
    juce::ComponentBoundsConstrainer            sizeConstrainer_;
    
    void triggerChannel(int channelIdx);
    void drawChannel(juce::Graphics& g, juce::Rectangle<int> bounds, int channelIndex);
    int getChannelAtY(int y) const;
    int getStepAtX(int x) const;
    
    // React design constants
    static constexpr int HEADER_HEIGHT = 30;
    static constexpr int CHANNEL_HEIGHT = 28;
    static constexpr int CH_INDEX_WIDTH = 24;
    static constexpr int CH_LED_SIZE = 9;
    static constexpr int CH_MUTE_SIZE = 11;
    static constexpr int CH_NAME_WIDTH = 78;
    static constexpr int STEP_WIDTH = 18;
    static constexpr int STEP_HEIGHT = 22;
    static constexpr int STEP_GAP = 2;
    static constexpr int BEAT_GAP = 6;  // Extra gap every 4 steps
    static constexpr int LEFT_PADDING = 6;
    static constexpr int CHANNELS_START_X = LEFT_PADDING + CH_INDEX_WIDTH + 6 + CH_LED_SIZE + 6 + CH_MUTE_SIZE + 4 + CH_MUTE_SIZE + 6 + CH_NAME_WIDTH + 8;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRack)
};
