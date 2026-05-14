#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <set>

class PluginHost;

// Simple note structure for Piano Roll
struct PianoRollNote
{
    int pitch;
    int startStep;
    int lengthSteps;
    int velocity = 100;
};

class PianoRoll : public juce::Component, private juce::Timer
{
public:
    PianoRoll(PluginHost& pluginHost);
    ~PianoRoll() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    // Set notes for a specific channel
    void setNotes(const std::vector<PianoRollNote>& notes);
    std::vector<PianoRollNote> getNotes() const;
    void setChannelName(const juce::String& name);

    // Playhead (step-based). step < 0 hides the line.
    // bpm is required for smooth interpolation between 16th-note ticks.
    void setPlayhead(int currentStep, bool playing, double bpm = 120.0);
    
    // Callback when notes are modified
    std::function<void()> onNotesChanged;

private:
    PluginHost& pluginHost_;
    
    struct Note { int pitch; int startStep; int lengthSteps; int velocity = 100; };
    std::vector<Note> notes_;
    juce::String channelName_;
    
    int draggingIdx_ = -1;
    bool resizing_ = false;
    juce::Point<int> dragStart_;
    int dragStartStep_ = 0;
    int dragStartPitch_ = 0;
    int dragStartLen_ = 0;

    // Multi-select
    std::set<int>        selectedNotes_;
    bool                 boxSelecting_ = false;
    juce::Point<int>     boxStart_;
    juce::Rectangle<int> boxRect_;
    // Cached start positions for selected notes (for multi-drag)
    std::vector<std::pair<int,int>> dragStartSelected_; // (startStep, pitch) per id in dragStartSelectedIds_
    std::vector<int>                dragStartSelectedIds_;
    
    int scrollX_ = 0;
    int scrollY_ = 0;     // pixels (vertical pitch scroll)
    float zoomX_ = 1.0f;

    int    playStep_    = -1;
    bool   isPlaying_   = false;
    double stepMs_      = 166.67;   // ms per 16th note (derived from BPM)
    double lastTickMs_  = 0.0;      // Time::getMillisecondCounterHiRes() at last tick

    void timerCallback() override;
    
    static constexpr int HEADER_H = 26;
    static constexpr int RULER_H = 24;
    static constexpr int KEY_W = 64;
    static constexpr int KEY_H = 14;       // pixels per semitone
    static constexpr int STEP_W_BASE = 18;  // pixels per 1/16 step
    static constexpr int TOTAL_NOTES = 88;  // A0 (21) to C8 (108) — we use 0..127 internally
    static constexpr int LOWEST_NOTE = 21;
    static constexpr int HIGHEST_NOTE = 108;
    
    int stepW() const { return juce::jmax(4, (int)(STEP_W_BASE * zoomX_)); }
    
    juce::Rectangle<int> getKeyboardRect() const;
    juce::Rectangle<int> getGridRect() const;
    juce::Rectangle<int> getNoteRect(const Note& n) const;
    
    int xToStep(int x) const;
    int yToPitch(int y) const;
    int findNoteAt(int x, int y) const;
    
    static bool isBlackKey(int pitch);
    static juce::String pitchName(int pitch);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRoll)
};
