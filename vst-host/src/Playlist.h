#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <set>

class PluginHost;

class Playlist : public juce::Component,
                 public juce::DragAndDropTarget,
                 public juce::FileDragAndDropTarget,
                 private juce::Timer
{
public:
    Playlist(PluginHost& pluginHost);
    ~Playlist() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Smooth playhead. step < 0 or playing==false hides the line.
    // bpm is required for interpolation between 16th-note ticks.
    void setPlayhead(int currentStep, bool playing, double bpm = 120.0);
    void setPlaybackEnabled(bool enabled);
    void setAbsoluteStep(int step);
    void setBPM(double bpm);
    int getAbsoluteStep() const { return absoluteStep_; }
    bool hasPatternClipAtStep(int step) const;
    int patternLocalStepAt(int step, int patternSteps) const;
    std::function<void(int /*absoluteStep*/)> onPlayheadSeek;

    // Provider for the current pattern's step grid. Returns one row per
    // channel; each row is a bool vector of step states. Used to render
    // a mini "what's inside" thumbnail inside each Pattern clip.
    std::function<std::vector<std::vector<bool>>()> getPatternGrid;

    // Project I/O
    juce::var toJson() const;
    void     fromJson(const juce::var& v);

    // ── Drag and drop ──────────────────────────────────────────
    bool isInterestedInDragSource(const SourceDetails& d) override;
    void itemDragEnter(const SourceDetails& d) override;
    void itemDragMove (const SourceDetails& d) override;
    void itemDragExit (const SourceDetails& d) override;
    void itemDropped  (const SourceDetails& d) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    PluginHost& pluginHost_;

    enum class ClipKind { Pattern, Sample };
    struct Clip {
        ClipKind kind     = ClipKind::Pattern;
        int      track    = 0;     // 0-based row index
        float    startBar = 0.0f;  // bar position (0 == bar 1)
        float    lengthBar = 1.0f; // length in bars
        juce::String  label;
        juce::File    sampleFile;  // valid only when kind == Sample
        double   sourceSeconds = 0.0;
        double   sourceBpm = 0.0;
        float    sourceBars = 0.0f;
        bool     tempoSync = false;
        float    volume = 1.0f;
        std::vector<float> waveformPeaks;
        int      lastFiredStep = -1; // for sample one-shot trigger logic
    };

    std::vector<Clip> clips_;
    std::vector<Clip> clipboard_;   // Ctrl+C / Ctrl+V buffer

    int numTracks_ = 30;
    int selectedTrack_ = 0;
    int scrollY_ = 0;
    float zoomX_ = 1.0f;   // Ctrl+scroll horizontal zoom
    double bpm_ = 130.0;
    juce::AudioFormatManager audioFormatManager_;

    int    playStep_   = -1;
    int    absoluteStep_ = 0;
    bool   isPlaying_  = false;
    bool   playbackEnabled_ = false;
    double stepMs_     = 166.67;
    double lastTickMs_ = 0.0;

    // Drag state
    enum class ClipDragMode { None, Move, ResizeStart, ResizeEnd };
    int  draggingClip_   = -1;
    ClipDragMode clipDragMode_ = ClipDragMode::None;
    int  dragGrabBarOffset_ = 0;     // unused (kept for future)
    float dragGrabDeltaBar_ = 0.0f;  // mouse-x → clip-start delta on grab
    float dragStartBar_ = 0.0f;
    float dragStartLengthBar_ = 1.0f;
    bool dragMoved_      = false;
    bool draggingPlayhead_ = false;
    bool draggingClipVolume_ = false;

    // Multi-select (Ctrl+RMB box / Ctrl+click)
    std::set<int>           selectedClips_;
    bool                    boxSelecting_   = false;
    juce::Point<int>        boxStart_;
    juce::Rectangle<int>    boxRect_;

    // DnD highlight
    int  dropHighlightTrack_ = -1;
    int  dropHighlightBar_   = -1;

    void   timerCallback() override;
    int    barW() const { return juce::jmax(20, (int)(BAR_W_BASE * zoomX_)); }
    juce::Rectangle<float> clipRect(const Clip& c) const;
    int    findClipAt(int x, int y) const;
    int    pixelToTrack(int y) const;
    float  pixelToBar(int x) const;          // returns bar position (0-based)
    int    pixelToStep(int x) const;
    int    playheadX() const;
    int    getClipEdgeAt(int x, int y) const;
    int    snapBars(float bar) const { return (int)std::floor(bar); } // 1-bar snap
    void   showClipContextMenu(int clipIdx);
    void   triggerSampleClipsAt(int playStep);
    void   configureSampleClip(Clip& c, const juce::File& file);
    void   drawClipEditor(juce::Graphics& g);
    void   setEditorVolumeFromX(int x);
    juce::Rectangle<int> clipEditorBounds() const;
    
    // Collapse state for the left "Pattern X" strip. When collapsed the
    // strip shrinks to a thin column with just the toggle, so the timeline
    // gets back ~52 px of width.
    bool patternStripCollapsed_ = true;
    juce::String currentPatternName_ = "Pattern 1";
    int editorClip_ = -1;
    juce::Rectangle<int> editorCloseRect_;
    juce::Rectangle<int> editorVolumeRect_;
    juce::Rectangle<int> editorLengthResetRect_;

    int  patternStripW() const { return patternStripCollapsed_ ? 16 : PATTERN_STRIP_W; }
    juce::Rectangle<int> patternToggleRect() const;

    static constexpr int HEADER_H = 28;
    static constexpr int RULER_H = 24;
    static constexpr int TRACK_H = 28;
    static constexpr int PATTERN_STRIP_W = 68;
    static constexpr int TRACK_LABEL_W = 76;
    static constexpr int BAR_W_BASE = 100;
    static constexpr int CLIP_INSET_X = 0;

public:
    void setCurrentPatternName(const juce::String& n) { currentPatternName_ = n; repaint(); }
private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Playlist)
};
