#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PatternsPanel.h"
#include <array>
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
        float volume = 1.0f;
        float pan = 0.0f;
        juce::File sampleFile; // optional drag-dropped audio file
        bool isMusicLoop = false;
        int loopSlot = 0;
        std::vector<float> waveformPeaks;

        // Mixer track index this channel is routed through. -1 = "auto" =
        // use the channel's row index (so row 0 → track 0, row 1 → track 1,
        // etc.). Out-of-range values fall back to the master bus.
        int mixerTrack = -1;

        // If non-negative, step triggers send MIDI notes to this loaded
        // plugin slot (e.g. a VST instrument like Kontakt). Otherwise the
        // channel falls back to its built-in drum synth or sample file.
        int pluginSlotId = -1;
        juce::String builtInInstrument;

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
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    // Callback when a channel name is clicked
    std::function<void(int channelIndex)> onChannelClicked;

    // Callback when the channel's index number (left-most column) is clicked.
    // Used to jump to that channel's mixer track (FL Studio-style).
    std::function<void(int channelIndex)> onChannelIndexClicked;
    
    // Callback when a channel's pattern/notes change (e.g. step toggled)
    std::function<void(int channelIndex)> onChannelDataChanged;

    // Callback when the channel list changes (add/remove/rename/reorder)
    std::function<void()> onChannelsChanged;
    std::function<void(int /*deletedChannelIndex*/)> onChannelDeleted;

    // Fires on every step advance; payload is (absoluteStep, isPlaying).
    std::function<void(int /*absoluteStep*/, bool /*playing*/)> onPlayheadTick;
    std::function<bool(int /*absoluteStep*/)> shouldPlayStep;
    std::function<int(int /*absoluteStep*/, int /*patternSteps*/)> getPlaybackStep;
    std::function<int(int /*absoluteStep*/, int /*patternSteps*/, int /*channelIndex*/)> getPlaybackStepForChannel;
    std::function<bool(int /*absoluteStep*/, int /*channelIndex*/)> shouldPlayChannelAtStep;
    std::function<bool()> isPlaylistPlaybackActive;

    int getCurrentStep() const { return currentStep_; }
    int getAbsoluteStep() const { return absoluteStep_; }
    int getTotalSteps() const { return totalSteps_; }
    bool getIsPlaying() const { return isPlaying_; }
    void setCurrentPatternName(const juce::String& name) { currentPatternName_ = name; repaint(); }

    // Local-coordinate rect of the bottom "+" VST-instrument button.
    // Empty if there isn't enough space to draw it.
    juce::Rectangle<int> getAddVstButtonBounds() const;
    
    // Default pitch for drum-style step hits (C5)
    static constexpr int DEFAULT_DRUM_PITCH = 60;
    
    // Callbacks for header buttons
    std::function<void()> onAddChannel;
    std::function<void()> onToggle16_32;
    std::function<void()> onStepGraph;
    std::function<void()> onAddPattern;
    std::function<void()> onAddInstrument;
    std::function<void()> onSplitPatternToPlaylist;
    std::function<void(const juce::String& presetId, const juce::String& presetLabel)> onDrumGenreButtonClicked;

    // Fires when the user clicks the bottom "+" button to add a new VST
    // instrument channel (FL Studio-style). Implementer should show a plugin
    // picker, load the chosen plugin, then push a new Channel whose
    // pluginSlotId is set to that slot.
    std::function<void()> onAddVstChannel;
    std::function<void(const PatternsPanel::PatternDefinition& pattern)> onDrumPatternVariantClicked;
    
    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;
    
    void setPlaying(bool playing);
    void setBPM(double bpm);
    void setPlaybackAudible(bool audible) { playbackAudible_ = audible; }
    void setPianoRealFeel(bool enabled) { pianoRealFeel_ = enabled; }
    void setAbsoluteStep(int step);
    void toggleStepCount();
    void timerCallback() override;
    
    // Access channels for Piano Roll sync
    std::vector<Channel>& getChannels() { return channels_; }
    int getSelectedChannel() const { return selectedChannel_; }
    void setSelectedChannel(int channelIndex) { selectedChannel_ = juce::jlimit(-1, (int)channels_.size() - 1, channelIndex); repaint(); }
    bool deleteChannel(int channelIndex);
    void auditionChannel(int channelIndex);
    // Audition a single piano-roll note on the given channel. Routes through
    // the exact same sample/synth/plugin pathway as the sequencer, so the
    // sound is identical to step playback.
    void auditionPianoRollNote(int channelIndex, int pitch, int lengthSteps, int velocity);
    static juce::String musicLoopSlotLabel(int loopSlot);
    void syncMusicLoopChannels(const std::vector<std::pair<juce::File, juce::String>>& loopsInOrder);
    int getMusicLoopChannelCount() const;
    int getMixerTrackForLoopFile(const juce::File& file) const;
    juce::String getChannelStripNumber(int channelIndex) const;
    int getDrumChannelIndexAmongDrums(int channelIndex) const;

    // Project I/O
    juce::var toJson() const;
    void     fromJson(const juce::var& v);

    using PatternGrid = std::array<std::array<int, 16>, 4>;
    void applyStepPattern(const juce::String& title, const PatternGrid& grid);
    void applyStepPatternToExistingRows(const PatternGrid& grid);
    void applyPatternLaneToExistingRows(const juce::String& title, int rowIndex, const std::array<int, 16>& steps);
    int applyExtractedBassMidi(const juce::String& sourceName, const std::vector<Channel::Note>& notes, int targetChannel = -1);
    int applyPlaylist808Midi(const juce::String& sourceName, const std::vector<Channel::Note>& notes);
    int applyWaitFor808Midi(const std::vector<Channel::Note>& notes);
    int applyExtractedChordifyMidi(const juce::String& sourceName, const std::vector<Channel::Note>& notes, int targetChannel = -1);
    bool setChannelToNativeBass(int channelIndex);
    bool rerollDrumSamples(const juce::String& presetId, juce::StringArray* outMissing = nullptr);
    bool rerollHiHatPattern();
    bool rerollHiHatSample(juce::StringArray* outMissing = nullptr);
    bool rerollChannelSample(int channelIndex, juce::StringArray* outMissing = nullptr);

    // Apply a named drum preset. Returns true if the preset was found and applied.
    // If the preset has a configured sample folder, auto-assigns a matching audio
    // file to each row's channel. Rows for which no matching file was found are
    // appended to `outMissing` (if non-null) as the row's display name.
    bool applyDrumPreset(const juce::String& presetId, juce::StringArray* outMissing = nullptr);
    static juce::StringArray getAvailableDrumPresets();
    // Returns the natural BPM for a preset, or 0 if not found / no specific tempo (e.g. "empty").
    static double getPresetBPM(const juce::String& presetId);

    // (id, label, absolute sample folder). folder is empty if the preset
    // has no configured drum-kit folder (steps-only preset).
    struct DrumPresetFolderInfo { juce::String id; juce::String label; juce::String folder; };
    static std::vector<DrumPresetFolderInfo> getDrumPresetFolders();

    // ── Multi-kit drum-path configuration (Drum Path tab) ─────────────
    // Each genre preset can have multiple linked drum-kit folders and a
    // selection mode controlling which folder(s) feed applyDrumPreset().
    enum class DrumPathMode { All = 0, Randomize = 1, Specific = 2 };
    struct DrumPathConfig {
        juce::String   id;            // preset id (e.g. "boom_bap")
        juce::String   label;         // human label (e.g. "Boom Bap")
        juce::StringArray folders;    // absolute paths to drum-kit folders
        DrumPathMode   mode = DrumPathMode::All;
        int            specificIndex = 0;
    };

    static std::vector<DrumPathConfig> getDrumPathConfigs();
    static DrumPathConfig getDrumPathConfig(const juce::String& presetId);
    static void           addDrumPathFolder(const juce::String& presetId, const juce::String& absolutePath);
    static void           removeDrumPathFolder(const juce::String& presetId, int folderIndex);
    static void           setDrumPathMode(const juce::String& presetId, DrumPathMode mode, int specificIndex);
    static void           saveDrumPathConfigs();
    static void           loadDrumPathConfigs();
    static juce::File     drumPathConfigFile();

    enum class SwingPreset { None, Dilla, MfDoom, JoeyBadass };
    SwingPreset getSwingPreset() const { return swingPreset_; }
    double getSwingDelaySeconds(int stepIndex, const Channel& channel) const;

private:
    PluginHost& pluginHost_;
    
    std::vector<Channel> channels_;
    int currentStep_ = 0;
    int absoluteStep_ = 0;
    // Default: 8 BAR (128 steps @ 16 steps per bar). Toggles between 64 (4 BAR)
    // and 128 (8 BAR). Old "16 STEP / 32 STEP" header replaced by bar labels.
    int totalSteps_ = 128;
    int selectedChannel_ = -1;
    bool isPlaying_ = false;
    bool playbackAudible_ = true;
    uint64_t playbackEpoch_ = 0;
    bool pianoRealFeel_ = false;
    bool draggingHeaderVolume_ = false;
    int headerVolumeDragStartY_ = 0;
    float headerVolumeDragStartValue_ = 1.0f;
    int pendingPatternDragChannel_ = -1;
    bool pendingChannelNameClick_ = false;
    bool startedPatternChannelDrag_ = false;
    double bpm_ = 130.0;
    int dropHighlightRow_ = -1;
    juce::String currentPatternName_ = "Pattern 1";
    juce::String currentDrumPresetId_ = "none";
    SwingPreset swingPreset_ = SwingPreset::None;
    int hiHatVariantCounter_ = 0;
    
    juce::ComponentDragger dragger_;
    bool isDraggingPanel_ = false;

    // Bottom-edge resize handle so user can drag to grow/shrink the rack.
    std::unique_ptr<juce::ResizableEdgeComponent> bottomResizer_;
    juce::ComponentBoundsConstrainer            sizeConstrainer_;
    
    void triggerChannel(int channelIdx, int playbackStep = -1);
    void triggerChannelImpl(int channelIdx, int playbackStep);
    void auditionSelectedChannelC5();
    void drawChannel(juce::Graphics& g, juce::Rectangle<int> bounds, int channelIndex);
    bool isMelodicChannel(const Channel& channel) const;
    bool isMusicLoopChannel(const Channel& channel) const;
    void buildWaveformPeaksForChannel(Channel& channel);
    int getChannelPatternLength(const Channel& channel) const;
    int getChannelAtY(int y) const;
    int getStepAtX(int x) const;
    juce::Rectangle<int> getAddVstButtonRect() const;
    juce::Rectangle<int> getHiHatChangeButtonRect(int channelIndex) const;
    juce::Rectangle<int> getMidiButtonRect(int channelIndex) const;
    void showMidiPatternMenu(int channelIndex);
    void showChannelContextMenu(int channelIndex, juce::Rectangle<int> targetArea);
    static int libraryPatternRowForChannel(const Channel& channel);
    juce::Rectangle<int> getCloseButtonRect() const;
    juce::Rectangle<int> getHeaderVolumeRect() const;
    juce::Rectangle<int> getDrumGenreButtonRect() const;
    juce::Rectangle<int> getSwingButtonRect() const;
    juce::Rectangle<int> getSplitButtonRect() const;
    juce::String getCurrentDrumPresetLabel() const;
    juce::String getSwingPresetLabel() const;
    void showDrumPresetMenu();
    void showDrumPatternVariantMenu();
    void showSwingMenu();
    void setSwingPreset(SwingPreset preset);
    void setSelectedChannelVolumeFromDrag(int startY, int currentY, float startValue);
    void showHeaderVolumeMenu();
    void applyDefaultChannelSettings(int channelIndex);
    bool isHiHatChannel(int channelIndex) const;
    int getRequiredWidthForSteps() const;
    void fitWidthToStepCount();

    // Adaptive step-cell sizing so the rack stays visible at 4/8-bar lengths.
    // With 16 steps we keep the original 18-px cells; for longer patterns the
    // cells shrink so the rack fits in a typical desktop window.
    int stepCellWidth() const;
    int stepCellGap() const;
    int beatCellGap() const;

    // The two supported pattern lengths (in 16th-note steps) — the header
    // toggles between these. 64 = 4 BAR, 128 = 8 BAR.
    static constexpr int STEPS_4_BAR = 64;
    static constexpr int STEPS_8_BAR = 128;
    
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
