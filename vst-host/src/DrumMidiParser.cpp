#include "DrumMidiParser.h"
#include <cmath>

namespace
{
double tempoAtTick(const juce::MidiMessageSequence& sequence, double tick)
{
    double tempoUs = 500000.0;
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto* ev = sequence.getEventPointer(i);
        const auto& msg = ev->message;
        if (msg.isMetaEvent() && msg.getMetaEventType() == 0x51 && msg.getMetaEventLength() >= 3)
        {
            const auto* d = msg.getMetaEventData();
            tempoUs = (double)((d[0] << 16) | (d[1] << 8) | d[2]);
        }
        if (ev->message.getTimeStamp() > tick)
            break;
    }
    return tempoUs;
}

int roundUpToBars(int steps)
{
    if (steps <= 16) return 16;
    if (steps <= 32) return 32;
    if (steps <= 64) return 64;
    if (steps <= 128) return 128;
    return ((steps + 15) / 16) * 16;
}
} // namespace

juce::File DrumMidiParser::popularSongMidiRoot()
{
    const auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    const juce::File candidates[] = {
        exeDir.getChildFile("popular-song-midi"),
        exeDir.getParentDirectory().getChildFile("popular-song-midi"),
        juce::File::getCurrentWorkingDirectory().getChildFile("vst-host/data/popular-song-midi"),
        juce::File::getCurrentWorkingDirectory().getChildFile("data/popular-song-midi")
    };

    for (const auto& dir : candidates)
        if (dir.isDirectory())
            return dir;

    return exeDir.getChildFile("popular-song-midi");
}

juce::File DrumMidiParser::userPopularSongMidiRoot()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW")
        .getChildFile("popular-song-midi");
    dir.createDirectory();
    return dir;
}

juce::File DrumMidiParser::resolveBundledMidi(const juce::String& relativePath)
{
    if (relativePath.isEmpty())
        return {};

    const auto rel = relativePath.replaceCharacter('\\', '/');
    const auto bundled = popularSongMidiRoot().getChildFile(rel);
    if (bundled.existsAsFile())
        return bundled;

    const auto userCopy = userPopularSongMidiRoot().getChildFile(rel);
    if (userCopy.existsAsFile())
        return userCopy;

    return {};
}

int DrumMidiParser::drumLaneForPitch(int pitch)
{
    switch (pitch)
    {
        case 35: case 36: return 0;
        case 37: case 38: case 39: case 40: return 1;
        case 42: case 44: case 46: return 2;
        default: return 3;
    }
}

DrumMidiParseResult DrumMidiParser::parseFile(const juce::File& file, int maxSteps)
{
    DrumMidiParseResult result;
    if (!file.existsAsFile())
        return result;

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return result;

    juce::MidiFile midi;
    if (!midi.readFrom(stream))
        return result;

    const int ppq = midi.getTimeFormat();
    if (ppq <= 0)
        return result;

    juce::MidiMessageSequence merged;
    for (int t = 0; t < midi.getNumTracks(); ++t)
        if (const auto* track = midi.getTrack(t))
            merged.addSequence(*track, 0.0, 0.0, midi.getLastTimestamp());

    merged.updateMatchedPairs();

    const double ticksPerStep = (double)ppq / 4.0;
    const double tempoUs = tempoAtTick(merged, 0.0);
    result.bpm = juce::jlimit(40, 220, (int)std::llround(60000000.0 / tempoUs));

    for (int i = 0; i < merged.getNumEvents(); ++i)
    {
        const auto* holder = merged.getEventPointer(i);
        const auto& msg = holder->message;
        if (!msg.isNoteOn())
            continue;

        const int pitch = msg.getNoteNumber();
        const int lane = drumLaneForPitch(pitch);
        const int startStep = juce::jmax(0, (int)std::llround(holder->message.getTimeStamp() / ticksPerStep));
        int lengthSteps = 1;

        if (holder->noteOffObject != nullptr)
        {
            const double offTick = holder->noteOffObject->message.getTimeStamp();
            lengthSteps = juce::jmax(1, (int)std::llround((offTick - holder->message.getTimeStamp()) / ticksPerStep));
        }

        if (startStep >= maxSteps)
            continue;

        DrumMidiNote note;
        note.lane = lane;
        note.startStep = startStep;
        note.lengthSteps = lengthSteps;
        note.velocity = msg.getVelocity();
        note.midiPitch = pitch;
        result.notes.push_back(note);

        result.totalSteps = juce::jmax(result.totalSteps, startStep + lengthSteps);
    }

    if (result.notes.empty())
        return result;

    result.totalSteps = juce::jlimit(16, maxSteps, roundUpToBars(result.totalSteps));
    result.ok = true;
    writePreviewGridFromNotes(result.notes, result.previewGrid);
    return result;
}

bool DrumMidiParser::writePreviewGridFromNotes(const std::vector<DrumMidiNote>& notes,
                                               DrumPreviewGrid& grid)
{
    for (auto& row : grid)
        row.fill(0);

    bool any = false;
    for (const auto& note : notes)
    {
        if (note.lane < 0 || note.lane >= 4)
            continue;
        const int step = note.startStep % 16;
        grid[(size_t)note.lane][(size_t)step] = 1;
        any = true;
    }
    return any;
}
