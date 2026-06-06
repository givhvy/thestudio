#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

using DrumPreviewGrid = std::array<std::array<int, 16>, 4>;

struct DrumMidiNote
{
    int lane = 0;
    int startStep = 0;
    int lengthSteps = 1;
    int velocity = 100;
    int midiPitch = 36;
};

struct DrumMidiParseResult
{
    bool ok = false;
    int bpm = 90;
    int totalSteps = 16;
    std::vector<DrumMidiNote> notes;
    DrumPreviewGrid previewGrid {};
};

class DrumMidiParser
{
public:
    static juce::File popularSongMidiRoot();
    static juce::File resolveBundledMidi(const juce::String& relativePath);
    static juce::File userPopularSongMidiRoot();

    static int drumLaneForPitch(int pitch);
    static DrumMidiParseResult parseFile(const juce::File& file, int maxSteps = 512);
    static bool writePreviewGridFromNotes(const std::vector<DrumMidiNote>& notes,
                                          DrumPreviewGrid& grid);
};
