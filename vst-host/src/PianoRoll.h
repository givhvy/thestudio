#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
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

class PianoRoll : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Timer
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
    void mouseMove(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    
    // Set notes for a specific channel
    void setNotes(const std::vector<PianoRollNote>& notes);
    std::vector<PianoRollNote> getNotes() const;
    void setChannelName(const juce::String& name);
    void setChannelContext(bool isKickChannel, bool is808Channel);
    void setDrumLaneNames(const juce::StringArray& laneNames, int topPitch = 84);

    // Playhead (step-based). step < 0 hides the line.
    // bpm is required for smooth interpolation between 16th-note ticks.
    void setPlayhead(int currentStep, bool playing, double bpm = 120.0);
    int getPlayheadStep() const { return playStep_; }
    
    // Callback when notes are modified
    std::function<void()> onNotesChanged;
    std::function<void(int /*absoluteStep*/)> onPlayheadSeek;
    std::function<void(int /*pitch*/, int /*lengthSteps*/, int /*velocity*/)> onAuditionNote;
    std::function<void(int /*bpm*/)> onGeneratedMidiBpm;
    std::function<void(bool /*enabled*/)> onRealFeelChanged;
    std::function<void()> onPasteFrom808Requested;

private:
    PluginHost& pluginHost_;
    
    struct Note { int pitch; int startStep; int lengthSteps; int velocity = 100; };
    std::vector<Note> notes_;
    juce::String channelName_;
    bool isKickChannel_ = false;
    bool is808Channel_ = false;
    bool draggingMidiOver_ = false;
    juce::StringArray drumLaneNames_;
    int drumLaneTopPitch_ = 84;
    
    int draggingIdx_ = -1;
    bool resizing_ = false;
    juce::Point<int> dragStart_;
    int dragStartStep_ = 0;
    int dragStartPitch_ = 0;
    int dragStartLen_ = 0;

    // Multi-select
    std::set<int>        selectedNotes_;
    bool                 boxSelecting_ = false;
    bool                 eraseDragging_ = false;
    bool                 velocityDragging_ = false;
    int                  velocityDragNote_ = -1;
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
    bool   draggingPlayhead_ = false;
    bool   midiMoodMenuOpen_ = false;
    bool   realFeelEnabled_ = false;
    int    midiMoodMenuHover_ = -1;
    int    midiMoodMenuScrollY_ = 0;
    int    midiMoodMenuTab_ = 0; // 0 = all, 1 = chord progressions
    juce::String currentGeneratedMidiMood_;
    std::map<juce::String, int> midiMoodVariant_;

    void timerCallback() override;
    void eraseNoteAt(int x, int y);
    int getLoopLengthSteps() const;
    
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
    juce::Rectangle<int> getGenerateMidiButtonRect() const;
    juce::Rectangle<int> getRealFeelButtonRect() const;
    juce::Rectangle<int> getStrumButtonRect() const;
    juce::Rectangle<int> getHumanizeButtonRect() const;
    juce::Rectangle<int> getPaste808ButtonRect() const;
    juce::Rectangle<int> getHiHatRollButtonRect() const;
    juce::Rectangle<int> getSnareRollButtonRect() const;
    juce::Rectangle<int> getCurrentMidiStyleButtonRect() const;
    juce::Rectangle<int> getVelocityLaneRect() const;
    juce::Rectangle<int> getVelocityBarRect(const Note& n) const;
    
    int xToStep(int x) const;
    int yToPitch(int y) const;
    int findNoteAt(int x, int y) const;
    int findVelocityBarAt(int x, int y) const;
    void setVelocityFromPoint(int noteIndex, int y);
    juce::Rectangle<int> getGenerateMidiMenuRect() const;
    int getGenerateMidiMenuItemAt(int x, int y) const;
    void drawGenerateMidiMenu(juce::Graphics& g);
    void showGenerateMidiMenu();
    void generateMidiForMood(const juce::String& mood, bool nextVariant = false);
    juce::String getMidiChoiceLabel(const juce::String& mood) const;
    void applyStrumToSelection();
    void humanizeSelection();
    void transposeSelectedNotesByOctaves(int octaveDelta);
    bool shouldShowHiHatRollButton() const;
    bool shouldShowSnareRollButton() const;
    void showDrumRollMenu(bool hiHatRoll);
    void applyDrumRollVariant(bool hiHatRoll, int variantId);
    bool importLowestNotesFromMidi(const juce::File& file);
    
    static bool isBlackKey(int pitch);
    static juce::String pitchName(int pitch);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRoll)
};
