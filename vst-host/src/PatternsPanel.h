#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

class PatternsPanel : public juce::Component
{
public:
    using PatternGrid = std::array<std::array<int, 16>, 4>;

    struct PatternDefinition
    {
        juce::String id;
        juce::String presetId;
        juce::String genre;
        juce::String title;
        juce::String feel;
        int bpm = 90;
        PatternGrid rows {};
        bool useFullPresetRows = false;
        bool artistPattern = false;
        bool popularSongPattern = false;
    };

    PatternsPanel();
    ~PatternsPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;

    std::function<void(const PatternDefinition& pattern)> onApplyPattern;
    std::function<void(const PatternDefinition& pattern, int rowIndex)> onApplyChannelPattern;
    std::function<void()> onClose;

    static std::vector<PatternDefinition> getPatternLibrary();
    static std::vector<PatternDefinition> getPatternsForPreset(const juce::String& presetId);

private:
    struct PatternEntry
    {
        PatternDefinition def;
        juce::Rectangle<int> rect;
    };

    static constexpr int HEADER_H = 32;
    static constexpr int FOOTER_H = 46;
    static constexpr int GENRE_W = 150;
    static constexpr int ROW_H = 56;

    std::vector<PatternEntry> patterns_;
    juce::StringArray genres_;
    juce::StringArray artists_;
    juce::StringArray midiGenres_;
    juce::StringArray popularArtists_;
    juce::String selectedGenre_ = "Boom Bap";
    juce::String selectedArtist_ = "Isaiah Rashad";
    juce::String selectedMidiGenre_ = "Boom Bap";
    juce::String selectedPopularArtist_ = "Drake";
    int selectedMidiLane_ = 0;
    int activeLibraryTab_ = 0; // 0 = Genres, 1 = Artist, 2 = MIDI lanes, 3 = Popular songs
    int selectedPattern_ = 0;
    int listScrollY_ = 0;

    juce::Rectangle<int> closeBtnRect_;
    juce::Rectangle<int> applyBtnRect_;
    juce::Rectangle<int> importMidiBtnRect_;
    juce::Rectangle<int> genresTabRect_;
    juce::Rectangle<int> artistTabRect_;
    juce::Rectangle<int> midiTabRect_;
    juce::Rectangle<int> popularSongsTabRect_;
    juce::Rectangle<int> genreRects_[32];
    juce::Rectangle<int> midiLaneRects_[4];

    juce::ComponentDragger dragger_;
    std::unique_ptr<juce::FileChooser> midiChooser_;
    bool isDraggingPanel_ = false;

    std::vector<int> visiblePatternIndices() const;
    const PatternEntry* selectedPattern() const;
    void drawPreview(juce::Graphics& g, juce::Rectangle<int> area, const PatternEntry& p);
    void importPopularSongMidi();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternsPanel)
};
