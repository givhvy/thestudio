#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

class PluginHost;

class Mixer : public juce::Component
{
public:
    struct FxSlot
    {
        int pluginSlotId = -1;       // PluginHost slot id, -1 = empty or WASM stub
        juce::String displayName;    // Shown in the FX chain UI
        bool isWasm = false;          // True for built-in/WASM-labelled entries
    };

    struct Track
    {
        juce::String name;
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
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

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

    // Project I/O
    juce::var toJson() const;
    void     fromJson(const juce::var& v);

    // Add an FX slot to the given track. If pluginSlotId >= 0 it's a real plugin instance;
    // pass -1 + a name for a WASM/stub entry. Returns the index of the new slot.
    int addFxToTrack(int trackIdx, int pluginSlotId, const juce::String& displayName, bool isWasm);
    void removeFxFromTrack(int trackIdx, int slotIdx);
    void openPluginPickerForTrack(int trackIdx);

    std::function<void()> onTracksChanged;

    // Fired when the user clicks the X button in the mixer header.
    std::function<void()> onClose;

    // Wide-mode toggle: hides the right detail panel so strips fill the width.
    bool isWideMode() const { return wideMode_; }
    void setWideMode(bool w) { if (w != wideMode_) { wideMode_ = w; repaint(); } }

private:
    PluginHost& pluginHost_;
    std::vector<Track> tracks_;
    int selectedStrip_ = 0;
    bool wideMode_ = false;
    juce::Rectangle<float> btnXRect_, btnWideRect_, btnRouteRect_;
    int hoveredHeaderBtn_ = -1; // 0 = X, 1 = Wide, 2 = Route
    
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
    static constexpr int DETAIL_PANEL_WIDTH = 200;
    
    juce::Rectangle<int> getStripRect(int idx) const;
    juce::Rectangle<int> getPanKnobRect(int idx) const;
    juce::Rectangle<int> getFaderRect(int idx) const;
    juce::Rectangle<int> getReverbRect(int idx) const;
    juce::Rectangle<int> getMuteRect(int idx) const;
    juce::Rectangle<int> getSoloRect(int idx) const;
    juce::Rectangle<int> getDetailPanelRect() const;
    juce::Rectangle<int> getFxSlotRect(int slotIdx) const;
    static constexpr int FX_SLOT_COUNT = 8;
    static constexpr int FX_SLOT_H = 22;
    
    std::unique_ptr<juce::FileChooser> fileChooser_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mixer)
};
