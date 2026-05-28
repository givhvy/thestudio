#pragma once

#include <juce_core/juce_core.h>
#include <vector>

class ChordifyMidiImporter
{
public:
    struct BassNote
    {
        int pitch = 60;
        int startStep = 0;
        int lengthSteps = 1;
        int velocity = 100;
    };

    /** Parse Chordify MIDI — bass roots only. */
    static std::vector<BassNote> import(const juce::File& midiFile,
                                        double bpmHint,
                                        int maxSteps);

    /** All notes from all MIDI tracks (chord voicings + bass). */
    static std::vector<BassNote> importAllTracks(const juce::File& midiFile,
                                                  double bpmHint,
                                                  int maxSteps);
};
