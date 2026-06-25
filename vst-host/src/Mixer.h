#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>
#include <array>

class PluginHost;

class Mixer : public juce::Component,
              private juce::Timer
{
public:
    struct FxSlot
    {
        int pluginSlotId = -1;       // PluginHost slot id, -1 = empty or WASM stub
        juce::String displayName;    // Shown in the FX chain UI
        bool isWasm = false;          // True for built-in/WASM-labelled entries
        bool enabled = true;          // False = bypassed but still loaded in the chain
    };

    struct Track
    {
        juce::String name;
        juce::String stripNumber;
        float volume = 0.8f;
        float pan = 0.0f;
        float reverbSend = 0.0f;
        bool muted = false;
        bool solo = false;
        int  routeTo = -1;             // index of track this is routed to (-1 = master/last track)
        std::vector<FxSlot> fxSlots;   // up to 8 FX slots, dynamically grown
    };
    
    Mixer(PluginHost& pluginHost);
    ~Mixer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void timerCallback() override;

    int getNumTracks() const { return (int)tracks_.size(); }
    juce::String getTrackName(int i) const { return (i >= 0 && i < (int)tracks_.size()) ? tracks_[i].name : juce::String(); }
    float getTrackVolume(int i) const { return (i >= 0 && i < (int)tracks_.size()) ? tracks_[i].volume : 0.0f; }
    bool isTrackMuted(int i) const { return (i >= 0 && i < (int)tracks_.size()) ? tracks_[i].muted : false; }
    int getSelectedTrack() const { return selectedStrip_; }
    void setSelectedTrack(int i)
    {
        if (i >= 0 && i < (int)tracks_.size() && i != selectedStrip_)
        {
            selectedStrip_ = i;
            repaint();
        }
    }

    void setTrackVolume(int i, float v);
    void setFxSlotEnabledById(int pluginSlotId, bool enabled);

    // Resize/rename tracks to match channel rack channels. The last track
    // is always kept as the master bus. Existing FX/volume/pan are preserved
    // when tracks overlap.
    void syncFromChannelRack(const std::vector<juce::String>& channelNames,
                             const juce::StringArray& stripNumbers = {});

    // Project I/O
    juce::var toJson() const;
    void     fromJson(const juce::var& v);

    // Add an FX slot to the given track. If pluginSlotId >= 0 it's a real plugin instance;
    // pass -1 + a name for a WASM/stub entry. Returns the index of the new slot.
    int addFxToTrack(int trackIdx, int pluginSlotId, const juce::String& displayName, bool isWasm);
    void removeFxFromTrack(int trackIdx, int slotIdx);
    void openPluginPickerForTrack(int trackIdx);

    std::function<void()> onTracksChanged;
    std::function<void(int /*trackIdx*/, int /*pluginSlotId*/, const juce::String& /*name*/)> onCreateFxAutomation;

    // Fired when the user clicks the X button in the mixer header.
    std::function<void()> onClose;

    // Wide-mode toggle: hides the right detail panel so strips fill the width.
    bool isWideMode() const { return wideMode_; }
    void setWideMode(bool w) { if (w != wideMode_) { wideMode_ = w; repaint(); } }

    // ── Floating window mode (FL-style) ──────────────────────────
    // When maximized the host fills the whole centre area (old behaviour);
    // otherwise the mixer is a draggable / resizable window.
    void setMaximized(bool m);
    bool isMaximized() const { return maximized_; }
    void moved() override;
    // Fired after the user drags or resizes the window so the host can
    // remember the floating bounds. Not called while maximized.
    std::function<void(juce::Rectangle<int>)> onWindowBoundsChanged;
    // Fired when the user clicks the maximize/restore button in the header.
    std::function<void()> onToggleMaximize;

private:
    PluginHost& pluginHost_;
    std::vector<Track> tracks_;
    int selectedStrip_ = 0;
    bool wideMode_ = false;
    juce::Rectangle<float> btnXRect_, btnWideRect_, btnRouteRect_, btnAutoMixRect_, btnAutoFreqRect_, btnSidechainRect_, btnMaxRect_;
    int hoveredHeaderBtn_ = -1; // 0 = X, 1 = Wide, 2 = Route, 3 = AutoMix, 4 = Sidechain, 5 = AutoFreq, 6 = Maximize

    // Floating window state
    bool maximized_ = false;
    juce::ComponentDragger windowDragger_;
    bool draggingWindow_ = false;
    std::unique_ptr<juce::ResizableBorderComponent> windowResizer_;
    juce::ComponentBoundsConstrainer windowConstrainer_;
    bool sidechainOn_ = false;
    juce::String autoFreqGenre_ = "boom bap";
    bool frequencyMixReady_ = false;
    
    int draggingTrackIdx_ = -1;
    enum class DragTarget { None, Volume, ReverbSend, Pan };
    DragTarget dragTarget_ = DragTarget::None;
    int dragStartY_ = 0;
    float dragStartValue_ = 0;
    
    // React design constants
    static constexpr int HEADER_HEIGHT = 26;
    static constexpr int STRIP_WIDTH = 72;
    static constexpr int MASTER_GAP = 14;   // visual gap between Master and Insert strips
    static constexpr int FADER_HEIGHT = 220;
    static constexpr int DETAIL_PANEL_WIDTH = 260;
    
    juce::Rectangle<int> getStripRect(int idx) const;
    juce::Rectangle<int> getPanKnobRect(int idx) const;
    juce::Rectangle<int> getFaderRect(int idx) const;
    juce::Rectangle<int> getReverbRect(int idx) const;
    juce::Rectangle<int> getMuteRect(int idx) const;
    juce::Rectangle<int> getSoloRect(int idx) const;
    juce::Rectangle<int> getDetailPanelRect() const;
    juce::Rectangle<int> getFxSlotRect(int slotIdx) const;
    juce::Rectangle<int> getFxSlotPowerRect(int slotIdx) const;
    static constexpr int FX_SLOT_COUNT = 8;
    static constexpr int FX_SLOT_H = 32;
    
    std::unique_ptr<juce::FileChooser> fileChooser_;

    void pushTrackControlsToHost();
    void showAutoMixMenu();
    void applyAutoMixPreset(const juce::String& genre);
    void showAutoFrequencyMenu();
    void applyAutoFrequencyMix(const juce::String& genre);
    int ensureAutoEqForTrack(int trackIdx);
    void drawFrequencyMixPanel(juce::Graphics& g, juce::Rectangle<int> panel);
    std::array<float, 6> getTargetFrequencyProfile(const juce::String& genre) const;
    std::array<float, 6> getCurrentFrequencyProfile() const;
    // Toggle auto-sidechain: finds the "kick" track and the "808"/"bass" track
    // by name and tells the host to duck the latter from the former.
    void toggleAutoSidechain();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mixer)
};
